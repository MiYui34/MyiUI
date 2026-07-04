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
    // Probe JNI versions from newest to oldest so the overlay is not pinned to Java 21.
    // GetEnv returns JNI_EDETACHED (version-independent) when the thread needs attaching,
    // JNI_EVERSION when the requested version is unsupported, or JNI_OK otherwise.
    static const jint kJniVersions[] = {
        JNI_VERSION_21,
        0x00140000,  // JNI_VERSION_20
        0x00130000,  // JNI_VERSION_19
        0x000a0000,  // JNI_VERSION_10
        JNI_VERSION_9,
        JNI_VERSION_1_8,
        JNI_VERSION_1_6,
    };
    for (jint version : kJniVersions) {
        const jint rc = g_vm->GetEnv(reinterpret_cast<void**>(&env), version);
        if (rc == JNI_OK) {
            return env;
        }
        if (rc == JNI_EDETACHED) {
            if (g_vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
                return nullptr;
            }
            return env;
        }
        // rc == JNI_EVERSION: fall through and try an older JNI version.
    }
    return nullptr;
}

}  // namespace myiui::jvm
