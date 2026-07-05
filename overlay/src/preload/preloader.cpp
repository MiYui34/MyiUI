#include "preload/preloader.h"

#include "bridge/native_bridge.h"
#include "jvm/jni_func.h"
#include "jvm/jvm_context.h"
#include "jvm/jvm_log.h"
#include "overlay_runtime.h"
#include "preload/preloader_class.h"

#include <windows.h>

#include <fstream>
#include <string>

namespace myiui::preload {

namespace {

void LogPendingException(JNIEnv* env, const wchar_t* prefix) {
    if (!env->ExceptionCheck()) {
        return;
    }
    jthrowable ex = env->ExceptionOccurred();
    env->ExceptionClear();
    if (!ex) {
        return;
    }
    jclass exClass = env->GetObjectClass(ex);
    jmethodID toString = env->GetMethodID(exClass, "toString", "()Ljava/lang/String;");
    if (!toString) {
        jvm::SpikeLog(prefix);
        return;
    }
    jstring msg = static_cast<jstring>(env->CallObjectMethod(ex, toString));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        jvm::SpikeLog(prefix);
        return;
    }
    const char* utf = env->GetStringUTFChars(msg, nullptr);
    if (utf) {
        wchar_t buf[1024]{};
        MultiByteToWideChar(CP_UTF8, 0, utf, -1, buf, 1024);
        jvm::SpikeLog((std::wstring(prefix) + buf).c_str());
        env->ReleaseStringUTFChars(msg, utf);
    } else {
        jvm::SpikeLog(prefix);
    }
}

std::wstring FindAgentJar() {
    const std::wstring root = myiui::overlay::ResolveProjectRoot();
    if (root.empty()) {
        return {};
    }
    const wchar_t* candidates[] = {
        L"\\agent\\build\\libs\\myiui-agent-1.0.0.jar",
        L"\\agent\\build\\libs\\myiui-agent-1.0.jar",
        L"\\agent\\build\\libs\\myiui-agent.jar",
    };
    for (const wchar_t* rel : candidates) {
        const std::wstring path = root + rel;
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return path;
        }
    }
    return root + candidates[0];
}

}  // namespace

bool DefineAndInit(JNIEnv* env) {
    constexpr const char* kBootstrapBinary = "com.myiui.preload.AgentBootstrap";

    if (preloader_classSizes == 0) {
        jvm::SpikeLog(L"[preloader] preloader_class.h not generated — build agent first");
        return false;
    }

    jclass loaderClass = jvm::jni_func::DefineClass(env, reinterpret_cast<const jbyte*>(preloader_class),
                                                    static_cast<jsize>(preloader_classSizes),
                                                    jvm::GetClassLoader());
    if (!loaderClass) {
        LogPendingException(env, L"[preloader] defineClass failed: ");
        loaderClass = jvm::jni_func::LoadClassViaGameLoader(env, kBootstrapBinary);
        if (!loaderClass) {
            loaderClass = jvm::jni_func::FindClassGlobal("Lcom/myiui/preload/AgentBootstrap;");
        }
        if (!loaderClass) {
            jvm::SpikeLog(L"[preloader] AgentBootstrap defineClass/load failed");
            jvm::SpikeLog(L"[preloader] EXIT Minecraft completely, then inject ONCE on a fresh launch");
            return false;
        }
        jfieldID versionField = env->GetStaticFieldID(loaderClass, "BOOTSTRAP_VERSION", "I");
        if (!versionField) {
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
            jvm::SpikeLog(L"[preloader] outdated AgentBootstrap in JVM — fully EXIT Minecraft and restart");
            return false;
        }
        const jint version = env->GetStaticIntField(loaderClass, versionField);
        if (version < 2) {
            jvm::SpikeLog(L"[preloader] outdated AgentBootstrap in JVM — fully EXIT Minecraft and restart");
            return false;
        }
        jvm::SpikeLog(L"[preloader] reusing AgentBootstrap already in JVM");
    }

    if (myiui::bridge::RegisterPreloaderNatives(env, loaderClass) != JNI_OK) {
        jvm::SpikeLog(L"[preloader] RegisterPreloaderNatives failed");
        return false;
    }

    const std::wstring root = myiui::overlay::ResolveProjectRoot();
    if (root.empty()) {
        jvm::SpikeLog(L"[preloader] project root unresolved — check project_root.txt or folder layout");
        return false;
    }
    jvm::SpikeLog((std::wstring(L"[preloader] project root: ") + root).c_str());

    const std::wstring agentJar = FindAgentJar();
    if (agentJar.empty()) {
        jvm::SpikeLog(L"[preloader] agent jar path missing");
        return false;
    }
    if (GetFileAttributesW(agentJar.c_str()) == INVALID_FILE_ATTRIBUTES) {
        jvm::SpikeLog((std::wstring(L"[preloader] agent jar NOT FOUND: ") + agentJar).c_str());
        return false;
    }
    jvm::SpikeLog((std::wstring(L"[preloader] agent jar: ") + agentJar).c_str());

    jmethodID prepare = env->GetStaticMethodID(loaderClass, "prepareAgent", "(Ljava/lang/String;)V");
    if (!prepare) {
        jvm::SpikeLog(L"[preloader] prepareAgent method missing");
        return false;
    }

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, agentJar.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string jarUtf8(static_cast<size_t>(utf8Len > 0 ? utf8Len - 1 : 0), '\0');
    if (utf8Len > 1) {
        WideCharToMultiByte(CP_UTF8, 0, agentJar.c_str(), -1, jarUtf8.data(), utf8Len, nullptr, nullptr);
    }
    jstring jarPath = env->NewStringUTF(jarUtf8.c_str());
    env->CallStaticVoidMethod(loaderClass, prepare, jarPath);
    if (env->ExceptionCheck()) {
        LogPendingException(env, L"[preloader] prepareAgent threw: ");
        return false;
    }

    jmethodID bridgeClassMethod =
        env->GetStaticMethodID(loaderClass, "nativeBridgeClass", "()Ljava/lang/Class;");
    if (!bridgeClassMethod) {
        jvm::SpikeLog(L"[preloader] nativeBridgeClass method missing");
        return false;
    }
    jobject bridgeClassObj = env->CallStaticObjectMethod(loaderClass, bridgeClassMethod);
    if (env->ExceptionCheck() || !bridgeClassObj) {
        LogPendingException(env, L"[preloader] nativeBridgeClass threw: ");
        return false;
    }
    jclass bridgeClass = static_cast<jclass>(bridgeClassObj);
    if (myiui::bridge::RegisterBridgeNatives(env, bridgeClass) != JNI_OK) {
        jvm::SpikeLog(L"[preloader] RegisterBridgeNatives failed");
        return false;
    }
    jvm::SpikeLog(L"[preloader] NativeBridge natives registered");

    jmethodID startAgent = env->GetStaticMethodID(loaderClass, "startAgent", "()V");
    if (!startAgent) {
        jvm::SpikeLog(L"[preloader] startAgent method missing");
        return false;
    }
    env->CallStaticVoidMethod(loaderClass, startAgent);
    if (env->ExceptionCheck()) {
        LogPendingException(env, L"[preloader] startAgent threw: ");
        return false;
    }

    jvm::SpikeLog(L"[preloader] agent started");
    return true;
}

}  // namespace myiui::preload
