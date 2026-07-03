#include "bridge/native_bridge.h"
#include "native_state.h"

#include "jvm/jvm_context.h"
#include "jvm/jvm_log.h"

#include <windows.h>

#include <cstring>
#include <vector>

namespace myiui::bridge {

namespace {

jclass g_targetClazz = nullptr;
unsigned char* g_classBytes = nullptr;
int g_classBytesLen = 0;

void* JvmtiAllocate(jlong size) {
    unsigned char* buffer = nullptr;
    jvm::GetJvmti()->Allocate(size, &buffer);
    return buffer;
}

void JNICALL ClassFileLoadHook(jvmtiEnv* /*jvmti_env*/, JNIEnv* env, jclass class_being_redefined,
                               jobject /*loader*/, const char* name, jobject /*protection_domain*/,
                               jint class_data_len, const unsigned char* class_data, jint* /*new_class_data_len*/,
                               unsigned char** /*new_class_data*/) {
    if (!g_targetClazz || !class_being_redefined || !class_data || class_data_len <= 0 || g_classBytes) {
        return;
    }
    if (!env || !env->IsSameObject(class_being_redefined, g_targetClazz)) {
        return;
    }
    jvmtiEnv* jvmti = jvm::GetJvmti();
    unsigned char* copy = nullptr;
    if (!jvmti || jvmti->Allocate(class_data_len, &copy) != JVMTI_ERROR_NONE || !copy) {
        return;
    }
    std::memcpy(copy, class_data, static_cast<size_t>(class_data_len));
    g_classBytes = copy;
    g_classBytesLen = class_data_len;
    if (name) {
        char buf[256]{};
        std::snprintf(buf, sizeof(buf), "[hook] ClassFileLoadHook captured %s (%d bytes)", name, class_data_len);
        wchar_t wbuf[512]{};
        MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, 512);
        jvm::SpikeLog(wbuf);
    }
}

jboolean JNICALL Java_com_myiui_agent_NativeBridge_startup(JNIEnv* /*env*/, jclass /*clazz*/) {
    jvmtiCapabilities caps{};
    caps.can_retransform_classes = 1;
    caps.can_retransform_any_class = 1;
    caps.can_redefine_any_class = 1;
    caps.can_redefine_classes = 1;
    caps.can_generate_all_class_hook_events = 1;
    if (jvm::GetJvmti()->AddCapabilities(&caps) != JVMTI_ERROR_NONE) {
        return JNI_FALSE;
    }

    jvmtiEventCallbacks callbacks{};
    callbacks.ClassFileLoadHook = &ClassFileLoadHook;
    jvm::GetJvmti()->SetEventCallbacks(&callbacks, sizeof(callbacks));
    jvm::GetJvmti()->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
    return JNI_TRUE;
}

jint JNICALL Java_com_myiui_agent_NativeBridge_redefineClasses(JNIEnv* env, jclass /*clazz*/, jclass targetClass,
                                                               jbyteArray newClassBytes) {
    const jsize length = env->GetArrayLength(newClassBytes);
    jbyte* bytes = env->GetByteArrayElements(newClassBytes, nullptr);
    jvmtiClassDefinition def{};
    def.klass = targetClass;
    def.class_byte_count = length;
    def.class_bytes = reinterpret_cast<unsigned char*>(bytes);
    const jvmtiError err = jvm::GetJvmti()->RedefineClasses(1, &def);
    env->ReleaseByteArrayElements(newClassBytes, bytes, JNI_ABORT);
    return static_cast<jint>(err);
}

jbyteArray JNICALL Java_com_myiui_agent_NativeBridge_getClassBytes(JNIEnv* env, jclass /*clazz*/, jclass targetClass) {
    g_targetClazz = targetClass;
    g_classBytes = nullptr;
    g_classBytesLen = 0;
    jclass* classes = static_cast<jclass*>(JvmtiAllocate(sizeof(jclass)));
    if (!classes) {
        jvm::SpikeLog(L"[hook] JvmtiAllocate failed in getClassBytes");
        return nullptr;
    }
    classes[0] = targetClass;
    const jvmtiError err = jvm::GetJvmti()->RetransformClasses(1, classes);
    jvm::GetJvmti()->Deallocate(reinterpret_cast<unsigned char*>(classes));

    if (err != JVMTI_ERROR_NONE) {
        wchar_t buf[128]{};
        swprintf_s(buf, L"[hook] RetransformClasses err=%d", static_cast<int>(err));
        jvm::SpikeLog(buf);
    }

    if (!g_classBytes || g_classBytesLen <= 0) {
        jvm::SpikeLog(L"[hook] getClassBytes: no bytes captured");
        g_targetClazz = nullptr;
        return nullptr;
    }

    jbyteArray output = env->NewByteArray(g_classBytesLen);
    if (output) {
        env->SetByteArrayRegion(output, 0, g_classBytesLen, reinterpret_cast<const jbyte*>(g_classBytes));
    }
    if (g_classBytes) {
        jvm::GetJvmti()->Deallocate(g_classBytes);
        g_classBytes = nullptr;
        g_classBytesLen = 0;
    }
    g_targetClazz = nullptr;
    return output;
}

void CopyStringField(JNIEnv* env, jstring src, char* dst, size_t dstLen) {
    if (!src || !dst || dstLen == 0) {
        return;
    }
    const char* utf = env->GetStringUTFChars(src, nullptr);
    if (!utf) {
        return;
    }
    std::strncpy(dst, utf, dstLen - 1);
    dst[dstLen - 1] = '\0';
    env->ReleaseStringUTFChars(src, utf);
}

void JNICALL Java_com_myiui_agent_NativeBridge_pushScreenState(JNIEnv* /*env*/, jclass /*clazz*/, jbyte kind,
                                                               jint seq, jboolean overlayActive, jboolean islandActive,
                                                               jboolean overlayAck) {
    NativeState::Instance().PushScreen(static_cast<uint8_t>(kind), static_cast<uint32_t>(seq), overlayActive == JNI_TRUE,
                                       islandActive == JNI_TRUE, overlayAck == JNI_TRUE);
}

void JNICALL Java_com_myiui_agent_NativeBridge_pushIslandState(
    JNIEnv* env, jclass /*clazz*/, jbyte valid, jbyte mode, jbyte activeSlot, jbyte notifyCount, jshort islandSeq,
    jshort fps, jstring title, jstring subtitle, jstring lyrics, jbyteArray slots, jint notifyExpireMs) {
    myiui::shared::IslandState state{};
    state.valid = static_cast<uint8_t>(valid);
    state.mode = static_cast<uint8_t>(mode);
    state.active_slot = static_cast<uint8_t>(activeSlot);
    state.notify_count = static_cast<uint8_t>(notifyCount);
    state.island_seq = static_cast<uint16_t>(islandSeq);
    state.fps = fps;
    state.notify_expire_ms = static_cast<uint32_t>(notifyExpireMs);
    CopyStringField(env, title, state.title, sizeof(state.title));
    CopyStringField(env, subtitle, state.subtitle, sizeof(state.subtitle));
    CopyStringField(env, lyrics, state.lyrics_line, sizeof(state.lyrics_line));
    if (slots) {
        const jsize len = env->GetArrayLength(slots);
        jbyte* bytes = env->GetByteArrayElements(slots, nullptr);
        if (bytes) {
            const jsize copy = len < static_cast<jsize>(sizeof(state.slots)) ? len : static_cast<jsize>(sizeof(state.slots));
            std::memcpy(state.slots, bytes, static_cast<size_t>(copy));
            env->ReleaseByteArrayElements(slots, bytes, JNI_ABORT);
        }
    }
    NativeState::Instance().PushIsland(state);
}

void JNICALL Java_com_myiui_agent_NativeBridge_pushHudState(JNIEnv* env, jclass /*clazz*/, jbyteArray packed) {
    if (!packed) {
        return;
    }
    const jsize len = env->GetArrayLength(packed);
    if (len < static_cast<jsize>(sizeof(myiui::shared::HudState))) {
        return;
    }
    myiui::shared::HudState state{};
    env->GetByteArrayRegion(packed, 0, static_cast<jsize>(sizeof(state)), reinterpret_cast<jbyte*>(&state));
    NativeState::Instance().PushHud(state);
}

void JNICALL Java_com_myiui_agent_NativeBridge_pushTabListState(JNIEnv* env, jclass /*clazz*/, jbyteArray packed) {
    if (!packed) {
        return;
    }
    const jsize len = env->GetArrayLength(packed);
    if (len < static_cast<jsize>(sizeof(myiui::shared::TabListState))) {
        return;
    }
    myiui::shared::TabListState state{};
    env->GetByteArrayRegion(packed, 0, static_cast<jsize>(sizeof(state)), reinterpret_cast<jbyte*>(&state));
    NativeState::Instance().PushTabList(state);
}

void JNICALL Java_com_myiui_agent_NativeBridge_pushVideoFrame(JNIEnv* env, jclass /*clazz*/, jbyteArray rgba, jint width,
                                                              jint height, jint frameIndex) {
    if (!rgba || width <= 0 || height <= 0) {
        return;
    }
    const jsize len = env->GetArrayLength(rgba);
    jbyte* bytes = env->GetByteArrayElements(rgba, nullptr);
    if (!bytes) {
        return;
    }
  NativeState::Instance().PushVideoFrame(reinterpret_cast<const uint8_t*>(bytes), width, height, frameIndex);
    env->ReleaseByteArrayElements(rgba, bytes, JNI_ABORT);
}

void JNICALL Java_com_myiui_preload_Preloader_log(JNIEnv* env, jclass /*clazz*/, jstring message) {
    const char* utf = env->GetStringUTFChars(message, nullptr);
    if (!utf) {
        return;
    }
    wchar_t buf[512]{};
    MultiByteToWideChar(CP_UTF8, 0, utf, -1, buf, 512);
    env->ReleaseStringUTFChars(message, utf);
    jvm::SpikeLog(buf);
}

jobject JNICALL Java_com_myiui_preload_Preloader_getClassLoader(JNIEnv* /*env*/, jclass /*clazz*/) {
    return jvm::GetClassLoader();
}

jobjectArray JNICALL Java_com_myiui_agent_NativeBridge_jvmLoadedClasses(JNIEnv* env, jclass /*clazz*/) {
    jvmtiEnv* jvmti = jvm::GetJvmti();
    if (!jvmti) {
        return nullptr;
    }
    jint count = 0;
    jclass* loaded = nullptr;
    if (jvmti->GetLoadedClasses(&count, &loaded) != JVMTI_ERROR_NONE || count <= 0 || !loaded) {
        return env->NewObjectArray(0, env->FindClass("java/lang/Class"), nullptr);
    }
    jclass classClass = env->FindClass("java/lang/Class");
    jobjectArray result = env->NewObjectArray(count, classClass, nullptr);
    for (jint i = 0; i < count; ++i) {
        env->SetObjectArrayElement(result, i, loaded[i]);
    }
    jvmti->Deallocate(reinterpret_cast<unsigned char*>(loaded));
    return result;
}

}  // namespace

jint RegisterBridgeNatives(JNIEnv* env, jclass nativeBridgeClass) {
    static JNINativeMethod bridgeMethods[] = {
        {const_cast<char*>("startup"), const_cast<char*>("()Z"),
         reinterpret_cast<void*>(Java_com_myiui_agent_NativeBridge_startup)},
        {const_cast<char*>("redefineClasses"), const_cast<char*>("(Ljava/lang/Class;[B)I"),
         reinterpret_cast<void*>(Java_com_myiui_agent_NativeBridge_redefineClasses)},
        {const_cast<char*>("getClassBytes"), const_cast<char*>("(Ljava/lang/Class;)[B"),
         reinterpret_cast<void*>(Java_com_myiui_agent_NativeBridge_getClassBytes)},
        {const_cast<char*>("jvmLoadedClasses"), const_cast<char*>("()[Ljava/lang/Class;"),
         reinterpret_cast<void*>(Java_com_myiui_agent_NativeBridge_jvmLoadedClasses)},
        {const_cast<char*>("pushScreenState"), const_cast<char*>("(BIZZZ)V"),
         reinterpret_cast<void*>(Java_com_myiui_agent_NativeBridge_pushScreenState)},
        {const_cast<char*>("pushIslandState"),
         const_cast<char*>("(BBBBSSLjava/lang/String;Ljava/lang/String;Ljava/lang/String;[BI)V"),
         reinterpret_cast<void*>(Java_com_myiui_agent_NativeBridge_pushIslandState)},
        {const_cast<char*>("pushHudState"), const_cast<char*>("([B)V"),
         reinterpret_cast<void*>(Java_com_myiui_agent_NativeBridge_pushHudState)},
        {const_cast<char*>("pushTabListState"), const_cast<char*>("([B)V"),
         reinterpret_cast<void*>(Java_com_myiui_agent_NativeBridge_pushTabListState)},
        {const_cast<char*>("pushVideoFrame"), const_cast<char*>("([BIII)V"),
         reinterpret_cast<void*>(Java_com_myiui_agent_NativeBridge_pushVideoFrame)},
    };
    return env->RegisterNatives(nativeBridgeClass, bridgeMethods,
                                static_cast<jint>(sizeof(bridgeMethods) / sizeof(bridgeMethods[0])));
}

jint RegisterPreloaderNatives(JNIEnv* env, jclass preloaderClass) {
    static JNINativeMethod preloaderMethods[] = {
        {const_cast<char*>("log"), const_cast<char*>("(Ljava/lang/String;)V"),
         reinterpret_cast<void*>(Java_com_myiui_preload_Preloader_log)},
        {const_cast<char*>("getClassLoader"), const_cast<char*>("()Ljava/lang/ClassLoader;"),
         reinterpret_cast<void*>(Java_com_myiui_preload_Preloader_getClassLoader)},
    };
    return env->RegisterNatives(preloaderClass, preloaderMethods,
                                static_cast<jint>(sizeof(preloaderMethods) / sizeof(preloaderMethods[0])));
}

}  // namespace myiui::bridge
