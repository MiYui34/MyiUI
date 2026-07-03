#pragma once

#include <jni.h>

namespace myiui::inject {

bool InstallGameHook();
bool IsJvmEntryDone();
bool TryRunJvmEntry(JNIEnv* env);

}  // namespace myiui::inject
