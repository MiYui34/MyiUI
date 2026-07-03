#pragma once

#include <jni.h>

namespace myiui::jvm::jni_func {

jobject GetClassLoaderFromThreadName(JNIEnv* env, const char* threadName);
jclass DefineClass(JNIEnv* env, const jbyte* bytes, jsize len, jobject classLoader);
jclass DefineClassFromArray(JNIEnv* env, jbyteArray classBytes, jobject classLoader);
jclass FindClassGlobal(const char* signature);

}  // namespace myiui::jvm::jni_func
