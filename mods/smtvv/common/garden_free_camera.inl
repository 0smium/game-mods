// Native garden free-camera overlap has a dedicated, synchronous Blueprint
// delegate: cast OtherActor to GardenFreeCameraPawn, then enable demon dither.
// Only that delegate sees a null OtherActor. Its caller's input is restored in
// ProcessEvent's finally block. No collision, material, Tick or actor flag writes.
namespace GardenFreeCamera {
constexpr const char* BeginName = "BndEvt__BP_GardenDevil_GardenDevilFreeCameraCollision_K2Node_ComponentBoundEvent_0_ComponentBeginOverlapSignature__DelegateSignature";
struct CallScope {
    std::uint8_t* slot{};
    SDK::UObject* original{};
    bool active{};
};
ULONGLONG lastBlockLog{}, lastApplyLog{};

bool Block(const char* reason) {
    const auto now = GetTickCount64();
    if (now - lastBlockLog >= 2000) {
        Log("GARDEN_FREECAM_BLOCK reason=%s; native overlap retained", reason);
        lastBlockLog = now;
    }
    return false;
}

bool Signature(SDK::UObject* wrapper, SDK::UFunction* function) {
    if (!IndexedObject(function) || !Readable(function, sizeof(*function)) ||
        !HasClass(function, "Function") || function->Outer != wrapper->Class ||
        Name(function) != BeginName || function->Size != 0xa8 ||
        !(function->FunctionFlags & 0x08000000) || (function->FunctionFlags & 0x400) ||
        !Executable(reinterpret_cast<void*>(function->ExecFunction))) return false;
    struct Expected { const char* name; const char* kind; int offset, size; };
    constexpr Expected expected[] = {
        {"OverlappedComponent", "ObjectProperty", 0, 8},
        {"OtherActor", "ObjectProperty", 8, 8},
        {"OtherComp", "ObjectProperty", 16, 8},
        {"OtherBodyIndex", "IntProperty", 24, 4},
        {"bFromSweep", "BoolProperty", 28, 1},
        {"SweepResult", "StructProperty", 32, 0x88}
    };
    auto field = reinterpret_cast<SDK::FProperty*>(function->ChildProperties);
    for (const auto& item : expected) {
        if (!Readable(field, sizeof(*field)) || !Readable(field->ClassPrivate, sizeof(SDK::FFieldClass)) ||
            field->Name.ToString() != item.name || field->ClassPrivate->Name.ToString() != item.kind ||
            !field->Owner.bIsUObject || field->Owner.Container.Object != function ||
            field->ArrayDim != 1 || field->ElementSize != item.size || field->Offset != item.offset ||
            !(field->PropertyFlags & 0x80) || (field->PropertyFlags & 0x400)) return false;
        // Only OtherActor's input bytes are changed. Never reinterpret an out parameter.
        if (item.offset == 8 && (field->PropertyFlags & 0x100)) return false;
        field = reinterpret_cast<SDK::FProperty*>(field->Next);
    }
    return field == nullptr;
}

bool Context(SDK::UObject* wrapper, SDK::UObject* pawn, SDK::UObject* collision) {
    if (!IndexedObject(pawn) || ClassName(pawn) != "GardenFreeCameraPawn" || !HasClass(pawn, "Pawn")) return Block("other-actor-not-freecam");
    const auto world = OwningWorld(wrapper);
    if (!IndexedObject(world) || !Readable(world, sizeof(SDK::UWorld)) || OwningWorld(pawn) != world) return Block("world");
    if (!IndexedObject(collision) || collision->Outer != wrapper || !HasClass(collision, "CapsuleComponent") ||
        GardenDiagnostics::ObjectProperty(wrapper, "GardenDevilFreeCameraCollision") != collision ||
        GardenDiagnostics::ObjectProperty(wrapper, "GardenDevilCameraCollision") == collision ||
        GardenDiagnostics::ObjectProperty(wrapper, "GardenDevilCollision") == collision) return Block("collision-binding");
    double ready{};
    if (!GardenDiagnostics::Scalar(wrapper, "m_IsInitialiationDone", ready) || ready != 1) return Block("wrapper-not-ready");
    const auto devil = GardenDiagnostics::ObjectProperty(wrapper, "DevilBaseInstance");
    const auto garden = GardenDiagnostics::ObjectProperty(wrapper, "BP Garden Manager");
    const auto component = GardenDiagnostics::ObjectProperty(wrapper, "GardenDevilDitherComponent");
    if (!devil || !HasClass(devil, "CharaBase_C") || OwningWorld(devil) != world ||
        !garden || ClassName(garden) != "BP_GardenManager_C" || OwningWorld(garden) != world ||
        !GardenDiagnostics::Scalar(garden, "GardenDevilsReady", ready) || ready != 1 ||
        !component || ClassName(component) != "BP_GardenDevilDitherComponent_C" || component->Outer != wrapper ||
        GardenDiagnostics::ObjectProperty(component, "GardenDevilRef") != wrapper) return Block("garden-binding");
    const auto mode = world->AuthorityGameMode;
    const auto sublevels = GardenDiagnostics::ObjectProperty(mode, "MapSubLevelManager");
    if (!GardenDiagnostics::Scalar(sublevels, "IsGardenLoaded", ready) || ready != 1 ||
        !GardenDiagnostics::Scalar(sublevels, "IsEndLoadGarden", ready) || ready != 1) return Block("garden-loading");

    const auto game = world->OwningGameInstance;
    if (!IndexedObject(game) || !Readable(game, sizeof(SDK::UGameInstance)) || !HasClass(game, "ProjectGameInstance_C") ||
        !MeshVisibility::ArrayReadable(game->LocalPlayers, 4)) return Block("local-players");
    for (int i = 0; i < game->LocalPlayers.Num(); ++i) {
        const auto local = game->LocalPlayers[i];
        if (!IndexedObject(local) || !Readable(local, sizeof(SDK::ULocalPlayer))) continue;
        const auto viewport = local->ViewportClient;
        const auto controller = local->PlayerController;
        if (!IndexedObject(viewport) || !Readable(viewport, sizeof(SDK::UGameViewportClient)) || viewport->World != world ||
            !IndexedObject(controller) || !Readable(controller, sizeof(SDK::APlayerController)) ||
            ClassName(controller) != "BP_FreeCameraController_C" || controller->Player != local || OwningWorld(controller) != world) continue;
        // Native free camera is a SpectatorPawn; controller.Pawn is null in the
        // verified game. F3 and the normal player-camera binding are unrelated.
        if (GardenDiagnostics::ObjectProperty(controller, "SpectatorPawn") != pawn ||
            GardenDiagnostics::ObjectProperty(controller, "GardenFreeCameraPawnRef") != pawn ||
            GardenDiagnostics::ObjectProperty(pawn, "Controller") != controller) continue;
        const auto manager = GardenDiagnostics::ObjectProperty(garden, "BP_GardenFreeCameraManager");
        if (!manager || ClassName(manager) != "BP_GardenFreeCameraManager_C" || manager->Outer != garden ||
            GardenDiagnostics::ObjectProperty(manager, "GardenFreeCameraControllerRef") != controller ||
            GardenDiagnostics::ObjectProperty(controller, "BP Garden Free Camera Manager") != manager) continue;
        return true;
    }
    return Block("local-freecam-binding");
}
} // namespace GardenFreeCamera

bool GardenFreeCameraBefore(SDK::UObject* object, SDK::UFunction* function, void* parameters, GardenFreeCamera::CallScope& scope) {
    using namespace GardenFreeCamera;
    if (scope.active || !insideProbe || sampleThread.load() != GetCurrentThreadId() ||
        !object || !function || !object->Class) return false;
    // Most ProcessEvent calls have nothing to do with a garden demon. Cache
    // only immutable class FName keys, never a UObject address or live binding.
    thread_local std::unordered_map<std::uint64_t, bool> classes;
    std::uint64_t key{};
    std::memcpy(&key, &object->Class->Name, sizeof(key));
    auto known = classes.find(key);
    if (known == classes.end()) {
        if (classes.size() >= 4096) classes.clear();
        known = classes.emplace(key, ClassName(object) == "BP_GardenDevil_C").first;
    }
    if (!known->second || !IndexedObject(object) || !IndexedObject(function) ||
        !HasClass(object, "GardenDevil") || Name(function) != BeginName) return false;
    if (!Signature(object, function) || !Readable(parameters, 0xa8)) return Block("event-signature");
    auto bytes = static_cast<std::uint8_t*>(parameters);
    SDK::UObject* collision{};
    SDK::UObject* pawn{};
    std::memcpy(&collision, bytes, sizeof(collision));
    std::memcpy(&pawn, bytes + 8, sizeof(pawn));
    if (!Context(object, pawn, collision)) return false;
    // Claim the caller-owned parameter slot before changing it. The original
    // delegate only reads this input; restoration does not dereference a Pawn
    // that could have retired while the callback ran.
    scope = {bytes + 8, pawn, true};
    SDK::UObject* empty{};
    std::memcpy(scope.slot, &empty, sizeof(empty));
    const auto now = GetTickCount64();
    if (now - lastApplyLog >= 2000) {
        Log("GARDEN_FREECAM_SUPPRESS wrapper=%s pawn=%s; dedicated begin-overlap input only; native event still runs",
            Name(object).c_str(), Name(pawn).c_str());
        lastApplyLog = now;
    }
    return true;
}

void GardenFreeCameraAfter(GardenFreeCamera::CallScope& scope) {
    if (!scope.active) return;
    // Must be called in the same ProcessEvent frame's finally block. Restore
    // the input bytes even if the engine has retired an object. No UObject is
    // written, and no restore work is retained between events or frames.
    std::memcpy(scope.slot, &scope.original, sizeof(scope.original));
    scope = {};
}
