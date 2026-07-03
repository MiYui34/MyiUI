#pragma once

#include <jni.h>
#include <jvmti.h>

namespace myiui::jvm {

void SetJvm(JavaVM* vm);
void SetClassLoader(jobject classLoader);
jobject GetClassLoader();
bool IsReady();
JavaVM* GetJvm();
jvmtiEnv* GetJvmti();
JNIEnv* AttachEnv();

}  // namespace myiui::jvm
