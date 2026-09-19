// A single atomic tuning snapshot crosses from the keyboard worker to the game.
// Disk IO and Win32 input never access game objects.
#include "settings_policy.hpp"
using CameraSettings = CameraTuning::Values;
std::atomic<std::uint32_t> settingBits{CameraTuning::Defaults};
std::atomic<bool> foregroundGame{false};
std::atomic<const char*> publishedState{"starting"};
std::atomic<float> publishedBob{0}, publishedSpeed{0};
std::atomic<const char*> publishedGround{"not sampled"};
constexpr std::array<int, 4> cameraHotkeys{VK_F3, VK_F4, VK_F5, VK_F6};
std::filesystem::path statusPath, configPath;
bool settingsDirty{};
ULONGLONG settingsChangedAt{};
bool settingsWriteFailed{};
CameraSettings Settings() { return CameraTuning::Decode(settingBits.load()); }

float ReadSetting(const wchar_t* key, float fallback) {
    wchar_t buffer[96]{};
    GetPrivateProfileStringW(L"Camera", key, L"", buffer, 96, configPath.c_str());
    wchar_t* end{};
    const auto result = std::wcstof(buffer, &end);
    while (end && (*end == L' ' || *end == L'\t')) ++end;
    return end && end != buffer && !*end && std::isfinite(result) ? result : fallback;
}
void LoadSettings(HMODULE module) {
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(module, path, MAX_PATH)) return;
    configPath = std::filesystem::path(path).parent_path() / L"SMTVVFirstPerson.ini";
    settingBits.store(CameraTuning::Encode(ReadSetting(L"EyeOffsetCm", 40),
        ReadSetting(L"FovDegrees", 110), ReadSetting(L"BobAmplitudeCm", 5)));
    // Preserve existing settings. Only a missing file is created automatically.
    settingsDirty = GetFileAttributesW(configPath.c_str()) == INVALID_FILE_ATTRIBUTES;
    settingsChangedAt = GetTickCount64();
}
void FlushSettings() {
    if (!settingsDirty || configPath.empty() || GetTickCount64() - settingsChangedAt < 500) return;
    const auto value = Settings();
    const wchar_t* keys[]{L"EyeOffsetCm", L"FovDegrees", L"BobAmplitudeCm"};
    const float values[]{value.eye, value.fov, value.gait};
    bool ok = true;
    for (unsigned i = 0; i < 3; ++i) {
        wchar_t text[40]{};
        std::swprintf(text, 40, L"%.2f", static_cast<double>(values[i]));
        ok = WritePrivateProfileStringW(L"Camera", keys[i], text, configPath.c_str()) != 0 && ok;
    }
    settingsWriteFailed = !ok;
    settingsDirty = false; // No repeated IO/error loop; next edit can retry.
    if (!ok) Log("CONFIG_WRITE_FAILED: settings apply for this session; check folder write access");
}
void ChangedSettings() {
    settingsDirty = true;
    settingsChangedAt = GetTickCount64();
    snapshotRequest.fetch_add(1);
    const auto v = Settings();
    Log("SETTINGS eye=%+.1fcm fov=%.0f bob=%.2fcm", v.eye, v.fov, v.gait);
}
void SeedKeys(std::array<bool, 4>& previous) {
    for (unsigned i = 0; i < previous.size(); ++i)
        previous[i] = (GetAsyncKeyState(cameraHotkeys[i]) & 0x8000) != 0;
}
void PollKeys(std::array<bool, 4>& previous) {
    DWORD pid{};
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    const bool focused = pid == GetCurrentProcessId();
    foregroundGame.store(focused);
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    for (unsigned i = 0; i < previous.size(); ++i) {
        const bool down = (GetAsyncKeyState(cameraHotkeys[i]) & 0x8000) != 0;
        if (focused && down && !previous[i]) {
            const auto action = CameraTuning::Chord(i, shift, ctrl, alt);
            if (action == CameraTuning::Action::Toggle) {
                requested.store(!requested.load());
                Log("F3 requested=%d", requested.load());
            } else if (action == CameraTuning::Action::Reset) {
                settingBits.store(CameraTuning::Defaults);
                ChangedSettings();
            } else if (action != CameraTuning::Action::None) {
                settingBits.store(CameraTuning::Adjust(settingBits.load(), i - 1, shift, ctrl));
                ChangedSettings();
            }
        }
        previous[i] = down; // Held keys cannot carry an edge across focus changes.
    }
}
void WriteStatus() {
    if (statusPath.empty()) return;
    const auto file = _wfsopen(statusPath.c_str(), L"w", _SH_DENYNO);
    if (!file) return;
    const auto v = Settings();
    std::fprintf(file, "SMTVV First Person 1.0.0-rc1\nRequested: %s\nState: %s\n"
        "Eye offset: %+.1f cm [-20..100]\nFOV: %.0f [60..140]\nBob amplitude: %.2f cm [0..10]\n"
        "F3 Toggle | Ctrl+F3 Reset tuning\nF4/F5/F6 Increase | Shift Decrease | Ctrl Coarse step\n"
        "Fine steps: 1 cm / 1 degree / 0.25 cm. Coarse: 5 cm / 5 degrees / 1 cm.\n"
        "Defaults: +40 / 110 / 5. Settings saved in SMTVVFirstPerson.ini.\n"
        "Config write: %s\n", requested.load() ? "ON" : "OFF", publishedState.load(),
        v.eye, v.fov, v.gait, settingsWriteFailed ? "FAILED (session changes still apply)" : "OK");
    std::fclose(file);
}
