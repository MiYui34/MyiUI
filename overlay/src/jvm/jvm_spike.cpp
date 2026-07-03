#include "jvm_spike.h"

#include "jvm_context.h"
#include "jvm_log.h"

#include <windows.h>

#include <jni.h>

namespace myiui::jvm {

namespace {

using JNI_GetCreatedJavaVMs_t = jint(JNICALL*)(JavaVM**, jsize, jsize*);

JNI_GetCreatedJavaVMs_t ResolveGetCreatedJavaVMs() {
    HMODULE jvm = GetModuleHandleW(L"jvm.dll");
    if (!jvm) {
        return nullptr;
    }
    return reinterpret_cast<JNI_GetCreatedJavaVMs_t>(
        GetProcAddress(jvm, "JNI_GetCreatedJavaVMs"));
}

}  // namespace

bool RunJvmSpike() {
    SpikeLog(L"[spike] begin");

    for (int i = 0; i < 120; ++i) {
        auto getVMs = ResolveGetCreatedJavaVMs();
        if (!getVMs) {
            Sleep(500);
            continue;
        }

        JavaVM* vm = nullptr;
        jsize count = 0;
        const jint rc = getVMs(&vm, 1, &count);
        if (rc != JNI_OK || count == 0 || vm == nullptr) {
            SpikeLogf(L"[spike] wait jvm (%d/120) rc=%d count=%d", i + 1, rc, count);
            Sleep(500);
            continue;
        }

        JNIEnv* env = nullptr;
        if (vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK || env == nullptr) {
            SpikeLog(L"[spike] AttachCurrentThread failed");
            return false;
        }

        jclass stringClass = env->FindClass("java/lang/String");
        if (stringClass == nullptr) {
            SpikeLog(L"[spike] FindClass(java/lang/String) failed");
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
            return false;
        }

        SetJvm(vm);
        SpikeLogf(L"[spike] success jvm=%p env=%p jvmti=%p", vm, env, GetJvmti());
        return true;
    }

    SpikeLog(L"[spike] timeout waiting for JVM");
    return false;
}

}  // namespace myiui::jvm
