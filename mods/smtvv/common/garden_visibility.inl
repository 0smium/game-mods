// The native GardenManager "Tick Hit" sphere trace calls TempHidden_Garden.
// That function has a CanBeHidden guard, then owns its own hide/restore timeline.
// Temporarily change the permission only around GardenManager.ReceiveTick,
// whose verified synchronous body is Tick Hit. Restore before other events run.
// Never force ActorHidden, material opacity, collision, particles, or timelines.
namespace GardenVisibility {
struct Entry {
    ObjectIdentity wrapper, devil, manager, world;
    bool original{};
    bool owned{};
};
std::vector<Entry> entries;
ULONGLONG lastFailure{}, lastApplyLog{};
bool restoring{};
bool scopeActive{};

void Failure(const char* reason) {
    const auto now = GetTickCount64();
    if (now - lastFailure >= 2000) {
        Log("GARDEN_VISIBILITY_BLOCK reason=%s; no actor-hidden/material/collision writes", reason);
        lastFailure = now;
    }
}

bool ReadPermission(SDK::UObject* wrapper, bool& value) {
    const auto property = GardenDiagnostics::FindProperty(wrapper, "CanBeHidden");
    if (property.offset < 0 || property.kind != "BoolProperty" || property.size != 1 ||
        property.byteOffset != 0 || property.mask != 0xff || !IndexedObject(wrapper) ||
        !Readable(wrapper->Class, sizeof(SDK::UClass)) || property.offset >= wrapper->Class->Size) return false;
    const auto address = reinterpret_cast<const std::uint8_t*>(wrapper) + property.offset;
    if (!Readable(address, 1) || *address > 1) return false;
    value = *address != 0;
    return true;
}

SDK::UFunction* Setter(SDK::UObject* wrapper) {
    if (!IndexedObject(wrapper) || ClassName(wrapper) != "BP_GardenDevil_C") return nullptr;
    const auto type = wrapper->Class;
    if (!IndexedObject(type) || !Readable(type, sizeof(SDK::UClass))) return nullptr;
    auto field = type->Children;
    for (unsigned n = 0; field && n < 256; ++n) {
        if (!IndexedObject(field) || !Readable(field, sizeof(SDK::UField))) return nullptr;
        if (Name(field) == "SetCanBeHiddenFlag") {
            if (field->Outer != type || !HasClass(field, "Function") || !Readable(field, sizeof(SDK::UFunction))) return nullptr;
            const auto function = reinterpret_cast<SDK::UFunction*>(field);
            // This is the verified Blueprint setter, not a native function.
            if (function->Size != 1 || !(function->FunctionFlags & 0x08000000) ||
                !Executable(reinterpret_cast<void*>(function->ExecFunction))) return nullptr;
            const auto parameter = reinterpret_cast<SDK::FBoolProperty*>(function->ChildProperties);
            if (!Readable(parameter, sizeof(*parameter)) || !Readable(parameter->ClassPrivate, sizeof(SDK::FFieldClass)) ||
                parameter->ClassPrivate->Name.ToString() != "BoolProperty" || parameter->Name.ToString() != "CanBeHidden" ||
                !parameter->Owner.bIsUObject || parameter->Owner.Container.Object != function ||
                parameter->Next || parameter->ArrayDim != 1 || parameter->ElementSize != 1 || parameter->Offset != 0 ||
                parameter->ByteOffset != 0 || parameter->FieldSize != 1 || parameter->FieldMask != 0xff ||
                !(parameter->PropertyFlags & 0x80) || (parameter->PropertyFlags & (0x100 | 0x400))) return nullptr;
            return function;
        }
        field = field->Next;
    }
    return nullptr;
}

bool SetPermission(SDK::UObject* wrapper, bool value) {
    if (!insideProbe || sampleThread.load() != GetCurrentThreadId()) return false;
    const auto identity = Identify(wrapper);
    const auto setter = Setter(wrapper);
    if (!identity.pointer || !setter) return false;
    struct Parameters { bool CanBeHidden; } parameters{value};
    processEventHook.unsafe_call<void>(wrapper, setter, &parameters);
    bool actual{};
    return Matches(identity, wrapper) && ReadPermission(wrapper, actual) && actual == value;
}

bool ReadyWrapper(SDK::UObject* wrapper, SDK::UWorld* world, SDK::UObject*& devil, SDK::UObject*& manager) {
    if (!IndexedObject(wrapper) || ClassName(wrapper) != "BP_GardenDevil_C" ||
        !HasClass(wrapper, "GardenDevil") || OwningWorld(wrapper) != world) return false;
    double ready{};
    if (!GardenDiagnostics::Scalar(wrapper, "m_IsInitialiationDone", ready) || ready != 1) return false;
    devil = GardenDiagnostics::ObjectProperty(wrapper, "DevilBaseInstance");
    manager = GardenDiagnostics::ObjectProperty(wrapper, "BP Garden Manager");
    return devil && HasClass(devil, "CharaBase_C") && OwningWorld(devil) == world &&
        manager && ClassName(manager) == "BP_GardenManager_C" && OwningWorld(manager) == world &&
        GardenDiagnostics::Scalar(manager, "GardenDevilsReady", ready) && ready == 1;
}

bool GardenArray(SDK::UObject* manager, SDK::TArray<SDK::UObject*>& output) {
    const auto type = manager->Class;
    if (!IndexedObject(type) || !Readable(type, sizeof(SDK::UStruct))) return false;
    auto field = type->ChildProperties;
    for (unsigned n = 0; field && n < 256; ++n) {
        if (!Readable(field, sizeof(SDK::FProperty)) || !Readable(field->ClassPrivate, sizeof(SDK::FFieldClass))) return false;
        if (field->Name.ToString() == "GardenDevils") {
            if (field->ClassPrivate->Name.ToString() != "ArrayProperty" || !Readable(field, sizeof(SDK::FArrayProperty))) return false;
            const auto property = reinterpret_cast<SDK::FArrayProperty*>(field);
            const auto inner = property->InnerProperty;
            if (!field->Owner.bIsUObject || field->Owner.Container.Object != type || property->ArrayDim != 1 ||
                property->ElementSize != sizeof(output) || property->Offset < 0 || property->Offset + sizeof(output) > static_cast<size_t>(type->Size) ||
                !Readable(inner, sizeof(SDK::FProperty)) || !Readable(inner->ClassPrivate, sizeof(SDK::FFieldClass)) ||
                inner->ClassPrivate->Name.ToString() != "ObjectProperty" || inner->ElementSize != sizeof(void*)) return false;
            const auto address = reinterpret_cast<const std::uint8_t*>(manager) + property->Offset;
            if (!Readable(address, sizeof(output))) return false;
            std::memcpy(&output, address, sizeof(output)); // Read-only view; never modify the engine array.
            return MeshVisibility::ArrayReadable(output, 128);
        }
        field = field->Next;
    }
    return false;
}
} // namespace GardenVisibility

bool GardenVisibilityRestore(const char* reason) {
    using namespace GardenVisibility;
    restoring = true;
    bool complete = true;
    for (auto& entry : entries) {
        if (!entry.owned) continue;
        SDK::UObject* wrapper{};
        const auto state = Resolve(entry.wrapper, wrapper);
        if (state == ObjectState::Retired) { entry.owned = false; continue; }
        SDK::UObject* world{};
        if (state != ObjectState::Live || Resolve(entry.world, world) != ObjectState::Live || OwningWorld(wrapper) != world) {
            complete = false; continue;
        }
        const auto currentDevil = GardenDiagnostics::ObjectProperty(wrapper, "DevilBaseInstance");
        const auto currentManager = GardenDiagnostics::ObjectProperty(wrapper, "BP Garden Manager");
        if (!currentDevil || !currentManager) { complete = false; continue; }
        if (!Matches(entry.devil, currentDevil) || !Matches(entry.manager, currentManager)) {
            // A live replacement proves a new garden binding. Do not apply an
            // earlier demon's permission snapshot to that replacement's setup.
            entry.owned = false;
            Log("GARDEN_VISIBILITY_RETIRE wrapper=%s garden binding replaced; current permission retained without write", Name(wrapper).c_str());
            continue;
        }
        bool current{};
        if (!ReadPermission(wrapper, current)) { complete = false; continue; }
        // Only undo our last false value. A newer native true already releases
        // the restriction; do not force any actor's visible/hidden state.
        if (!current && !SetPermission(wrapper, entry.original)) { complete = false; continue; }
        entry.owned = false;
        if (std::strcmp(reason, "garden-tick-finished") != 0)
            Log("GARDEN_VISIBILITY_RESTORE reason=%s wrapper=%s original=%d; native timeline/visibility retained",
                reason, Name(wrapper).c_str(), entry.original);
    }
    if (complete) { entries.clear(); restoring = false; }
    else Failure("restore-pending");
    return complete;
}

bool GardenVisibilityBefore(SDK::UObject* object, SDK::UFunction* function) {
    using namespace GardenVisibility;
    if (scopeActive || !requested.load() || !applied || restorationPending || !binding.valid ||
        !insideProbe || sampleThread.load() != GetCurrentThreadId()) return false;
    if (!object || !function || !object->Class) return false;
    // Cache only immutable FName class keys, never raw UObject addresses.
    thread_local std::unordered_map<std::uint64_t, bool> classes;
    std::uint64_t key{};
    std::memcpy(&key, &object->Class->Name, sizeof(key));
    auto known = classes.find(key);
    if (known == classes.end()) {
        if (classes.size() >= 4096) classes.clear();
        known = classes.emplace(key, ClassName(object) == "BP_GardenManager_C").first;
    }
    if (!known->second || !IndexedObject(object) || !IndexedObject(function) ||
        Name(function) != "ReceiveTick" || function->Outer != object->Class) return false;
    SDK::UObject* cameraObject{};
    if (Resolve(binding.manager, cameraObject) != ObjectState::Live) return false;
    const auto manager = reinterpret_cast<SDK::AProjectPlayerCameraManager_C*>(cameraObject);
    CameraContext context{};
    if (!Context(manager, context) || !BoundTo(context) || ViewBlock(context) || OwningWorld(object) != context.world) return false;
    const auto mode = context.world->AuthorityGameMode;
    const auto sublevels = GardenDiagnostics::ObjectProperty(mode, "MapSubLevelManager");
    double loaded{}, finished{};
    if (!GardenDiagnostics::Scalar(sublevels, "IsGardenLoaded", loaded) || loaded != 1 ||
        !GardenDiagnostics::Scalar(sublevels, "IsEndLoadGarden", finished) || finished != 1) {
        return false;
    }
    if ((restoring || !entries.empty()) && !GardenVisibilityRestore("retry-before-scope")) return false;
    double ready{};
    if (!GardenDiagnostics::Scalar(object, "GardenDevilsReady", ready) || ready != 1) return false;
    SDK::TArray<SDK::UObject*> wrappers;
    if (!GardenArray(object, wrappers)) { Failure("garden-array-signature"); return false; }
    scopeActive = true;
    for (int i = 0; i < wrappers.Num(); ++i) {
        const auto wrapper = wrappers[i];
        SDK::UObject* devil{};
        SDK::UObject* gardenManager{};
        if (!ReadyWrapper(wrapper, context.world, devil, gardenManager) || gardenManager != object) continue;
        bool current{};
        if (!ReadPermission(wrapper, current)) { Failure("permission-property"); continue; }
        if (!current) continue; // Preserve native dialogue/scene opt-outs untouched.
        if (entries.size() >= 128) { Failure("snapshot-budget"); continue; }
        if (!Setter(wrapper)) { Failure("permission-setter-signature"); continue; }
        // Record ownership before the native call, so partial failures retain
        // enough identity to retry restoration instead of losing the snapshot.
        entries.push_back({Identify(wrapper), Identify(devil), Identify(gardenManager), Identify(context.world), current, true});
        if (!SetPermission(wrapper, false)) {
            Failure("permission-setter-failed");
            GardenVisibilityRestore("apply-failed");
            return true;
        }
    }
    const auto now = GetTickCount64();
    if (!entries.empty() && now - lastApplyLog >= 2000) {
        Log("GARDEN_VISIBILITY_SCOPE manager=%s wrappers=%zu; CanBeHidden=false only during native Tick Hit; dialogue flags restored before callback returns",
            Name(object).c_str(), entries.size());
        lastApplyLog = now;
    }
    return true;
}

void GardenVisibilityAfter() {
    GardenVisibilityRestore("garden-tick-finished");
    GardenVisibility::scopeActive = false;
}
