#include "toolkit.h"
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        ctl::Start(hModule);
    } else if (reason == DLL_PROCESS_DETACH) {
        ctl::Stop();
    }
    return TRUE;
}
