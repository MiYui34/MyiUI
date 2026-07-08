#include "game_hook.h"

#include "jvm/jvm_context.h"
#include "jvm/jvm_log.h"
#include "jvm/jni_func.h"
#include "preload/preloader.h"

#include <MinHook.h>

#include <atomic>
#include <jni.h>
#include <thread>

namespace myiui::inject {

namespace {

using LwjglCallP = void(JNICALL*)(JNIEnv*, jclass, jlong);

LwjglCallP g_originalCallP = nullptr;
LPVOID g_hookTarget = nullptr;
std::atomic<bool> g_callPHookInstalled{false};
std::atomic<bool> g_entryStarted{false};
std::atomic<bool> g_entryDone{false};

// Phase 1 (quick, on render thread): cache JavaVM + game ClassLoader.
// Phase 2 (background thread): define Preloader, load agent.jar, retransform.
bool AcquireJvmContext(JNIEnv* env) {
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK || vm == nullptr) {
        jvm::SpikeLog(L"[hook] GetJavaVM failed");
        return false;
    }
    jvm::SetJvm(vm);

    if (jvm::GetClassLoader() == nullptr) {
        // Fallback chain (robust across MC/Fabric thread-naming changes):
        //   exact "Render thread" -> exact "Client thread" -> any thread whose name contains
        //   "render" -> the current (render) thread's context ClassLoader.
        jobject classLoader = jvm::jni_func::GetClassLoaderFromThreadName(env, "Render thread");
        if (!classLoader) {
            classLoader = jvm::jni_func::GetClassLoaderFromThreadName(env, "Client thread");
        }
        if (!classLoader) {
            classLoader = jvm::jni_func::GetClassLoaderFromThreadNameContains(env, "render");
        }
        if (!classLoader) {
            classLoader = jvm::jni_func::GetClassLoaderFromCurrentThread(env);
        }
        if (!classLoader) {
            jvm::SpikeLog(L"[hook] game ClassLoader not found");
            return false;
        }
        // Promote to global ref so it survives across threads.
        jobject globalRef = env->NewGlobalRef(classLoader);
        jvm::SetClassLoader(globalRef);
        jvm::SpikeLog(L"[hook] game ClassLoader acquired");
    }
    return true;
}

void BackgroundAgentStartup(JavaVM* vm) {
    jvm::SpikeLog(L"[hook] background agent startup begin");

    JNIEnv* env = nullptr;
    if (vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK || !env) {
        jvm::SpikeLog(L"[hook] background AttachCurrentThread failed");
        g_entryDone.store(true, std::memory_order_release);
        return;
    }

    if (!preload::DefineAndInit(env)) {
        jvm::SpikeLog(L"[hook] agent bootstrap failed");
        g_entryDone.store(true, std::memory_order_release);
        vm->DetachCurrentThread();
        return;
    }

    g_entryDone.store(true, std::memory_order_release);
    jvm::SpikeLog(L"[hook] agent bootstrap ok");
    // Stay attached so future JNI calls from this thread work;
    // the JVM will clean up on process exit.
}

void JNICALL Hook_LwjglCallP(JNIEnv* env, jclass clazz, jlong value) {
    g_originalCallP(env, clazz, value);

    if (g_hookTarget) {
        MH_DisableHook(g_hookTarget);
        g_hookTarget = nullptr;
    }

    if (g_entryStarted.exchange(true, std::memory_order_acq_rel)) {
        return;  // already started
    }

    // Phase 1: quick JVM context acquisition on render thread (fast, ~1ms).
    if (!AcquireJvmContext(env)) {
        g_entryStarted.store(false, std::memory_order_release);
        return;
    }

    JavaVM* vm = jvm::GetJvm();
    if (!vm) {
        jvm::SpikeLog(L"[hook] no JavaVM for background startup");
        return;
    }

    // Phase 2: heavy agent startup on background thread (doesn't block render).
    std::thread(BackgroundAgentStartup, vm).detach();
}

}  // namespace

bool IsJvmEntryDone() {
    return g_entryDone.load(std::memory_order_acquire);
}

bool TryRunJvmEntry(JNIEnv* env) {
    if (g_entryDone.load(std::memory_order_acquire)) {
        return true;
    }
    if (!AcquireJvmContext(env)) {
        return false;
    }
    JavaVM* vm = jvm::GetJvm();
    if (!vm) return false;
    if (g_entryStarted.exchange(true, std::memory_order_acq_rel)) {
        return false;  // already started by another path
    }
    std::thread(BackgroundAgentStartup, vm).detach();
    return true;
}

bool InstallGameHook() {
    if (g_callPHookInstalled.load(std::memory_order_acquire)) {
        jvm::SpikeLog(L"[hook] lwjgl callP hook already installed");
        return true;
    }

    const MH_STATUS mhInit = MH_Initialize();
    if (mhInit != MH_OK && mhInit != MH_ERROR_ALREADY_INITIALIZED) {
        jvm::SpikeLog(L"[hook] MinHook init failed");
        return false;
    }

    const MH_STATUS createStatus =
        MH_CreateHookApiEx(L"lwjgl.dll", "Java_org_lwjgl_system_JNI_callP__J", &Hook_LwjglCallP,
                           reinterpret_cast<LPVOID*>(&g_originalCallP), &g_hookTarget);
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
        jvm::SpikeLog(L"[hook] lwjgl callP hook failed, wgl fallback will retry");
        return false;
    }

    const MH_STATUS enableStatus = MH_EnableHook(g_hookTarget);
    if (enableStatus != MH_OK && enableStatus != MH_ERROR_ENABLED) {
        jvm::SpikeLog(L"[hook] enable lwjgl hook failed");
        return false;
    }

    g_callPHookInstalled.store(true, std::memory_order_release);
    jvm::SpikeLog(L"[hook] lwjgl callP hook installed");
    return true;
}

}  // namespace myiui::inject
