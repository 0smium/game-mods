// Included inside the probe's anonymous namespace after the identity helpers.
// Candidate v0.3 adds calibrated eye height, optional gait, and owned-mesh
// visibility. Native actor movement/collision and jump trajectories are intact.

struct ObjectIdentity {
    SDK::UObject* pointer{};
    SDK::UClass* type{};
    SDK::UObject* outer{};
    std::uint64_t name{};
    int index{-1};
};

bool IndexedObject(const SDK::UObject* object) {
    if (!ValidObject(object) || !objectTable || !Readable(objectTable, sizeof(*objectTable))) return false;
    const auto index = object->Index;
    if (objectTable->NumElements < 1 || objectTable->NumElements > 10000000 ||
        objectTable->NumChunks < 1 || objectTable->NumChunks > 1024 ||
        index < 0 || index >= objectTable->NumElements) return false;
    const auto chunk = index / SDK::TUObjectArray::ElementsPerChunk;
    const auto position = index % SDK::TUObjectArray::ElementsPerChunk;
    if (chunk >= objectTable->NumChunks || !Readable(objectTable->Objects + chunk, sizeof(void*))) return false;
    const auto entries = objectTable->Objects[chunk];
    return entries && Readable(entries + position, sizeof(SDK::FUObjectItem)) && entries[position].Object == object;
}

ObjectIdentity Identify(SDK::UObject* object) {
    ObjectIdentity identity{};
    if (!IndexedObject(object)) return identity;
    identity.pointer = object;
    identity.type = object->Class;
    identity.outer = object->Outer;
    identity.index = object->Index;
    std::memcpy(&identity.name, &object->Name, sizeof(identity.name));
    return identity;
}

bool Matches(const ObjectIdentity& identity, const SDK::UObject* object) {
    if (!identity.pointer || identity.pointer != object || !IndexedObject(object)) return false;
    std::uint64_t name{};
    std::memcpy(&name, &object->Name, sizeof(name));
    return identity.index == object->Index && identity.type == object->Class &&
        identity.outer == object->Outer && identity.name == name;
}

enum class ObjectState { Unresolved, Live, Retired };

// Resolve saved indices before touching saved pointers. A temporarily unreadable
// array or a changed ownership link is not proof that an object was destroyed.
ObjectState Resolve(const ObjectIdentity& saved, SDK::UObject*& object) {
    object = nullptr;
    if (!saved.pointer || !objectTable || !Readable(objectTable, sizeof(*objectTable)) ||
        objectTable->NumElements < 1 || objectTable->NumElements > 10000000 ||
        objectTable->NumElements > objectTable->MaxElements || objectTable->NumChunks < 1 ||
        objectTable->NumChunks > 1024 || objectTable->NumChunks > objectTable->MaxChunks) return ObjectState::Unresolved;
    if (saved.index < 0 || saved.index >= objectTable->NumElements) return ObjectState::Retired;
    const auto chunkIndex = saved.index / SDK::TUObjectArray::ElementsPerChunk;
    const auto position = saved.index % SDK::TUObjectArray::ElementsPerChunk;
    if (chunkIndex >= objectTable->NumChunks || !Readable(objectTable->Objects + chunkIndex, sizeof(void*))) return ObjectState::Unresolved;
    const auto chunk = objectTable->Objects[chunkIndex];
    if (!chunk || !Readable(chunk + position, sizeof(SDK::FUObjectItem))) return ObjectState::Unresolved;
    object = chunk[position].Object;
    if (!object || object != saved.pointer) return ObjectState::Retired;
    if (!Readable(object, sizeof(SDK::UObject))) return ObjectState::Unresolved;
    if (static_cast<std::uint32_t>(object->Flags) & (0x8000 | 0x10000)) return ObjectState::Retired;
    std::uint64_t name{};
    std::memcpy(&name, &object->Name, sizeof(name));
    if (object->Index != saved.index || object->Class != saved.type || name != saved.name) return ObjectState::Retired;
    if (object->Outer != saved.outer || !ValidObject(object)) return ObjectState::Unresolved;
    return ObjectState::Live;
}

struct CameraContext {
    SDK::AProjectPlayerCameraManager_C* manager{};
    SDK::APlayerController* controller{};
    SDK::APlayerBase_C* pawn{};
    SDK::UCameraComponent* camera{};
    SDK::UCustomSpringArmComponent* arm{};
    SDK::UWorld* world{};
    SDK::UProjectGameInstance_C* game{};
};

const char* contextFailure = "identity-unresolved";
bool RejectContext(const char* reason) { contextFailure = reason; return false; }

bool Context(SDK::AProjectPlayerCameraManager_C* manager, CameraContext& out) {
    if (!IndexedObject(manager) || !Readable(manager, sizeof(*manager))) return RejectContext("manager-array-identity");
    const auto pc = manager->PCOwner;
    if (!IndexedObject(pc) || !Readable(pc, sizeof(*pc)) || pc->PlayerCameraManager != manager) return RejectContext("controller-identity");
    if (!IndexedObject(pc->Player) || !HasClass(pc->Player, "LocalPlayer")) return RejectContext("local-player-class");
    const auto local = reinterpret_cast<SDK::ULocalPlayer*>(pc->Player);
    if (!Readable(local, sizeof(*local)) || local->PlayerController != pc) return RejectContext("local-player-owner");
    const auto pawn = pc->Pawn;
    if (!IndexedObject(pawn) || !HasClass(pawn, "PlayerBase_C") || !Readable(pawn, sizeof(SDK::APlayerBase_C))) return RejectContext("pawn-identity");
    const auto player = reinterpret_cast<SDK::APlayerBase_C*>(pawn);
    const auto world = OwningWorld(manager);
    if (!IndexedObject(world) || !Readable(world, sizeof(*world)) || OwningWorld(player) != world) return RejectContext("world-identity");
    if (!IndexedObject(local->ViewportClient) || !Readable(local->ViewportClient, sizeof(SDK::UGameViewportClient)) || local->ViewportClient->World != world) return RejectContext("viewport-world");
    const auto instance = world->OwningGameInstance;
    if (!IndexedObject(instance) || !HasClass(instance, "ProjectGameInstance_C") || !Readable(instance, sizeof(SDK::UProjectGameInstance_C))) return RejectContext("game-instance");
    if (instance->LocalPlayers.Num() < 1 || instance->LocalPlayers.Num() > 4) return RejectContext("local-player-count");
    bool belongsToInstance = false;
    for (int i = 0; i < instance->LocalPlayers.Num(); ++i) belongsToInstance |= instance->LocalPlayers[i] == local;
    if (!belongsToInstance) return RejectContext("local-player-membership");
    const auto camera = player->Camera;
    const auto arm = player->CustomSpringArm;
    if (!IndexedObject(camera) || !Readable(camera, sizeof(*camera)) || !HasClass(camera, "CameraComponent") ||
        !IndexedObject(arm) || !Readable(arm, sizeof(*arm)) || !HasClass(arm, "CustomSpringArmComponent")) return RejectContext("pawn-camera-components");
    if (camera->AttachParent != arm || OwningWorld(camera) != world || OwningWorld(arm) != world) return RejectContext("component-attachment-world");
    out = { manager, pc, player, camera, arm, world, reinterpret_cast<SDK::UProjectGameInstance_C*>(instance) };
    return true;
}

const char* ViewBlock(const CameraContext& c) {
    if (c.manager->ViewTarget.target != c.pawn) return "view-target";
    if (c.manager->PendingViewTarget.target) return "view-blend";
    if (c.manager->ForceHidden) return "native-force-hidden";
    if (c.manager->ForceCounter != 0) return "scripted-camera";
    if (c.game->IsMapChange || c.game->IsLoadMap || c.game->IsMapStartLoad) return "map-transition";
    if (c.game->IsEventView || c.game->IsEventView_Simple) return "event-camera";
    // IsEncount and IsLoadLevel were both true during validated ordinary field
    // play. They are logged only; their names do not establish their semantics.
    if (c.game->m_BattleState != 0) return "unobserved-battle-state";
    return nullptr;
}

#include "visibility_control.inl"
#include "camera_motion.inl"

struct CameraBinding {
    ObjectIdentity manager, controller, pawn, camera, arm, world, localPlayer;
    bool valid{};
};

CameraBinding binding;
bool applied = false;
bool lengthOwned = false;
bool fovOwned = false;
bool targetZOwned = false;
bool targetZSuppressed = false;
bool nearOffsetOwned = false;
bool restorationPending = false;
bool discardAfterRestore = false;
float originalLength{};
float originalFov{};
float lastWrittenFov{};
float originalTargetZ{};
float lastWrittenTargetZ{};
float originalNearOffset{};
const char* lastBlock = nullptr;

bool BoundTo(const CameraContext& c) {
    return binding.valid && Matches(binding.manager, c.manager) && Matches(binding.controller, c.controller) &&
        Matches(binding.pawn, c.pawn) && Matches(binding.camera, c.camera) && Matches(binding.arm, c.arm) && Matches(binding.world, c.world) &&
        Matches(binding.localPlayer, c.controller->Player);
}

const char* HeightCalibrationBlock(const CameraContext& c, float& baseOffset) {
    const auto root = c.pawn->RootComponent;
    const auto cameraRoot = c.pawn->CameraRoot;
    if (!IndexedObject(root) || !Readable(root, sizeof(SDK::USceneComponent)) ||
        !IndexedObject(cameraRoot) || !Readable(cameraRoot, sizeof(SDK::USceneComponent))) return "height root identity unresolved";
    if (root->AttachParent || cameraRoot->AttachParent != root || c.arm->AttachParent != cameraRoot || c.camera->AttachParent != c.arm)
        return "height attachment chain differs";
    for (const auto component : std::array<SDK::USceneComponent*, 4>{root, cameraRoot, c.arm, c.camera}) {
        if (component->bAbsoluteLocation || component->bAbsoluteRotation || component->bAbsoluteScale)
            return "height chain uses absolute transform flags";
        const auto& r = component->RelativeRotation;
        const auto& s = component->RelativeScale3D;
        if (!(std::abs(r.Pitch) < .1f && std::abs(r.Roll) < .1f &&
            std::abs(s.X - 1) < .001f && std::abs(s.Y - 1) < .001f && std::abs(s.Z - 1) < .001f))
            return "height chain has rotation or non-unit scale";
    }
    if (std::abs(originalTargetZ) > .01f) return "nonzero native targetZ: calibration skipped";
    const auto& socket = c.arm->SocketOffset;
    if (!(std::abs(socket.X) < .001f && std::abs(socket.Y) < .001f && std::abs(socket.Z) < .001f))
        return "nonzero socket offset: calibration skipped";
    if (!std::isfinite(c.pawn->BaseEyeHeight) || c.pawn->BaseEyeHeight < 1 || c.pawn->BaseEyeHeight > 300 ||
        !std::isfinite(cameraRoot->RelativeLocation.Z) || !std::isfinite(c.arm->RelativeLocation.Z)) return "invalid native eye-height parameters";
    baseOffset = c.pawn->BaseEyeHeight - (cameraRoot->RelativeLocation.Z + c.arm->RelativeLocation.Z);
    if (!std::isfinite(baseOffset) || std::abs(baseOffset) > 300) return "height correction outside calibration range";
    return nullptr;
}

void RestoreCurrentHeight(const CameraContext& c, const char* reason) {
    if (!BoundTo(c)) return;
    if (targetZOwned) {
        if (c.arm->TargetOffset.Z == lastWrittenTargetZ) c.arm->TargetOffset.Z = originalTargetZ;
        targetZOwned = false;
    }
    if (nearOffsetOwned) {
        if (c.pawn->NearCameraZOffset == 0.0f) c.pawn->NearCameraZOffset = originalNearOffset;
        nearOffsetOwned = false;
    }
    targetZSuppressed = true;
    ResetGait();
    Log("HEIGHT_PAUSE reason=%s; native height values restored if still owned", reason);
}

void Bind(const CameraContext& c) {
    binding = { Identify(c.manager), Identify(c.controller), Identify(c.pawn), Identify(c.camera), Identify(c.arm), Identify(c.world), Identify(c.controller->Player), true };
}

// Restoration deliberately does not require the old controller to still own
// the pawn: ownership replacement is precisely when restoration is necessary.
// Each field is restored/retired independently and unresolved work stays queued.
bool RestoreBoundIfStillIdentical(const char* reason) {
    const bool gardenComplete = GardenVisibilityRestore(reason);
    const bool dashComplete = DashEffectRestore(reason);
    const bool visibilityComplete = VisibilityRestore(reason);
    SDK::UObject* worldObject{};
    SDK::UObject* pawnObject{};
    const bool worldLive = Resolve(binding.world, worldObject) == ObjectState::Live;
    const bool pawnLive = Resolve(binding.pawn, pawnObject) == ObjectState::Live;
    const bool ownership = worldLive && pawnLive && Readable(pawnObject, sizeof(SDK::APlayerBase_C)) &&
        OwningWorld(pawnObject) == worldObject;
    const auto pawn = ownership ? reinterpret_cast<SDK::APlayerBase_C*>(pawnObject) : nullptr;
    if (nearOffsetOwned) {
        if (Resolve(binding.pawn, pawnObject) == ObjectState::Retired) {
            nearOffsetOwned = false;
            Log("RETIRE near-height snapshot: original pawn replaced or destroyed");
        } else if (pawn) {
            const bool ours = pawn->NearCameraZOffset == 0.0f;
            if (ours) pawn->NearCameraZOffset = originalNearOffset;
            nearOffsetOwned = false;
            Log("RESTORE reason=%s field=near-height written=%d original=%.3f", reason, ours, originalNearOffset);
        }
    }
    if (lengthOwned || targetZOwned) {
        SDK::UObject* object{};
        const auto state = Resolve(binding.arm, object);
        if (state == ObjectState::Retired) {
            Log("RETIRE arm snapshot: original object slot replaced or destruction flagged; no write");
            lengthOwned = false;
            targetZOwned = false;
        } else if (state == ObjectState::Live && pawn && Readable(object, sizeof(SDK::UCustomSpringArmComponent)) &&
            pawn->CustomSpringArm == object && OwningWorld(object) == worldObject) {
            const auto arm = reinterpret_cast<SDK::UCustomSpringArmComponent*>(object);
            if (lengthOwned) {
                const bool ours = arm->TargetArmLength == 0.0f;
                if (ours) arm->TargetArmLength = originalLength;
                lengthOwned = false;
                Log("RESTORE reason=%s field=arm written=%d original=%.3f (newer native values are retained)", reason, ours, originalLength);
            }
            if (targetZOwned) {
                const bool ours = arm->TargetOffset.Z == lastWrittenTargetZ;
                if (ours) arm->TargetOffset.Z = originalTargetZ;
                targetZOwned = false;
                Log("RESTORE reason=%s field=targetZ written=%d original=%.3f", reason, ours, originalTargetZ);
            }
        }
    }
    if (fovOwned) {
        SDK::UObject* object{};
        const auto state = Resolve(binding.camera, object);
        if (state == ObjectState::Retired) {
            Log("RETIRE FOV snapshot: original object slot replaced or destruction flagged; no write");
            fovOwned = false;
        } else if (state == ObjectState::Live && pawn && Readable(object, sizeof(SDK::UCameraComponent)) &&
            pawn->Camera == object && OwningWorld(object) == worldObject) {
            const auto camera = reinterpret_cast<SDK::UCameraComponent*>(object);
            const bool ours = camera->FieldOfView == lastWrittenFov;
            if (ours) camera->FieldOfView = originalFov;
            fovOwned = false;
            Log("RESTORE reason=%s field=fov written=%d original=%.3f (newer native values are retained)", reason, ours, originalFov);
        }
    }
    applied = lengthOwned || fovOwned || targetZOwned || nearOffsetOwned;
    if (applied || !visibilityComplete || !gardenComplete || !dashComplete) {
        publishedState.store("RESTORE_PENDING: new writes blocked");
        static ULONGLONG lastPendingLog{};
        const auto now = GetTickCount64();
        if (now - lastPendingLog >= 2000) {
            Log("RESTORE_PENDING reason=%s arm=%d fov=%d targetZ=%d nearHeight=%d visibility=%d garden=%d dash=%d; snapshot retained and new writes blocked", reason, lengthOwned, fovOwned, targetZOwned, nearOffsetOwned, !visibilityComplete, !gardenComplete, !dashComplete);
            lastPendingLog = now;
        }
        return false;
    }
    restorationPending = false;
    if (discardAfterRestore) { binding = {}; discardAfterRestore = false; }
    return true;
}

void BeginRestore(const char* reason, bool discard) {
    ResetGait();
    publishedState.store(requested.load() ? reason : "OFF / restoring native camera");
    restorationPending = true;
    discardAfterRestore |= discard;
    RestoreBoundIfStillIdentical(reason);
}

void ControlBefore(SDK::AProjectPlayerCameraManager_C* manager) {
    if (!requested.load() && !binding.valid) publishedState.store("OFF");
    CameraContext context{};
    const bool contextValid = Context(manager, context);
    if (binding.valid && !Matches(binding.manager, manager)) {
        // An unrelated camera instance must never cancel an active binding.
        // A new fully validated manager for the same local player is a real
        // takeover; restore the old components independently before releasing it.
        SDK::UObject* oldLocal{};
        const bool oldLocalRetired = Resolve(binding.localPlayer, oldLocal) == ObjectState::Retired;
        if (!contextValid || (!Matches(binding.localPlayer, context.controller->Player) && !oldLocalRetired)) return;
        Log("REBIND local-manager-replaced requested=%d; restoring old identity before automatic resume", requested.load());
        BeginRestore("local-manager-replaced", true);
        return;
    }
    if (binding.valid && restorationPending) {
        if (!RestoreBoundIfStillIdentical("retry")) return;
    }
    if (binding.valid && !requested.load()) {
        BeginRestore("F3-off", true);
        lastBlock = nullptr;
        return;
    }
    if (!contextValid) {
        if (requested.load() && lastBlock != contextFailure) {
            publishedState.store(contextFailure);
            Log("BLOCK reason=%s; identity chain not proven, no new camera writes", contextFailure);
            lastBlock = contextFailure;
        }
        if (binding.valid) {
            BeginRestore("context-temporarily-unavailable", false);
        }
        return;
    }
    if (binding.valid && !BoundTo(context)) {
        Log("REBIND world/pawn/component identity changed requested=%d; restoring old identity before automatic resume", requested.load());
        BeginRestore("bound-identity-changed", true);
        return;
    }
    if (!requested.load()) {
        lastBlock = nullptr;
        return;
    }
    const auto block = ViewBlock(context);
    if (block) {
        publishedState.store(block);
        if (binding.valid) BeginRestore(block, false);
        if (lastBlock != block) Log("PAUSE reason=%s; native camera retained", block);
        lastBlock = block;
    }
}

void ControlAfter(SDK::AProjectPlayerCameraManager_C* manager, const ObjectIdentity& callbackIdentity, float dt) {
    if (!requested.load() || restorationPending || !Matches(callbackIdentity, manager)) return;
    CameraContext context{};
    if (!Context(manager, context)) return;
    if (binding.valid && !BoundTo(context)) return;
    const auto block = ViewBlock(context);
    if (block) {
        publishedState.store(block);
        if (binding.valid) BeginRestore(block, false);
        if (lastBlock != block) Log("PAUSE reason=%s; native camera retained", block);
        lastBlock = block;
        return;
    }
    if (!binding.valid) Bind(context);
    if (!VisibilityApply(context)) {
        requested.store(false);
        BeginRestore("visibility-apply-failed", true);
        return;
    }
    const auto settings = Settings();
    const float length = context.arm->TargetArmLength;
    const float fov = context.camera->FieldOfView;
    if (!std::isfinite(length) || length < 0 || length > 10000 || !std::isfinite(fov) || fov <= 1 || fov >= 179 ||
        !std::isfinite(context.arm->TargetOffset.Z) || std::abs(context.arm->TargetOffset.Z) > 10000) {
        requested.store(false);
        BeginRestore("invalid-camera-values", true);
        return;
    }
    if (!applied) {
        originalLength = length;
        originalFov = fov;
        originalTargetZ = context.arm->TargetOffset.Z;
        targetZSuppressed = false;
        ResetGait();
        Log("APPLY near-eye arm=%.3f->0 fov=%.3f->%.0f eyeOffset=%.1f gait=%.1f targetZbase=%.3f; pawn=%s camera=%p arm=%p thread=%lu",
            originalLength, originalFov, settings.fov, settings.eye, settings.gait, originalTargetZ, Name(context.pawn).c_str(), context.camera, context.arm, GetCurrentThreadId());
        applied = true;
        lengthOwned = true;
        fovOwned = true;
    }
    if (targetZOwned && context.arm->TargetOffset.Z != lastWrittenTargetZ) {
        RestoreCurrentHeight(context, "native TargetOffset.Z takeover");
    }
    if (nearOffsetOwned && context.pawn->NearCameraZOffset != 0.0f) {
        RestoreCurrentHeight(context, "native NearCameraZOffset takeover");
    }
    context.arm->TargetArmLength = 0.0f;
    lastWrittenFov = settings.fov;
    context.camera->FieldOfView = lastWrittenFov;
    if (!targetZSuppressed) {
        float baseOffset{};
        const auto heightBlock = HeightCalibrationBlock(context, baseOffset);
        if (heightBlock) RestoreCurrentHeight(context, heightBlock);
        else if (!std::isfinite(context.pawn->NearCameraZOffset) || std::abs(context.pawn->NearCameraZOffset) > 300)
            RestoreCurrentHeight(context, "native near-height value outside range");
        else {
            if (!nearOffsetOwned) {
                originalNearOffset = context.pawn->NearCameraZOffset;
                nearOffsetOwned = true;
                Log("HEIGHT_CALIBRATION BaseEyeHeight=%.3f baseCorrection=%.3f NearCameraZOffset=%.3f->0; native Camera.RelativeZ converges without forced transform", context.pawn->BaseEyeHeight, baseOffset, originalNearOffset);
            }
            context.pawn->NearCameraZOffset = 0.0f;
            const auto bob = GaitOffset(context, dt, settings.gait);
            lastWrittenTargetZ = originalTargetZ + baseOffset + settings.eye + bob;
            targetZOwned = true;
            context.arm->TargetOffset.Z = lastWrittenTargetZ;
        }
    }
    publishedState.store(targetZSuppressed ? "ACTIVE; height calibration paused (toggle F3 to retry; see log)" : "ACTIVE / eye height calibrated");
    DashEffectUpdate(context);
    lastBlock = nullptr;
}
