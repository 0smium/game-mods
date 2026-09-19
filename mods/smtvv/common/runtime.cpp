// SMTVV camera runtime 1.0.0-rc1. MIT; see LICENSE and THIRD_PARTY_NOTICES.md.
// Game-thread camera/height/gait control, native mesh ownership visibility.
// Layouts and discovery patterns: Lyall/SMTVFix 1.0.0 (see README).
#include <Windows.h>
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#include <safetyhook.hpp>
#include "SDK/ProjectPlayerCameraManager_classes.hpp"
#include "SDK/ProjectGameInstance_classes.hpp"
#include "SDK/PlayerBase_classes.hpp"
#include <atomic>
#include <cmath>
#include <array>
#include <algorithm>
#include <cstdarg>
#include <cwchar>
#include <cstdio>
#include <share.h>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SDK::InSDKUtils {
uintptr_t GetImageBase() { return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)); }
}

namespace {
HMODULE moduleHandle{};
FILE* logFile{};
std::mutex logMutex;
std::atomic<bool> disabled{false};
// User intent survives map/pawn/manager changes. ViewBlock and restoration
// still decide when this request can safely become an active field camera.
std::atomic<bool> requested{true};
std::atomic<unsigned> snapshotRequest{0};
std::atomic<unsigned long> sampleThread{0};
SDK::TUObjectArray* objectTable{};
SafetyHookInline processEventHook;
thread_local bool insideProbe = false;
std::atomic<unsigned> enabledFeatures{0};
std::atomic<bool> workerStarted{false};
std::mutex registrationMutex;
bool FirstPersonEnabled() { return (enabledFeatures.load() & 1u) != 0; }
bool FreeCameraEnabled() { return (enabledFeatures.load() & 2u) != 0; }

void Log(const char* format, ...) {
    std::lock_guard guard(logMutex);
    if (!logFile) return;
    SYSTEMTIME t{};
    GetLocalTime(&t);
    std::fprintf(logFile, "[%02u:%02u:%02u.%03u] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list args;
    va_start(args, format);
    std::vfprintf(logFile, format, args);
    va_end(args);
    std::fputc('\n', logFile);
    std::fflush(logFile);
}

#include "camera_settings.inl"

bool Readable(const void* pointer, size_t length) {
    if (!pointer || !length) return false;
    auto cursor = reinterpret_cast<uintptr_t>(pointer);
    const auto end = cursor + length;
    if (end < cursor) return false;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        const auto next = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    return true;
}

bool Executable(const void* pointer) {
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(pointer, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    return (info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

std::vector<std::uint8_t*> PatternMatches(const char* pattern) {
    std::vector<int> bytes;
    for (const char* p = pattern; *p;) {
        if (*p == ' ') { ++p; continue; }
        if (*p == '?') { bytes.push_back(-1); while (*p == '?') ++p; }
        else { char* next{}; bytes.push_back(static_cast<int>(std::strtoul(p, &next, 16))); p = next; }
    }
    auto base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    auto section = IMAGE_FIRST_SECTION(nt);
    std::vector<std::uint8_t*> matches;
    for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
        if (!(section[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        auto begin = base + section[s].VirtualAddress;
        const size_t size = section[s].Misc.VirtualSize;
        if (size < bytes.size() || !Readable(begin, size)) continue;
        for (size_t i = 0; i <= size - bytes.size(); ++i) {
            size_t j = 0;
            while (j < bytes.size() && (bytes[j] == -1 || begin[i + j] == bytes[j])) ++j;
            if (j == bytes.size()) matches.push_back(begin + i);
        }
    }
    return matches;
}

// A unique match is required for every code address actually used by the probe.
std::uint8_t* UniquePattern(const char* pattern) {
    const auto matches = PatternMatches(pattern);
    const auto base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    Log("pattern matches=%zu offset=0x%llx", matches.size(), matches.empty() ? 0 : static_cast<unsigned long long>(matches.back() - base));
    return matches.size() == 1 ? matches.front() : nullptr;
}

const std::uint8_t* RelativeTarget(const std::uint8_t* operand) {
    std::int32_t displacement{};
    std::memcpy(&displacement, operand, sizeof(displacement));
    return operand + 4 + displacement;
}

void ObserveOptionalGObjectsReferences() {
    const auto matches = PatternMatches("48 8B ?? ?? ?? ?? ?? 48 8B ?? ?? 48 8D ?? ?? EB ?? 33 ?? 8B ?? ?? C1 ??");
    std::unordered_set<uintptr_t> readableTargets;
    size_t invalid = 0;
    for (const auto reference : matches) {
        const auto target = RelativeTarget(reference + 3);
        if (Readable(target, sizeof(void*))) readableTargets.insert(reinterpret_cast<uintptr_t>(target));
        else ++invalid;
    }
    Log("GObjects rawReferences=%zu distinctReadableTargets=%zu invalidTargets=%zu",
        matches.size(), readableTargets.size(), invalid);
    if (readableTargets.size() == 1 && invalid == 0) {
        objectTable = reinterpret_cast<SDK::TUObjectArray*>(*readableTargets.begin());
        Log("GObjects candidate=0x%llx (RIP target deduplicated; per-callback array/index validation required for writes)",
            static_cast<unsigned long long>(*readableTargets.begin()));
    } else Log("GObjects unresolved; observation can continue but camera control is blocked");
}

bool ValidObject(const SDK::UObject* obj) {
    if (!Readable(obj, sizeof(SDK::UObject))) return false;
    constexpr std::uint32_t rejectedFlags = 0x10 | 0x20 | 0x8000 | 0x10000;
    return !(static_cast<std::uint32_t>(obj->Flags) & rejectedFlags) && obj->Index >= 0 && Readable(obj->Class, sizeof(SDK::UObject));
}

std::string Name(const SDK::UObject* obj) {
    if (!obj) return "null";
    if (!Readable(obj, sizeof(SDK::UObject))) return "unreadable";
    return obj->Name.ToString();
}

std::string ClassName(const SDK::UObject* obj) {
    return obj && Readable(obj, sizeof(SDK::UObject)) ? Name(obj->Class) : "null";
}

bool HasClass(const SDK::UObject* object, const char* name) {
    if (!ValidObject(object)) return false;
    auto type = static_cast<SDK::UStruct*>(object->Class);
    for (unsigned depth = 0; type && depth < 24; ++depth) {
        if (!Readable(type, sizeof(SDK::UStruct))) return false;
        if (Name(type) == name) return true;
        type = type->Super;
    }
    return false;
}

// Traverse only the current callback object's ownership chain; do not keep a
// UObject pointer to dereference later or use the SDK's incomplete weak handles.
SDK::UWorld* OwningWorld(SDK::UObject* object) {
    for (unsigned n = 0; object && n < 12; ++n) {
        if (!Readable(object, sizeof(SDK::UObject))) return nullptr;
        if (ClassName(object) == "World") return reinterpret_cast<SDK::UWorld*>(object);
        object = object->Outer;
    }
    return nullptr;
}

bool GardenVisibilityRestore(const char* reason);
struct CameraContext;
bool DashEffectRestore(const char* reason);
void DashEffectUpdate(const CameraContext& context);
#include "camera_control.inl"
#include "garden_properties.inl"
#include "dash_effect_visibility.inl"
#include "garden_visibility.inl"
#include "garden_free_camera.inl"

bool Observe(SDK::UObject* object, SDK::UFunction* function) {
    if (!object || !function || !object->Class) return false;
    // Names are cached per thread by the immutable FName value, not UObject
    // pointer, so object/address reuse cannot turn a different class into a camera.
    thread_local std::unordered_map<std::uint64_t, bool> cameraClasses;
    std::uint64_t classKey{};
    std::memcpy(&classKey, &object->Class->Name, sizeof(classKey));
    auto it = cameraClasses.find(classKey);
    if (it == cameraClasses.end()) {
        if (cameraClasses.size() >= 4096) cameraClasses.clear();
        it = cameraClasses.emplace(classKey, object->Class->Name.ToString() == "ProjectPlayerCameraManager_C").first;
    }
    if (!it->second || !ValidObject(object)) return false;
    const auto functionName = function->Name.ToString();
    // ReceiveTick and its owning Blueprint were observed in the live game.
    // Manual_Tick stays diagnostic; it has not been observed as the active path.
    const bool tick = functionName == "ReceiveTick" || functionName == "Player_Camera_Manual_Tick";
    if (!tick) return false;
    unsigned long unset = 0;
    sampleThread.compare_exchange_strong(unset, GetCurrentThreadId());
    if (sampleThread.load() != GetCurrentThreadId()) { Log("rejected camera callback from a second thread"); return false; }
    return functionName == "ReceiveTick" && Name(function->Outer) == "ProjectPlayerCameraManager_C";
}

bool ObserveCpp(SDK::UObject* object, SDK::UFunction* function) noexcept {
    try { return Observe(object, function); }
    catch (...) { disabled.store(true); requested.store(false); publishedState.store("FAULT: restart required; restoration unconfirmed"); Log("C++ probe failure; future control disabled; restoration NOT confirmed, restart game before continuing"); return false; }
}

// This guard covers only our inspection. Exceptions from the original game
// callback are not swallowed. A stale/layout-mismatched read permanently stops
// observation instead of trying alternate offsets or modifying anything.
bool ObserveGuarded(SDK::UObject* object, SDK::UFunction* function) {
    __try { return ObserveCpp(object, function); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        disabled.store(true);
        requested.store(false);
        publishedState.store("FAULT: restart required; restoration unconfirmed");
        Log("probe access failure code=0x%08lx; future control disabled; restoration NOT confirmed, restart game before continuing", GetExceptionCode());
        return false;
    }
}

void ControlCpp(SDK::UObject* object, ObjectIdentity* identity, float* delta, const void* parameters, bool after) noexcept {
    try {
        const auto manager = reinterpret_cast<SDK::AProjectPlayerCameraManager_C*>(object);
        if (after) ControlAfter(manager, *identity, *delta);
        else {
            if (Readable(parameters, sizeof(float))) std::memcpy(delta, parameters, sizeof(float));
            *identity = Identify(object);
            ControlBefore(manager);
        }
    } catch (...) { disabled.store(true); requested.store(false); publishedState.store("FAULT: restart required; restoration unconfirmed"); Log("control failure; no further writes; restoration NOT confirmed, restart game before continuing"); }
}

void ControlGuarded(SDK::UObject* object, ObjectIdentity* identity, float* delta, const void* parameters, bool after) {
    __try { ControlCpp(object, identity, delta, parameters, after); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        disabled.store(true);
        requested.store(false);
        publishedState.store("FAULT: restart required; restoration unconfirmed");
        Log("control access failure code=0x%08lx; no further writes; restoration NOT confirmed, restart game before continuing", GetExceptionCode());
    }
}

bool GardenBeforeCpp(SDK::UObject* object, SDK::UFunction* function) noexcept {
    try { return GardenVisibilityBefore(object, function); }
    catch (...) {
        disabled.store(true); requested.store(false);
        publishedState.store("FAULT: garden guard; restart required");
        Log("GARDEN_VISIBILITY_FAULT before callback; cleanup will be attempted, restart required");
        return GardenVisibility::scopeActive;
    }
}

bool GardenBeforeGuarded(SDK::UObject* object, SDK::UFunction* function) {
    __try { return GardenBeforeCpp(object, function); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        disabled.store(true); requested.store(false);
        publishedState.store("FAULT: garden guard; restart required");
        Log("GARDEN_VISIBILITY_FAULT code=0x%08lx before callback; cleanup will be attempted", GetExceptionCode());
        return GardenVisibility::scopeActive;
    }
}

void GardenAfterCpp() noexcept {
    try { GardenVisibilityAfter(); }
    catch (...) {
        disabled.store(true); requested.store(false); GardenVisibility::scopeActive = false;
        publishedState.store("FAULT: garden restore unconfirmed; restart required");
        Log("GARDEN_VISIBILITY_FAULT cleanup failed; restoration unconfirmed, restart required");
    }
}

void GardenAfterGuarded() {
    __try { GardenAfterCpp(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        disabled.store(true); requested.store(false); GardenVisibility::scopeActive = false;
        publishedState.store("FAULT: garden restore unconfirmed; restart required");
        Log("GARDEN_VISIBILITY_FAULT code=0x%08lx cleanup failed; restart required", GetExceptionCode());
    }
}

bool FreeCameraBeforeCpp(SDK::UObject* object, SDK::UFunction* function, void* parameters, GardenFreeCamera::CallScope* scope) noexcept {
    try { return GardenFreeCameraBefore(object, function, parameters, *scope); }
    catch (...) {
        disabled.store(true); requested.store(false);
        publishedState.store("FAULT: garden free-camera guard; restart required");
        Log("GARDEN_FREE_CAMERA_FAULT before callback; parameter cleanup will be attempted");
        return scope->active;
    }
}

bool FreeCameraBeforeGuarded(SDK::UObject* object, SDK::UFunction* function, void* parameters, GardenFreeCamera::CallScope* scope) {
    __try { return FreeCameraBeforeCpp(object, function, parameters, scope); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        disabled.store(true); requested.store(false);
        publishedState.store("FAULT: garden free-camera guard; restart required");
        Log("GARDEN_FREE_CAMERA_FAULT code=0x%08lx before callback; parameter cleanup will be attempted", GetExceptionCode());
        return scope->active;
    }
}

void FreeCameraAfterCpp(GardenFreeCamera::CallScope* scope) noexcept {
    try { GardenFreeCameraAfter(*scope); }
    catch (...) {
        disabled.store(true); requested.store(false); scope->active = false;
        publishedState.store("FAULT: free-camera parameter restore unconfirmed; restart required");
        Log("GARDEN_FREE_CAMERA_FAULT cleanup failed; restart required");
    }
}

void FreeCameraAfterGuarded(GardenFreeCamera::CallScope* scope) {
    __try { FreeCameraAfterCpp(scope); }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        disabled.store(true); requested.store(false); scope->active = false;
        publishedState.store("FAULT: free-camera parameter restore unconfirmed; restart required");
        Log("GARDEN_FREE_CAMERA_FAULT code=0x%08lx cleanup failed; restart required", GetExceptionCode());
    }
}

void ProcessEvent(SDK::UObject* object, SDK::UFunction* function, void* parameters) {
    bool cameraTick = false;
    bool gardenTick = false;
    bool freeCameraOverlap = false;
    GardenFreeCamera::CallScope freeCameraScope{};
    ObjectIdentity identity{};
    float delta = 0;
    if (!insideProbe && !disabled.load()) {
        insideProbe = true;
        cameraTick = ObserveGuarded(object, function);
        if (FirstPersonEnabled() && cameraTick && !disabled.load()) ControlGuarded(object, &identity, &delta, parameters, false);
        if (FirstPersonEnabled() && !cameraTick && !disabled.load()) gardenTick = GardenBeforeGuarded(object, function);
        if (FreeCameraEnabled() && !cameraTick && !gardenTick && !disabled.load()) freeCameraOverlap = FreeCameraBeforeGuarded(object, function, parameters, &freeCameraScope);
        insideProbe = false;
    }
    // Both guarded events are synchronous. Restore temporary garden permission
    // or the free-camera callback argument even if the native event unwinds.
    // Never swallow the game's exception or skip/repeat its original callback.
    __try { processEventHook.unsafe_call<void>(object, function, parameters); }
    __finally {
        if (gardenTick) {
            insideProbe = true;
            GardenAfterGuarded();
            insideProbe = false;
        }
        if (freeCameraOverlap) {
            insideProbe = true;
            FreeCameraAfterGuarded(&freeCameraScope);
            insideProbe = false;
        }
    }
    if (FirstPersonEnabled() && cameraTick && !insideProbe && !disabled.load()) {
        insideProbe = true;
        ControlGuarded(object, &identity, &delta, parameters, true);
        insideProbe = false;
    }
}

DWORD WINAPI Initialize(void*) {
    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(moduleHandle, modulePath, MAX_PATH);
    const auto path = std::filesystem::path(modulePath).parent_path() / L"SMTVVCameraRuntime.log";
    statusPath = std::filesystem::path(modulePath).parent_path() / L"SMTVVFirstPerson.status.txt";
    logFile = _wfsopen(path.c_str(), L"a", _SH_DENYNO);
    Log("START v1.0.0-rc1 pid=%lu features=%u; shared hook, separate installable features", GetCurrentProcessId(), enabledFeatures.load());
    try {
        wchar_t executablePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
        if (_wcsicmp(std::filesystem::path(executablePath).filename().c_str(), L"SMT5V-Win64-Shipping.exe") != 0) {
            Log("STOP: unsupported host executable; no hook installed");
            return 0;
        }
        Log("discovery ProcessEvent");
        const auto event = UniquePattern("40 ?? 56 57 41 ?? 41 ?? 41 ?? 41 ?? 48 81 ?? ?? ?? ?? ?? 48 8D ?? ?? ?? 48 89 ?? ?? ?? ?? ?? 48 8B ?? ?? ?? ?? ?? 48 33 ?? 48 89 ?? ?? ?? ?? ?? 8B ?? ?? 45 33 ??");
        Log("discovery AppendString callsite");
        const auto appendCall = UniquePattern("48 89 ?? ?? ?? E8 ?? ?? ?? ?? 48 8B ?? ?? 48 85 ?? 75 ?? 48 8B ?? ?? ?? 48 8B ??");
        Log("discovery GObjects (required for control identity checks)");
        ObserveOptionalGObjectsReferences();
        if (!event || !appendCall) { Log("FAILED: required code discovery not unique; no hook installed"); return 0; }
        const auto append = RelativeTarget(appendCall + 6);
        if (!Executable(event) || !Executable(append)) {
            Log("FAILED: resolved addresses invalid; no hook installed"); return 0;
        }
        SDK::FName::InitManually(const_cast<std::uint8_t*>(append));
        Log("required code discovery verified; installing ProcessEvent observer/controller; request default ON, control waits for validated field camera");
        auto hook = safetyhook::InlineHook::create(event, reinterpret_cast<void*>(&ProcessEvent), safetyhook::InlineHook::StartDisabled);
        if (!hook) { Log("FAILED: hook construction; original callback untouched"); return 0; }
        processEventHook = std::move(*hook);
        publishedState.store("ON / waiting for validated field camera");
        if (const auto result = processEventHook.enable(); !result) { Log("FAILED: hook enable; no camera operations"); return 0; }
        Log("READY runtime v1.0.0-rc1; no debug menu, boss revival or creature scaling");
        std::array<bool, 4> previous{};
        SeedKeys(previous);
        ULONGLONG lastStatus{};
        for (;;) {
            if (FirstPersonEnabled()) { PollKeys(previous); FlushSettings(); }
            const auto now = GetTickCount64();
            if (FirstPersonEnabled() && now - lastStatus >= 1000) { WriteStatus(); lastStatus = now; }
            Sleep(25);
        }
    } catch (...) { disabled.store(true); Log("FAILED: initialization exception; no camera operations"); }
    return 0;
}
}

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        moduleHandle = self;
        DisableThreadLibraryCalls(self);

    }
    return TRUE;
}

// Feature entrypoints call from their worker, never from the loader lock.
// Windows loads this identically named DLL once, so both ASIs share one hook.
extern "C" __declspec(dllexport) BOOL RegisterCameraFeature(unsigned feature, unsigned abi) {
    if (abi != 1 || (feature != 1 && feature != 2)) return FALSE;
    std::lock_guard guard(registrationMutex);
    if ((enabledFeatures.load() & feature) != 0) return TRUE;
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&RegisterCameraFeature), &pinned)) return FALSE;
    if (feature == 1) LoadSettings(moduleHandle);
    enabledFeatures.fetch_or(feature);
    if (!workerStarted.exchange(true)) {
        const auto worker = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr);
        if (!worker) { workerStarted.store(false); enabledFeatures.fetch_and(~feature); return FALSE; }
        CloseHandle(worker);
    }
    return TRUE;
}
