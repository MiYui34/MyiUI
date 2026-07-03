#pragma once

#include <jni.h>

namespace myiui::bridge {

jint RegisterBridgeNatives(JNIEnv* env, jclass nativeBridgeClass);
jint RegisterPreloaderNatives(JNIEnv* env, jclass preloaderClass);

}  // namespace myiui::bridge
