#include "native_query.h"

#include "jvm/jvm_context.h"
#include "jvm/jvm_log.h"

#include <jni.h>

namespace myiui::bridge {

namespace {

std::string JStringToUtf8(JNIEnv* env, jstring value) {
    if (!value) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        return {};
    }
    std::string out(chars);
    env->ReleaseStringUTFChars(value, chars);
    return out;
}

QueryResult ParseResponse(const std::string& response) {
    QueryResult result{};
    if (response.rfind("OK ", 0) == 0) {
        result.ok = true;
        result.body = response.substr(3);
        return result;
    }
    if (response == "OK") {
        result.ok = true;
        return result;
    }
    if (response.rfind("ERR", 0) == 0) {
        result.error = response.size() > 4 ? response.substr(4) : response;
        return result;
    }
    result.error = response.empty() ? "empty" : response;
    return result;
}

// FindClass from a native-attached thread uses the bootstrap classloader,
// which cannot see user classes. Use the cached game ClassLoader instead.
jclass FindClassViaGameLoader(JNIEnv* env, const char* binaryName) {
    jobject loader = jvm::GetClassLoader();
    if (!loader) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return env->FindClass(binaryName);
    }
    jclass clClass = env->FindClass("java/lang/ClassLoader");
    if (!clClass || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return env->FindClass(binaryName);
    }
    jmethodID loadClass = env->GetMethodID(clClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!loadClass) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return env->FindClass(binaryName);
    }
    jstring jname = env->NewStringUTF(binaryName);
    jobject cls = env->CallObjectMethod(loader, loadClass, jname);
    env->DeleteLocalRef(jname);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        jvm::SpikeLog(L"[native_query] loadClass failed for class — clearing exception");
        return nullptr;
    }
    return static_cast<jclass>(cls);
}

// Cached class refs + method IDs for hot path.
jclass g_gameDataBridge = nullptr;
jmethodID g_dispatchQuery = nullptr;
jmethodID g_dispatchAction = nullptr;
jclass g_gameActions = nullptr;
jclass g_sharedState = nullptr;
jmethodID g_setOverlayAck = nullptr;

void EnsureCachedClasses(JNIEnv* env) {
    if (g_gameDataBridge && g_dispatchQuery && g_dispatchAction) {
        return;
    }
    if (env->ExceptionCheck()) env->ExceptionClear();

    jclass bridge = FindClassViaGameLoader(env, "com.myiui.agent.GameDataBridge");
    if (!bridge) {
        jvm::SpikeLog(L"[native_query] GameDataBridge not found via game loader");
        return;
    }
    g_gameDataBridge = static_cast<jclass>(env->NewGlobalRef(bridge));
    env->DeleteLocalRef(bridge);
    g_dispatchQuery = env->GetStaticMethodID(g_gameDataBridge, "dispatchQuery",
                                              "(Ljava/lang/String;)Ljava/lang/String;");
    g_dispatchAction = env->GetStaticMethodID(g_gameDataBridge, "dispatchAction",
                                               "(Ljava/lang/String;)Ljava/lang/String;");
    if (env->ExceptionCheck()) env->ExceptionClear();

    jclass actions = FindClassViaGameLoader(env, "com.myiui.agent.GameActions");
    if (actions) {
        g_gameActions = static_cast<jclass>(env->NewGlobalRef(actions));
        env->DeleteLocalRef(actions);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();

    jclass shared = FindClassViaGameLoader(env, "com.myiui.agent.SharedState");
    if (shared) {
        g_sharedState = static_cast<jclass>(env->NewGlobalRef(shared));
        env->DeleteLocalRef(shared);
        g_setOverlayAck = env->GetStaticMethodID(g_sharedState, "setOverlayAck", "(Z)V");
    }
    if (env->ExceptionCheck()) env->ExceptionClear();

    jvm::SpikeLog(L"[native_query] classes cached");
}

}  // namespace

QueryResult QueryJava(const std::string& command, int /*timeoutMs*/) {
    QueryResult result{};
    JNIEnv* env = jvm::AttachEnv();
    if (!env) {
        result.error = "no JNIEnv";
        return result;
    }

    EnsureCachedClasses(env);
    if (!g_dispatchQuery) {
        result.error = "dispatchQuery not resolved";
        return result;
    }

    jstring jcmd = env->NewStringUTF(command.c_str());
    jstring response = static_cast<jstring>(
        env->CallStaticObjectMethod(g_gameDataBridge, g_dispatchQuery, jcmd));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        result.error = "dispatchQuery exception";
        return result;
    }
    if (!response) {
        result.error = "dispatchQuery null";
        return result;
    }
    return ParseResponse(JStringToUtf8(env, response));
}

bool ActionJava(const std::string& command) {
    JNIEnv* env = jvm::AttachEnv();
    if (!env) {
        return false;
    }

    EnsureCachedClasses(env);
    const bool useQuery = command.rfind("NE_", 0) == 0;
    jmethodID dispatch = useQuery ? g_dispatchQuery : g_dispatchAction;
    if (!dispatch) {
        return false;
    }

    jstring jcmd = env->NewStringUTF(command.c_str());
    jstring response = static_cast<jstring>(
        env->CallStaticObjectMethod(g_gameDataBridge, dispatch, jcmd));
    env->DeleteLocalRef(jcmd);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    if (!response) {
        return false;
    }
    const std::string text = JStringToUtf8(env, response);
    env->DeleteLocalRef(response);
    return text == "OK" || text.rfind("OK ", 0) == 0;
}

void ActionJavaAsync(const std::string& command) {
    JNIEnv* env = jvm::AttachEnv();
    if (!env) {
        return;
    }

    EnsureCachedClasses(env);

    // 网易云等 NE_* 命令走 dispatchQuery（与菜单 GET_* 查询同路径）
    if (command.rfind("NE_", 0) == 0) {
        if (g_dispatchQuery) {
            jstring jcmd = env->NewStringUTF(command.c_str());
            env->CallStaticObjectMethod(g_gameDataBridge, g_dispatchQuery, jcmd);
            env->DeleteLocalRef(jcmd);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    if (!g_gameActions) {
        if (g_dispatchAction) {
            jstring jcmd = env->NewStringUTF(command.c_str());
            env->CallStaticObjectMethod(g_gameDataBridge, g_dispatchAction, jcmd);
            env->DeleteLocalRef(jcmd);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    if (command == "OPEN_SINGLEPLAYER") {
        env->CallStaticVoidMethod(g_gameActions, env->GetStaticMethodID(g_gameActions, "openSingleplayer", "()V"));
    } else if (command == "OPEN_MULTIPLAYER") {
        env->CallStaticVoidMethod(g_gameActions, env->GetStaticMethodID(g_gameActions, "openMultiplayer", "()V"));
    } else if (command == "OPEN_OPTIONS") {
        env->CallStaticVoidMethod(g_gameActions, env->GetStaticMethodID(g_gameActions, "openOptions", "()V"));
    } else if (command == "OPEN_VIDEO_OPTIONS") {
        env->CallStaticVoidMethod(g_gameActions, env->GetStaticMethodID(g_gameActions, "openVideoOptions", "()V"));
    } else if (command == "QUIT") {
        env->CallStaticVoidMethod(g_gameActions, env->GetStaticMethodID(g_gameActions, "quit", "()Z"));
    } else if (command.rfind("SET_BG_VIDEO:", 0) == 0) {
        jstring path = env->NewStringUTF(command.substr(13).c_str());
        env->CallStaticVoidMethod(g_gameActions,
                                  env->GetStaticMethodID(g_gameActions, "setBackgroundVideo", "(Ljava/lang/String;)Z"),
                                  path);
    } else if (command == "RELOAD_BG") {
        env->CallStaticVoidMethod(g_gameActions, env->GetStaticMethodID(g_gameActions, "reloadBackground", "()V"));
    } else if (command == "OVERLAY_READY") {
        if (g_sharedState && g_setOverlayAck) {
            env->CallStaticVoidMethod(g_sharedState, g_setOverlayAck, JNI_TRUE);
        }
    } else if (command == "OVERLAY_SUSPEND") {
        if (g_sharedState && g_setOverlayAck) {
            env->CallStaticVoidMethod(g_sharedState, g_setOverlayAck, JNI_FALSE);
        }
    } else {
        jstring jcmd = env->NewStringUTF(command.c_str());
        if (g_dispatchAction) {
            env->CallStaticObjectMethod(g_gameDataBridge, g_dispatchAction, jcmd);
        }
        env->DeleteLocalRef(jcmd);
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

}  // namespace myiui::bridge
