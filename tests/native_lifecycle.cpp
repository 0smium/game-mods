// The runner generates camera_control.test.inl from the current production file
// verbatim, replacing only its two external subsystem includes with adapters.
// This exercises actual Context/Resolve/Before/After/restore code, not a model.
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace SDK {
struct UClass;
struct UWorld;
struct APlayerController;
struct AProjectPlayerCameraManager_C;
struct UObject {
    UClass* Class{};
    UObject* Outer{};
    std::uint64_t Name{};
    int Index{-1};
    std::uint32_t Flags{};
    UWorld* mockWorld{};
};
struct UClass : UObject { std::string mockClass; };
template<class T> struct TArray : std::vector<T> {
    int Num() const { return static_cast<int>(this->size()); }
};
struct FVector { float X{}, Y{}, Z{}; };
struct FRotator { float Pitch{}, Yaw{}, Roll{}; };
struct USceneComponent : UObject {
    USceneComponent* AttachParent{};
    bool bAbsoluteLocation{}, bAbsoluteRotation{}, bAbsoluteScale{};
    FVector RelativeLocation{}, RelativeScale3D{1, 1, 1};
    FRotator RelativeRotation{};
};
struct UCameraComponent : USceneComponent { float FieldOfView{75}; };
struct UCustomSpringArmComponent : USceneComponent {
    float TargetArmLength{250};
    FVector TargetOffset{}, SocketOffset{};
};
struct APlayerBase_C : UObject {
    UCameraComponent* Camera{};
    UCustomSpringArmComponent* CustomSpringArm{};
    USceneComponent* RootComponent{};
    USceneComponent* CameraRoot{};
    float NearCameraZOffset{3};
    float BaseEyeHeight{64};
};
struct UGameViewportClient : UObject { UWorld* World{}; };
struct ULocalPlayer : UObject {
    APlayerController* PlayerController{};
    UGameViewportClient* ViewportClient{};
};
struct APlayerController : UObject {
    AProjectPlayerCameraManager_C* PlayerCameraManager{};
    ULocalPlayer* Player{};
    APlayerBase_C* Pawn{};
};
struct UProjectGameInstance_C : UObject {
    TArray<ULocalPlayer*> LocalPlayers;
    bool IsMapChange{}, IsLoadMap{}, IsMapStartLoad{};
    bool IsEventView{}, IsEventView_Simple{};
    int m_BattleState{};
};
struct UWorld : UObject { UProjectGameInstance_C* OwningGameInstance{}; };
struct FViewTarget { UObject* target{}; };
struct AProjectPlayerCameraManager_C : UObject {
    APlayerController* PCOwner{};
    FViewTarget ViewTarget{}, PendingViewTarget{};
    bool ForceHidden{};
    int ForceCounter{};
};
struct FUObjectItem { UObject* Object{}; };
struct TUObjectArray {
    static constexpr int ElementsPerChunk = 512;
    FUObjectItem** Objects{};
    int NumElements{}, MaxElements{512}, NumChunks{1}, MaxChunks{1};
};
}

std::array<SDK::FUObjectItem, 512> testObjects;
SDK::FUObjectItem* testChunk = testObjects.data();
SDK::TUObjectArray testTable{&testChunk};
SDK::TUObjectArray* objectTable = &testTable;
std::map<std::string, std::unique_ptr<SDK::UClass>> testClasses;
std::set<const void*> unreadable;
std::atomic<bool> requested{TEST_STARTUP_REQUEST};
std::atomic<const char*> publishedState{"test"};
using ULONGLONG = unsigned long long;
ULONGLONG GetTickCount64() { return 3000; }
unsigned long GetCurrentThreadId() { return 1; }
void Log(const char*, ...) {}
bool Readable(const void* p, std::size_t) { return p && !unreadable.count(p); }
bool ValidObject(const SDK::UObject* p) {
    return Readable(p, sizeof(*p)) && p->Class && !(p->Flags & (0x8000 | 0x10000));
}
bool HasClass(const SDK::UObject* p, const char* kind) {
    return ValidObject(p) && p->Class->mockClass == kind;
}
SDK::UWorld* OwningWorld(const SDK::UObject* p) { return p ? p->mockWorld : nullptr; }
std::string Name(const SDK::UObject* p) { return p ? std::to_string(p->Name) : "none"; }
struct CameraSettings { float eye, fov, gait; };
CameraSettings Settings() { return {40, 110, 0}; }

#include "camera_control.test.inl"

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<class T> void Register(T& object, const char* type, SDK::UWorld* world) {
    auto& cls = testClasses[type];
    if (!cls) {
        cls = std::make_unique<SDK::UClass>();
        cls->mockClass = type;
    }
    object.Class = cls.get();
    object.Index = testTable.NumElements++;
    object.Name = static_cast<std::uint64_t>(object.Index + 100);
    object.mockWorld = world;
    testObjects.at(object.Index).Object = &object;
}

struct Pawn {
    SDK::APlayerBase_C object;
    SDK::USceneComponent root, cameraRoot;
    SDK::UCustomSpringArmComponent arm;
    SDK::UCameraComponent camera;
    explicit Pawn(SDK::UWorld* world) {
        Register(object, "PlayerBase_C", world);
        Register(root, "SceneComponent", world);
        Register(cameraRoot, "SceneComponent", world);
        Register(arm, "CustomSpringArmComponent", world);
        Register(camera, "CameraComponent", world);
        object.RootComponent = &root;
        object.CameraRoot = &cameraRoot;
        object.CustomSpringArm = &arm;
        object.Camera = &camera;
        cameraRoot.AttachParent = &root;
        cameraRoot.RelativeLocation.Z = 100;
        arm.AttachParent = &cameraRoot;
        arm.RelativeLocation.Z = 30;
        camera.AttachParent = &arm;
    }
    void Native() const {
        Require(arm.TargetArmLength == 250, "native arm length must be restored");
        Require(camera.FieldOfView == 75, "native FOV must be restored");
        Require(arm.TargetOffset.Z == 0, "native target height must be restored");
        Require(object.NearCameraZOffset == 3, "native near height must be restored");
    }
    void Active() const {
        Require(arm.TargetArmLength == 0, "first-person arm must be active");
        Require(camera.FieldOfView == 110, "requested FOV must be active");
        Require(arm.TargetOffset.Z == -26, "calibrated target height must be active");
        Require(object.NearCameraZOffset == 0, "native near height must be suppressed");
    }
};

struct Scene {
    SDK::UWorld world;
    SDK::UProjectGameInstance_C game;
    SDK::UGameViewportClient viewport;
    SDK::ULocalPlayer ownLocal;
    SDK::ULocalPlayer* local;
    SDK::APlayerController controller;
    SDK::AProjectPlayerCameraManager_C manager;
    Pawn pawn{&world};
    explicit Scene(SDK::ULocalPlayer* shared = nullptr) : local(shared ? shared : &ownLocal) {
        Register(world, "World", &world);
        Register(game, "ProjectGameInstance_C", &world);
        Register(viewport, "GameViewportClient", &world);
        Register(ownLocal, "LocalPlayer", &world);
        Register(controller, "PlayerController", &world);
        Register(manager, "ProjectPlayerCameraManager_C", &world);
        world.OwningGameInstance = &game;
        game.LocalPlayers.push_back(local);
        viewport.World = &world;
        controller.Player = local;
        controller.Pawn = &pawn.object;
        controller.PlayerCameraManager = &manager;
        manager.PCOwner = &controller;
        manager.ViewTarget.target = &pawn.object;
    }
    void Select() {
        local->PlayerController = &controller;
        local->ViewportClient = &viewport;
    }
    void Tick() {
        const auto beforeIdentity = Identify(&manager);
        ControlBefore(&manager);
        ControlAfter(&manager, beforeIdentity, 1.f / 60);
    }
    void Start() { Select(); Tick(); pawn.Active(); Require(requested.load(), "request retained"); }
};

void Reset() {
    testObjects.fill({});
    testTable = {&testChunk};
    objectTable = &testTable;
    testClasses.clear();
    unreadable.clear();
    requested.store(TEST_STARTUP_REQUEST);
    publishedState.store("test");
    binding = {};
    applied = lengthOwned = fovOwned = targetZOwned = nearOffsetOwned = false;
    targetZSuppressed = restorationPending = discardAfterRestore = false;
    originalLength = originalFov = lastWrittenFov = originalTargetZ = 0;
    lastWrittenTargetZ = originalNearOffset = 0;
    lastBlock = nullptr;
    contextFailure = "identity-unresolved";
    TestVisibility::owner = nullptr;
    TestVisibility::pending = TestVisibility::failApply = false;
    TestVisibility::applies = TestVisibility::restores = 0;
    TestVisibility::onApply = {};
    TestGarden::pending = false;
    TestGarden::restores = 0;
    TestDash::pending = false;
    TestDash::restores = TestDash::updates = 0;
}

void StartupAndManualOff() {
    Require(requested.load(), "production default must request first person");
    Scene s;
    s.Start();
    requested.store(false);
    s.Tick();
    s.pawn.Native();
    Require(!binding.valid && !restorationPending, "off must release completed binding");
    Scene next(s.local);
    next.Select();
    next.Tick();
    next.pawn.Native();
    Require(!requested.load(), "F3 off persists through map change");
    requested.store(true);
    next.Tick();
    next.pawn.Active();
}

void ManagerTakeover() {
    Scene old;
    old.Start();
    Scene next(old.local);
    next.Select();
    bool checkedOrder = false;
    TestVisibility::onApply = [&](const CameraContext& c) {
        Require(c.manager == &next.manager, "only new manager may apply");
        old.pawn.Native();
        next.pawn.Native();
        checkedOrder = true;
    };
    next.Tick();
    Require(checkedOrder, "old camera restoration must precede new camera apply");
    Require(requested.load(), "manager takeover must preserve request");
    next.pawn.Active();
    TestVisibility::onApply = {};
    old.Tick();
    next.pawn.Active();
    Require(binding.manager.pointer == &next.manager, "stale manager must not steal binding");
}

void SameManagerNewPawn() {
    Scene s;
    s.Start();
    Pawn replacement(&s.world);
    s.controller.Pawn = &replacement.object;
    s.manager.ViewTarget.target = &replacement.object;
    s.Tick();
    s.pawn.Native();
    replacement.Active();
    Require(requested.load() && binding.pawn.pointer == &replacement.object, "pawn replacement automatically resumes");
}

void TemporaryInvalidContext() {
    Scene s;
    s.Start();
    s.local->ViewportClient = nullptr;
    s.Tick();
    s.pawn.Native();
    Require(requested.load() && binding.valid && !applied, "temporary context loss pauses without losing intent");
    s.Select();
    s.Tick();
    s.pawn.Active();
}

void PendingArmRestore() {
    Scene old;
    old.Start();
    Scene next(old.local);
    next.Select();
    unreadable.insert(&old.pawn.arm);
    next.Tick();
    Require(restorationPending && lengthOwned && targetZOwned, "unreadable old arm retains snapshot");
    Require(!fovOwned && !nearOffsetOwned, "independent readable fields restore while arm waits");
    Require(old.pawn.camera.FieldOfView == 75 && old.pawn.object.NearCameraZOffset == 3, "readable old fields restored");
    next.pawn.Native();
    Require(requested.load(), "pending restore preserves request");
    next.Tick();
    next.pawn.Native();
    unreadable.erase(&old.pawn.arm);
    next.Tick();
    old.pawn.Native();
    next.pawn.Active();
    Require(!restorationPending, "restoration resolved before automatic resume");
}

void PendingVisibilityAndF3Off() {
    Scene old;
    old.Start();
    Scene next(old.local);
    next.Select();
    TestVisibility::pending = true;
    next.Tick();
    old.pawn.Native();
    next.pawn.Native();
    Require(restorationPending && !applied, "pending visibility alone must block new camera");
    requested.store(false);
    next.Tick();
    Require(restorationPending && !requested.load(), "F3 off accepted during pending restoration");
    TestVisibility::pending = false;
    next.Tick();
    next.Tick();
    next.pawn.Native();
    Require(!binding.valid && !restorationPending && !requested.load(), "late restore completion must not re-enable F3 off");
}

void PendingGardenAndF3Off() {
    Scene old;
    old.Start();
    Scene next(old.local);
    next.Select();
    TestGarden::pending = true;
    next.Tick();
    old.pawn.Native();
    next.pawn.Native();
    Require(restorationPending && !applied && binding.valid, "pending garden flags alone must retain camera binding and block new writes");
    Require(TestGarden::restores > 0, "camera restoration must call garden restoration");
    requested.store(false);
    next.Tick();
    Require(restorationPending && !requested.load(), "F3 off stays accepted while garden restoration waits");
    TestGarden::pending = false;
    next.Tick();
    next.Tick();
    next.pawn.Native();
    Require(!binding.valid && !restorationPending && !requested.load(), "garden restore completion releases binding without re-enabling off intent");
}

void PendingDashAndF3Off() {
    Scene old;
    old.Start();
    Require(TestDash::updates == 1, "successful production camera apply invokes dash update");
    Scene next(old.local);
    next.Select();
    TestDash::pending = true;
    next.Tick();
    old.pawn.Native();
    next.pawn.Native();
    Require(restorationPending && !applied && binding.valid, "pending dash flag alone retains camera binding and blocks new map");
    Require(TestDash::restores > 0 && TestDash::updates == 1, "restore calls dash cleanup and suppresses new effect update");
    requested.store(false);
    next.Tick();
    Require(restorationPending && !requested.load(), "F3 off remains accepted while dash cleanup waits");
    TestDash::pending = false;
    next.Tick();
    next.Tick();
    next.pawn.Native();
    Require(!binding.valid && !restorationPending && !requested.load(), "dash restore completion does not re-enable manually disabled intent");
}

void UnrelatedManager() {
    Scene active;
    active.Start();
    Scene other;
    other.Select();
    other.Tick();
    active.pawn.Active();
    other.pawn.Native();
    Require(binding.manager.pointer == &active.manager && requested.load(), "valid different local player must not cancel active binding");
    other.controller.PlayerCameraManager = nullptr;
    other.Tick();
    active.pawn.Active();
}

void RetiredLocalAllowsTakeover() {
    Scene old;
    old.Start();
    old.local->Flags |= 0x8000;
    Scene next;
    next.Start();
    old.pawn.Native();
    Require(binding.localPlayer.pointer == next.local, "proven retired local allows replacement local");
}

void BlockedViewsResume() {
    Scene s;
    s.Start();
    const std::vector<std::function<void(bool)>> blocks{
        [&](bool b) { s.manager.ViewTarget.target = b ? static_cast<SDK::UObject*>(&s.manager) : &s.pawn.object; },
        [&](bool b) { s.manager.PendingViewTarget.target = b ? &s.manager : nullptr; },
        [&](bool b) { s.manager.ForceHidden = b; },
        [&](bool b) { s.manager.ForceCounter = b ? 1 : 0; },
        [&](bool b) { s.game.IsMapChange = b; },
        [&](bool b) { s.game.IsLoadMap = b; },
        [&](bool b) { s.game.IsMapStartLoad = b; },
        [&](bool b) { s.game.IsEventView = b; },
        [&](bool b) { s.game.IsEventView_Simple = b; },
        [&](bool b) { s.game.m_BattleState = b ? 1 : 0; }
    };
    for (const auto& block : blocks) {
        block(true);
        s.Tick();
        s.pawn.Native();
        Require(requested.load() && !applied, "menu/event/map guard pauses request");
        s.Tick();
        s.pawn.Native();
        block(false);
        s.Tick();
        s.pawn.Active();
    }
}

void BlockArisesDuringNativeCallback() {
    Scene s;
    s.Start();
    const auto identity = Identify(&s.manager);
    ControlBefore(&s.manager);
    s.game.IsEventView = true;
    ControlAfter(&s.manager, identity, 1.f / 60);
    s.pawn.Native();
    Require(requested.load(), "late event preserves request");
    s.game.IsEventView = false;
    s.Tick();
    s.pawn.Active();
}

void RecycledManagerCallbackRejected() {
    Scene s;
    s.Select();
    const auto oldIdentity = Identify(&s.manager);
    ControlBefore(&s.manager);
    ++s.manager.Name;
    ControlAfter(&s.manager, oldIdentity, 1.f / 60);
    s.pawn.Native();
    Require(!binding.valid && TestVisibility::applies == 0, "recycled callback identity blocks writes");
    s.Tick();
    s.pawn.Active();
}

void NewerNativeValuesRetained() {
    Scene old;
    old.Start();
    old.pawn.arm.TargetArmLength = 333;
    old.pawn.camera.FieldOfView = 88;
    old.pawn.arm.TargetOffset.Z = 7;
    old.pawn.object.NearCameraZOffset = 9;
    Scene next(old.local);
    next.Select();
    next.Tick();
    Require(old.pawn.arm.TargetArmLength == 333 && old.pawn.camera.FieldOfView == 88 &&
        old.pawn.arm.TargetOffset.Z == 7 && old.pawn.object.NearCameraZOffset == 9,
        "restoration retains newer native state instead of old saved values");
    next.pawn.Active();
}

void LiveDetachedComponentsRemainPending() {
    Scene s;
    s.Start();
    SDK::UCameraComponent replacement;
    Register(replacement, "CameraComponent", &s.world);
    replacement.AttachParent = &s.pawn.arm;
    s.pawn.object.Camera = &replacement;
    s.Tick();
    Require(restorationPending && fovOwned && replacement.FieldOfView == 75, "live old component with unresolved owner blocks new writes");
    // Explicit destruction is sufficient proof to retire the old snapshot.
    s.pawn.camera.Flags |= 0x8000;
    s.Tick();
    Require(!restorationPending && replacement.FieldOfView == 110 && requested.load(), "retired component permits safe new binding");
}

void InvalidValuesDisarm() {
    Scene s;
    s.Select();
    s.pawn.camera.FieldOfView = std::numeric_limits<float>::quiet_NaN();
    s.Tick();
    Require(!requested.load() && !binding.valid && !applied, "invalid values must still disarm");
    Require(TestVisibility::owner == nullptr, "invalid-value path restores preceding visibility write");
    s.pawn.camera.FieldOfView = 75;
    s.Tick();
    s.pawn.Native();
}

void VisibilityFailureDisarms() {
    Scene s;
    s.Select();
    TestVisibility::failApply = true;
    s.Tick();
    s.pawn.Native();
    Require(!requested.load() && !binding.valid && !restorationPending, "visibility failure must still disarm and restore");
    Require(TestVisibility::owner == nullptr, "partial visibility write must be restored");
}

int main() {
    const std::vector<std::pair<const char*, void(*)()>> tests{
        {"startup and F3 off survives next map", StartupAndManualOff},
        {"manager takeover restores before new camera writes; stale manager ignored", ManagerTakeover},
        {"same manager with new pawn resumes", SameManagerNewPawn},
        {"temporary invalid context restores and resumes", TemporaryInvalidContext},
        {"pending old arm restores independently before next map applies", PendingArmRestore},
        {"visibility pending and F3 off during transition", PendingVisibilityAndF3Off},
        {"garden pending alone blocks new map and preserves F3 off", PendingGardenAndF3Off},
        {"dash pending alone blocks new map and preserves F3 off", PendingDashAndF3Off},
        {"unrelated valid/invalid manager cannot cancel binding", UnrelatedManager},
        {"retired local player allows new local takeover", RetiredLocalAllowsTakeover},
        {"ten menu/event/map/view guards restore and resume", BlockedViewsResume},
        {"event begins during native callback", BlockArisesDuringNativeCallback},
        {"recycled manager rejected after native callback", RecycledManagerCallbackRejected},
        {"newer native camera values survive restoration", NewerNativeValuesRetained},
        {"live detached component blocks until retirement", LiveDetachedComponentsRemainPending},
        {"invalid camera values still disarm", InvalidValuesDisarm},
        {"visibility failure still disarms and restores", VisibilityFailureDisarms}
    };
    unsigned failed = 0;
    for (const auto& [name, test] : tests) {
        Reset();
        try { test(); std::printf("PASS %s\n", name); }
        catch (const std::exception& e) { ++failed; std::printf("FAIL %s: %s\n", name, e.what()); }
    }
    std::printf("%zu scenarios, %u failures\n", tests.size(), failed);
    return failed ? 1 : 0;
}
