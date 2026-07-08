#pragma once

#include "ipc/shm_reader.h"

namespace myiui::bridge {

// v2 no longer reads an OS shared-memory mapping here; the data comes from
// NativeState pushed by JNI. Keep ShmReader for compatibility and expose a
// clearer name for new render code.
using OverlayStateReader = ::ShmReader;

}  // namespace myiui::bridge
