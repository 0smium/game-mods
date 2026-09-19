#include <Windows.h>
extern "C" __declspec(dllimport) BOOL RegisterCameraFeature(unsigned feature, unsigned abi);
DWORD WINAPI Enable(void*) { RegisterCameraFeature(1, 1); return 0; }
BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        const auto worker = CreateThread(nullptr, 0, Enable, nullptr, 0, nullptr);
        if (worker) CloseHandle(worker);
    }
    return TRUE;
}
