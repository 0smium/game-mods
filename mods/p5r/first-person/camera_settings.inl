// SPDX-License-Identifier: GPL-3.0-or-later
// Worker owns configuration IO; game thread consumes a coherent atomic snapshot.
static std::atomic<uint32_t> settingBits{CameraTuning::Defaults};
static std::atomic<bool> requested{true};
static bool settingsDirty;
static ULONGLONG settingsChangedAt;
static float ReadSetting(const wchar_t* key,float fallback) {
    wchar_t buffer[96]{};GetPrivateProfileStringW(L"Camera",key,L"",buffer,96,iniPath);
    wchar_t* end{};float value=std::wcstof(buffer,&end);
    while(end && (*end==L' ' || *end==L'\t'))++end;
    return end && end!=buffer && !*end && std::isfinite(value)?value:fallback;
}
static void LoadSettings() {
    settingBits=CameraTuning::Encode(ReadSetting(L"EyeHeightCm",165),ReadSetting(L"FovDegrees",110),ReadSetting(L"BobAmplitudeCm",5));
    tuning=CameraTuning::Decode(settingBits.load());
    settingsDirty=GetFileAttributesW(iniPath)==INVALID_FILE_ATTRIBUTES;
    settingsChangedAt=GetTickCount64();
    Log("Settings eye=%.1f fov=%.1f bob=%.2f; first person starts requested",tuning.eye,tuning.fov,tuning.gait);
}
static void FlushSettings() {
    if(!settingsDirty || GetTickCount64()-settingsChangedAt<500)return;
    const auto v=CameraTuning::Decode(settingBits.load());
    const wchar_t* keys[]{L"EyeHeightCm",L"FovDegrees",L"BobAmplitudeCm"};
    const float values[]{v.eye,v.fov,v.gait};bool ok=true;
    for(unsigned i=0;i<3;i++) {wchar_t text[40];swprintf_s(text,L"%.2f",static_cast<double>(values[i]));ok=WritePrivateProfileStringW(L"Camera",keys[i],text,iniPath)!=0 && ok;}
    settingsDirty=false;
    if(!ok)Log("CONFIG_WRITE_FAILED: session values apply; check folder write access");
}
static void PollSettings(bool (&previous)[4]) {
    const bool focused=Focused();
    const bool shift=(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0,ctrl=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0,alt=(GetAsyncKeyState(VK_MENU)&0x8000)!=0;
    for(unsigned i=0;i<4;i++) {
        const bool down=(GetAsyncKeyState(VK_F3+i)&0x8000)!=0;
        if(focused && down && !previous[i]) {
            const auto action=CameraTuning::Chord(i,shift,ctrl,alt);
            if(action==CameraTuning::Action::Toggle) {requested=!requested.load();Log("F3 requested=%d",requested.load());}
            else if(action!=CameraTuning::Action::None) {
                settingBits=action==CameraTuning::Action::Reset?CameraTuning::Defaults:CameraTuning::Adjust(settingBits.load(),i-1,shift,ctrl);
                settingsDirty=true;settingsChangedAt=GetTickCount64();
                const auto v=CameraTuning::Decode(settingBits.load());Log("SETTINGS eye=%.1f fov=%.1f bob=%.2f",v.eye,v.fov,v.gait);
            }
        }
        previous[i]=down;
    }
}
