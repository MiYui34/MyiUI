#include <windows.h>

#include "hooks.h"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HooksInit(module);
    }
    // Do not tear down WebView2/COM from DllMain (loader lock). Process exit reclaims.
    return TRUE;
}
