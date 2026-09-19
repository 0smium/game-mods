// Included after CameraContext/ViewBlock. All calls must stay inside the
// guarded native camera callback on its observed game thread.
// Only render ownership/shadow flags are changed, via native UE setters.

namespace MeshVisibility {
struct Entry {
    ObjectIdentity mesh;
    bool ownerOwned{};
    bool shadowOwned{};
    bool originalOwner{};
    bool originalShadow{};
};
std::vector<Entry> entries;
ObjectIdentity pawnIdentity, worldIdentity;
bool restoring = false;
ULONGLONG lastFailureLog{};

bool Failure(const char* reason) {
    const auto now = GetTickCount64();
    if (now - lastFailureLog >= 2000) {
        Log("VISIBILITY_BLOCK reason=%s; no unverified mesh or actor writes", reason);
        lastFailureLog = now;
    }
    return false;
}

template<class T>
bool ArrayReadable(const SDK::TArray<T>& array, int limit) {
    return array.Num() >= 0 && array.Num() <= limit && array.Max() >= array.Num() &&
        (!array.Num() || Readable(array.GetDataPtr(), sizeof(T) * array.Num()));
}

// An attachment by itself does not establish UE's view-owner relationship.
// Require the component's actual Outer Actor/Owner chain to reach our pawn.
bool BelongsTo(SDK::UObject* component, SDK::APlayerBase_C* pawn, SDK::UWorld* world) {
    if (!IndexedObject(component) || OwningWorld(component) != world) return false;
    SDK::UObject* outer = component->Outer;
    SDK::AActor* actor{};
    for (unsigned n = 0; outer && n < 12; ++n) {
        if (!IndexedObject(outer)) return false;
        if (HasClass(outer, "Actor")) { actor = reinterpret_cast<SDK::AActor*>(outer); break; }
        outer = outer->Outer;
    }
    for (unsigned n = 0; actor && n < 8; ++n) {
        if (!IndexedObject(actor) || !Readable(actor, sizeof(SDK::AActor))) return false;
        if (actor == pawn) return true;
        actor = actor->Owner;
    }
    return false;
}

SDK::UFunction* NativeBoolFunction(SDK::UObject* component, const char* functionName, const char* parameterName) {
    auto type = reinterpret_cast<SDK::UStruct*>(component->Class);
    for (unsigned depth = 0; type && depth < 24; ++depth) {
        if (!IndexedObject(type) || !Readable(type, sizeof(SDK::UStruct))) return nullptr;
        if (Name(type) == "PrimitiveComponent") {
            auto field = type->Children;
            for (unsigned count = 0; field && count < 1024; ++count) {
                if (!IndexedObject(field) || !Readable(field, sizeof(SDK::UField))) return nullptr;
                if (Name(field) == functionName) {
                    if (!HasClass(field, "Function") || field->Outer != type || !Readable(field, sizeof(SDK::UFunction))) return nullptr;
                    const auto function = reinterpret_cast<SDK::UFunction*>(field);
                    if (!(function->FunctionFlags & 0x400) || !Executable(reinterpret_cast<void*>(function->ExecFunction))) return nullptr;
                    const auto parameter = reinterpret_cast<SDK::FBoolProperty*>(function->ChildProperties);
                    if (!Readable(parameter, sizeof(*parameter)) || !Readable(parameter->ClassPrivate, sizeof(SDK::FFieldClass)) ||
                        parameter->ClassPrivate->Name.ToString() != "BoolProperty" || parameter->Name.ToString() != parameterName ||
                        parameter->Next || parameter->ArrayDim != 1 || parameter->ElementSize != 1 || parameter->Offset != 0 ||
                        parameter->ByteOffset != 0 || parameter->FieldSize != 1 || parameter->FieldMask != 0xff ||
                        !(parameter->PropertyFlags & 0x80) || (parameter->PropertyFlags & (0x100 | 0x400))) return nullptr;
                    return function;
                }
                field = field->Next;
            }
            return nullptr;
        }
        type = type->Super;
    }
    return nullptr;
}

bool SetFlag(SDK::UMeshComponent* mesh, bool ownerFlag, bool value) {
    if (!insideProbe || sampleThread.load() != GetCurrentThreadId() || !IndexedObject(mesh) ||
        !Readable(mesh, sizeof(SDK::UMeshComponent))) return Failure("setter-thread-or-object");
    const auto function = NativeBoolFunction(mesh, ownerFlag ? "SetOwnerNoSee" : "SetCastHiddenShadow",
        ownerFlag ? "bNewOwnerNoSee" : "NewCastHiddenShadow");
    if (!function) return Failure("native-bool-signature-unresolved");
    // Invoke the actual engine UFunction through the signature-scanned original
    // ProcessEvent. Do not use a hardcoded vtable index or mutate FunctionFlags.
    struct BoolParameter { bool value; } parameter{value};
    processEventHook.unsafe_call<void>(mesh, function, &parameter);
    return (ownerFlag ? static_cast<bool>(mesh->bOwnerNoSee) : static_cast<bool>(mesh->bCastHiddenShadow)) == value;
}

bool Gather(const CameraContext& context, std::vector<SDK::UMeshComponent*>& meshes) {
    std::vector<SDK::USceneComponent*> queue;
    std::unordered_set<SDK::USceneComponent*> seen;
    if (context.pawn->RootComponent) queue.push_back(context.pawn->RootComponent);
    if (context.pawn->Mesh) queue.push_back(context.pawn->Mesh);
    if (!ArrayReadable(context.pawn->CreatedSkelMeshes, 128) || !ArrayReadable(context.pawn->CreatedSkelActors, 64)) return false;
    for (int i = 0; i < context.pawn->CreatedSkelMeshes.Num(); ++i)
        if (context.pawn->CreatedSkelMeshes[i]) queue.push_back(context.pawn->CreatedSkelMeshes[i]);
    for (int i = 0; i < context.pawn->CreatedSkelActors.Num(); ++i) {
        const auto actor = context.pawn->CreatedSkelActors[i];
        if (IndexedObject(actor) && Readable(actor, sizeof(SDK::AActor)) && actor->RootComponent)
            queue.push_back(actor->RootComponent);
    }
    for (size_t i = 0; i < queue.size(); ++i) {
        if (queue.size() > 512) return false;
        const auto scene = queue[i];
        if (!scene || !seen.insert(scene).second) continue;
        if (!IndexedObject(scene) || !HasClass(scene, "SceneComponent") || !Readable(scene, sizeof(*scene))) return false;
        // Never traverse a neighbouring actor which is not owned by this pawn.
        if (!BelongsTo(scene, context.pawn, context.world)) continue;
        if (HasClass(scene, "MeshComponent")) {
            if (!Readable(scene, sizeof(SDK::UMeshComponent)) || meshes.size() >= 128) return false;
            meshes.push_back(reinterpret_cast<SDK::UMeshComponent*>(scene));
        }
        if (!ArrayReadable(scene->AttachChildren, 256)) return false;
        for (int child = 0; child < scene->AttachChildren.Num(); ++child)
            if (scene->AttachChildren[child]) queue.push_back(scene->AttachChildren[child]);
    }
    return !meshes.empty();
}

bool Owned() {
    for (const auto& entry : entries) if (entry.ownerOwned || entry.shadowOwned) return true;
    return false;
}
} // namespace MeshVisibility

// Pending means a requested restore is incomplete, not normal active ownership.
bool VisibilityPending() { return MeshVisibility::restoring && MeshVisibility::Owned(); }

bool VisibilityRestore(const char* reason) {
    using namespace MeshVisibility;
    restoring = true;
    SDK::UObject* pawnObject{};
    SDK::UObject* worldObject{};
    const bool ownersLive = Resolve(pawnIdentity, pawnObject) == ObjectState::Live &&
        Resolve(worldIdentity, worldObject) == ObjectState::Live && Readable(pawnObject, sizeof(SDK::APlayerBase_C));
    for (auto& entry : entries) {
        if (!entry.ownerOwned && !entry.shadowOwned) continue;
        SDK::UObject* object{};
        const auto state = Resolve(entry.mesh, object);
        if (state == ObjectState::Retired) { entry.ownerOwned = entry.shadowOwned = false; continue; }
        if (state != ObjectState::Live || !ownersLive || !Readable(object, sizeof(SDK::UMeshComponent)) ||
            !BelongsTo(object, reinterpret_cast<SDK::APlayerBase_C*>(pawnObject), reinterpret_cast<SDK::UWorld*>(worldObject))) continue;
        const auto mesh = reinterpret_cast<SDK::UMeshComponent*>(object);
        // Clear our view-owner flag first so third-person visibility returns
        // before the optional hidden-shadow override is removed.
        if (entry.ownerOwned && (!mesh->bOwnerNoSee || SetFlag(mesh, true, entry.originalOwner))) entry.ownerOwned = false;
        if (entry.shadowOwned && (!mesh->bCastHiddenShadow || SetFlag(mesh, false, entry.originalShadow))) entry.shadowOwned = false;
    }
    if (Owned()) return Failure("restore-pending");
    if (!entries.empty()) Log("VISIBILITY_RESTORE reason=%s meshes=%zu; original flags restored or native newer state retained", reason, entries.size());
    entries.clear();
    pawnIdentity = {};
    worldIdentity = {};
    restoring = false;
    return true;
}

bool VisibilityApply(const CameraContext& context) {
    using namespace MeshVisibility;
    if (restoring && !VisibilityRestore("retry-before-apply")) return false;
    if (pawnIdentity.pointer && (!Matches(pawnIdentity, context.pawn) || !Matches(worldIdentity, context.world))) {
        if (!VisibilityRestore("pawn-world-changed")) return false;
    }
    std::vector<SDK::UMeshComponent*> meshes;
    if (!Gather(context, meshes)) return Failure("bounded-pawn-mesh-discovery");
    if (!pawnIdentity.pointer) {
        pawnIdentity = Identify(context.pawn);
        worldIdentity = Identify(context.world);
        if (!pawnIdentity.pointer || !worldIdentity.pointer) return Failure("pawn-world-identity");
    }
    for (const auto mesh : meshes) {
        Entry* entry{};
        for (auto& existing : entries) if (Matches(existing.mesh, mesh)) { entry = &existing; break; }
        if (!entry) {
            if (entries.size() >= 256) return Failure("mesh-lifecycle-budget");
            entries.push_back({Identify(mesh)});
            entry = &entries.back();
            if (!entry->mesh.pointer) return Failure("mesh-identity");
            Log("VISIBILITY_MESH name=%s type=%s ownerNoSee=%d castShadow=%d hiddenShadow=%d; collision/animation untouched",
                Name(mesh).c_str(), ClassName(mesh).c_str(), mesh->bOwnerNoSee, mesh->CastShadow, mesh->bCastHiddenShadow);
            if (HasClass(mesh, "SkinnedMeshComponent") && Readable(mesh, sizeof(SDK::USkinnedMeshComponent))) {
                const auto skinned = reinterpret_cast<SDK::USkinnedMeshComponent*>(mesh);
                Log("VISIBILITY_ANIMATION mesh=%s tickPolicy=%u updateRateOptimization=%d (observed, not modified)",
                    Name(mesh).c_str(), static_cast<unsigned>(skinned->VisibilityBasedAnimTickOption), skinned->bEnableUpdateRateOptimizations);
            }
        }
        if (mesh->CastShadow && !mesh->bCastHiddenShadow) {
            if (!entry->shadowOwned) { entry->originalShadow = false; entry->shadowOwned = true; }
            if (!SetFlag(mesh, false, true)) return Failure("set-hidden-shadow");
        }
        if (!mesh->bOwnerNoSee) {
            if (!entry->ownerOwned) { entry->originalOwner = false; entry->ownerOwned = true; }
            if (!SetFlag(mesh, true, true)) return Failure("set-owner-no-see");
        }
    }
    return true;
}
