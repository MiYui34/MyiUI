#pragma once

#include <jni.h>

namespace myiui::jvm::jni_func {

jobject GetClassLoaderFromThreadName(JNIEnv* env, const char* threadName);
jobject GetClassLoaderFromThreadNameContains(JNIEnv* env, const char* substring);
jobject GetClassLoaderFromCurrentThread(JNIEnv* env);
jclass DefineClass(JNIEnv* env, const jbyte* bytes, jsize len, jobject classLoader);
jclass DefineClassFromArray(JNIEnv* env, jbyteArray classBytes, jobject classLoader);
jclass LoadClassViaLoader(JNIEnv* env, jobject loader, const char* binaryName);
jclass LoadClassViaGameLoader(JNIEnv* env, const char* binaryName);
jclass FindClassGlobal(const char* signature);

}  // namespace myiui::jvm::jni_func
