#include "jni_func.h"

#include "jvm_context.h"

#include <cstring>

namespace myiui::jvm::jni_func {

jobject GetClassLoaderFromThreadName(JNIEnv* env, const char* threadName) {
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
        if (nameStr && std::strcmp(nameStr, threadName) == 0) {
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
