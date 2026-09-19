// Real Windows loader smoke test, deliberately NOT named like the game.
// The runtime must stop at its host guard before installing any hook.
#include <Windows.h>
#include <iostream>
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    HMODULE runtime{};
    for (int i = 1; i < argc; ++i) {
        const auto module = LoadLibraryExA(argv[i], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!module) { std::cerr << "LoadLibrary failed: " << GetLastError() << '\n'; return 3; }
        const auto current = GetModuleHandleW(L"SMTVVCameraRuntime.dll");
        if (!current || (runtime && current != runtime)) return 4;
        runtime = current;
    }
    const auto api = reinterpret_cast<BOOL(*)(unsigned,unsigned)>(GetProcAddress(runtime, "RegisterCameraFeature"));
    if (!api || api(0,1) || api(3,1) || api(1,999)) return 5;
    Sleep(800); // Allow feature registration/host rejection in this test process.
    std::cout << "PASS real feature loading and one shared runtime; invalid feature/ABI rejected\n";
}
