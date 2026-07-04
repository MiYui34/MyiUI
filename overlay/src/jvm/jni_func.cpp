#include "jni_func.h"

#include "jvm_context.h"

#include <cctype>
#include <cstring>

namespace myiui::jvm::jni_func {

namespace {

bool ContainsCaseInsensitive(const char* haystack, const char* needle) {
    if (!haystack || !needle) {
        return false;
    }
    const size_t nlen = std::strlen(needle);
    if (nlen == 0) {
        return true;
    }
    for (const char* p = haystack; *p; ++p) {
        size_t i = 0;
        while (i < nlen && p[i] &&
               std::tolower(static_cast<unsigned char>(p[i])) ==
                   std::tolower(static_cast<unsigned char>(needle[i]))) {
            ++i;
        }
        if (i == nlen) {
            return true;
        }
    }
    return false;
}

// Enumerate live threads and return the context ClassLoader of the first whose name matches.
// When substring==true the match is a case-insensitive contains, else an exact compare.
jobject FindContextClassLoader(JNIEnv* env, const char* target, bool substring) {
    jclass threadClass = env->FindClass("java/lang/Thread");
    if (!threadClass) {
        return nullptr;
    }
    jmethodID getAllStackTraces =
        env->GetStaticMethodID(threadClass, "getAllStackTraces", "()Ljava/util/Map;");
    if (!getAllStackTraces) {
        return nullptr;
    }
    jobject stackTraceMap = env->CallStaticObjectMethod(threadClass, getAllStackTraces);
    if (!stackTraceMap) {
        return nullptr;
    }

    jclass mapClass = env->FindClass("java/util/Map");
    jmethodID keySet = env->GetMethodID(mapClass, "keySet", "()Ljava/util/Set;");
    jobject keySetObj = env->CallObjectMethod(stackTraceMap, keySet);

    jclass setClass = env->FindClass("java/util/Set");
    jmethodID iterator = env->GetMethodID(setClass, "iterator", "()Ljava/util/Iterator;");
    jobject iter = env->CallObjectMethod(keySetObj, iterator);

    jclass iteratorClass = env->FindClass("java/util/Iterator");
    jmethodID hasNext = env->GetMethodID(iteratorClass, "hasNext", "()Z");
    jmethodID next = env->GetMethodID(iteratorClass, "next", "()Ljava/lang/Object;");

    jobject contextClassLoader = nullptr;
    while (env->CallBooleanMethod(iter, hasNext) == JNI_TRUE) {
        jobject thread = env->CallObjectMethod(iter, next);
        jclass threadObjClass = env->GetObjectClass(thread);
        jmethodID getName = env->GetMethodID(threadObjClass, "getName", "()Ljava/lang/String;");
        jstring name = static_cast<jstring>(env->CallObjectMethod(thread, getName));
        const char* nameStr = env->GetStringUTFChars(name, nullptr);
        const bool matched = nameStr &&
            (substring ? ContainsCaseInsensitive(nameStr, target) : std::strcmp(nameStr, target) == 0);
        if (matched) {
            jmethodID getContextClassLoader =
                env->GetMethodID(threadObjClass, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
            contextClassLoader = env->CallObjectMethod(thread, getContextClassLoader);
        }
        if (nameStr) {
            env->ReleaseStringUTFChars(name, nameStr);
        }
        if (contextClassLoader != nullptr) {
            break;
        }
    }
    return contextClassLoader;
}

}  // namespace

jobject GetClassLoaderFromThreadName(JNIEnv* env, const char* threadName) {
    return FindContextClassLoader(env, threadName, false);
}

jobject GetClassLoaderFromThreadNameContains(JNIEnv* env, const char* substring) {
    return FindContextClassLoader(env, substring, true);
}

jobject GetClassLoaderFromCurrentThread(JNIEnv* env) {
    // The LWJGL callP hook runs on the render thread, so the current thread's context
    // ClassLoader is the game (Knot) loader — a version-independent last-resort fallback.
    jclass threadClass = env->FindClass("java/lang/Thread");
    if (!threadClass) {
        return nullptr;
    }
    jmethodID currentThread =
        env->GetStaticMethodID(threadClass, "currentThread", "()Ljava/lang/Thread;");
    if (!currentThread) {
        return nullptr;
    }
    jobject thread = env->CallStaticObjectMethod(threadClass, currentThread);
    if (!thread) {
        return nullptr;
    }
    jmethodID getContextClassLoader =
        env->GetMethodID(threadClass, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    if (!getContextClassLoader) {
        return nullptr;
    }
    return env->CallObjectMethod(thread, getContextClassLoader);
}

jclass DefineClass(JNIEnv* env, const jbyte* bytes, jsize len, jobject classLoader) {
    jclass clClass = env->FindClass("java/lang/ClassLoader");
    if (!clClass) {
        return nullptr;
    }
    jmethodID defineClass =
        env->GetMethodID(clClass, "defineClass", "([BII)Ljava/lang/Class;");
    if (!defineClass) {
        return nullptr;
    }
    jbyteArray arr = env->NewByteArray(len);
    if (!arr) {
        return nullptr;
    }
    env->SetByteArrayRegion(arr, 0, len, bytes);
    jclass clazz = static_cast<jclass>(env->CallObjectMethod(classLoader, defineClass, arr, 0, len));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    return clazz;
}

jclass DefineClassFromArray(JNIEnv* env, jbyteArray classBytes, jobject classLoader) {
    const jsize len = env->GetArrayLength(classBytes);
    jbyte* bytes = env->GetByteArrayElements(classBytes, nullptr);
    if (!bytes) {
        return nullptr;
    }
    jclass clazz = DefineClass(env, bytes, len, classLoader);
    env->ReleaseByteArrayElements(classBytes, bytes, JNI_ABORT);
    return clazz;
}

jclass FindClassGlobal(const char* signature) {
    jvmtiEnv* jvmti = GetJvmti();
    if (jvmti == nullptr) {
        return nullptr;
    }
    jint count = 0;
    jclass* loaded = nullptr;
    if (jvmti->GetLoadedClasses(&count, &loaded) != JVMTI_ERROR_NONE) {
        return nullptr;
    }
    jclass found = nullptr;
    for (jint i = 0; i < count; ++i) {
        char* sig = nullptr;
        if (jvmti->GetClassSignature(loaded[i], &sig, nullptr) == JVMTI_ERROR_NONE && sig != nullptr) {
            if (std::strcmp(sig, signature) == 0) {
                found = loaded[i];
            }
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(sig));
        }
        if (found != nullptr) {
            break;
        }
    }
    jvmti->Deallocate(reinterpret_cast<unsigned char*>(loaded));
    return found;
}

}  // namespace myiui::jvm::jni_func
