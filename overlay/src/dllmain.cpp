#include <windows.h>

#include "hooks.h"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HooksInit(module);
    }
    return TRUE;
}
