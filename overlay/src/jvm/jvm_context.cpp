#include "jvm_context.h"

#include "jvm_log.h"

namespace myiui::jvm {

namespace {

JavaVM* g_vm = nullptr;
jvmtiEnv* g_jvmti = nullptr;
jobject g_classloader = nullptr;

}  // namespace

void SetJvm(JavaVM* vm) {
    if (g_vm != nullptr || vm == nullptr) {
        return;
    }
    g_vm = vm;
    if (vm->GetEnv(reinterpret_cast<void**>(&g_jvmti), JVMTI_VERSION_1_2) != JNI_OK) {
        g_jvmti = nullptr;
        SpikeLog(L"JVMTI GetEnv failed.");
    }
}

void SetClassLoader(jobject classLoader) {
    g_classloader = classLoader;
}

jobject GetClassLoader() {
    return g_classloader;
}

bool IsReady() {
    return g_vm != nullptr && g_jvmti != nullptr;
}

JavaVM* GetJvm() {
    return g_vm;
}

jvmtiEnv* GetJvmti() {
    return g_jvmti;
}

JNIEnv* AttachEnv() {
    if (g_vm == nullptr) {
        return nullptr;
    }
    JNIEnv* env = nullptr;
    const jint rc = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_21);
    if (rc == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
            return nullptr;
        }
        return env;
    }
    if (rc != JNI_OK) {
        return nullptr;
    }
    return env;
}

}  // namespace myiui::jvm
