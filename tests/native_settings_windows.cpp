#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <share.h>
#include <iostream>
std::array<bool,256> keys{};
bool focused = true;
ULONGLONG clockMs = 1000;
std::wstring fakeModule;
short MockKey(int key) { return keys.at(key) ? static_cast<short>(0x8000) : 0; }
HWND MockForeground() { return reinterpret_cast<HWND>(1); }
DWORD MockWindowPid(HWND, DWORD* pid) { *pid = focused ? 123 : 456; return 1; }
DWORD MockPid() { return 123; }
ULONGLONG MockTime() { return clockMs; }
DWORD MockModule(HMODULE, wchar_t* path, DWORD capacity) {
    if (fakeModule.size() >= capacity) return 0;
    std::wcscpy(path, fakeModule.c_str()); return static_cast<DWORD>(fakeModule.size());
}
#define GetAsyncKeyState MockKey
#define GetForegroundWindow MockForeground
#define GetWindowThreadProcessId MockWindowPid
#define GetCurrentProcessId MockPid
#define GetTickCount64 MockTime
#define GetModuleFileNameW MockModule
std::atomic<bool> requested{true};
std::atomic<unsigned> snapshotRequest{0};
void Log(const char*, ...) {}
#include "../mods/smtvv/common/camera_settings.inl"
int main() {
    wchar_t temp[MAX_PATH]{};
    assert(GetTempPathW(MAX_PATH, temp));
    const auto dir = std::filesystem::path(temp) / (L"smtvv-settings-test-" + std::to_wstring(::GetCurrentThreadId()));
    assert(!std::filesystem::exists(dir));
    std::filesystem::create_directory(dir);
    fakeModule = (dir / "runtime.dll").wstring();
    LoadSettings(nullptr);
    assert(Settings().eye == 40 && Settings().fov == 110 && Settings().gait == 5);
    clockMs += 600; FlushSettings();
    assert(std::filesystem::exists(configPath));
    std::array<bool, 4> previous{}; SeedKeys(previous);
    auto release = [&] { keys.fill(false); PollKeys(previous); };
    keys[VK_F4] = true; PollKeys(previous); PollKeys(previous);
    assert(Settings().eye == 41); // Held key does not repeat.
    release(); keys[VK_SHIFT] = keys[VK_CONTROL] = keys[VK_F5] = true; PollKeys(previous);
    assert(Settings().fov == 105);
    release(); keys[VK_SHIFT] = keys[VK_F6] = true; PollKeys(previous);
    assert(Settings().gait == 4.75f);
    release(); focused = false; keys[VK_F4] = true; PollKeys(previous);
    focused = true; PollKeys(previous); assert(Settings().eye == 41);
    release(); keys[VK_F9] = true; PollKeys(previous); assert(requested.load());
    clockMs += 600; FlushSettings();
    settingBits.store(CameraTuning::Defaults); LoadSettings(nullptr);
    assert(Settings().eye == 41 && Settings().fov == 105 && Settings().gait == 4.75f);
    assert(WritePrivateProfileStringW(L"Camera", L"FovDegrees", L"invalid", configPath.c_str()));
    LoadSettings(nullptr); assert(Settings().fov == 110);
    assert(WritePrivateProfileStringW(L"Camera", L"EyeOffsetCm", L"99999", configPath.c_str()));
    LoadSettings(nullptr); assert(Settings().eye == 100);
    release(); keys[VK_CONTROL] = keys[VK_F3] = true; PollKeys(previous);
    assert(settingBits.load() == CameraTuning::Defaults && requested.load());
    release(); keys[VK_F3] = true; PollKeys(previous); assert(!requested.load());
    std::filesystem::remove(configPath); std::filesystem::remove(dir);
    std::cout << "PASS production Win32 input, focus, no F9 binding, reset, real INI roundtrip and malformed values\n";
}
