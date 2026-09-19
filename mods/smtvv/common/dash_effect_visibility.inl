// Purple Tsukuyomi dash only. Verified Pla603.SettingDashEffect(true) loads
// pl_603_11 into DashEffect2 and attaches it to Mesh/Eff_102. The blue branch
// loads pl_603_10 into that reference at EFF_001 and is deliberately untouched.
// TickMovement owns SetActive(IsDashingForEffect): do not deactivate effects or
// change gameplay, sound, material, attachment, or particle simulation state.
// Include after garden_diagnostics.inl; restore participates in camera cleanup.
namespace DashEffectVisibility {
struct Entry {
    ObjectIdentity component, pawn, world;
    bool originalOwner{};
    bool owned{};
};
Entry entry;
bool restoring{};
ULONGLONG lastStatus{};
std::string previousStatus;

void Status(const char* reason, SDK::UParticleSystemComponent* effect = nullptr) {
    const auto now = GetTickCount64();
    if (previousStatus == reason && now - lastStatus < 5000) return;
    previousStatus = reason;
    lastStatus = now;
    Log("DASH_EFFECT_STATUS reason=%s component=%p; only purple DashEffect2/pl_603_11 owner-view flag is eligible", reason, effect);
}

bool SetOwnerNoSee(SDK::UParticleSystemComponent* effect, bool hidden) {
    if (!insideProbe || sampleThread.load() != GetCurrentThreadId() ||
        !IndexedObject(effect) || !Readable(effect, sizeof(*effect)) ||
        !HasClass(effect, "ParticleSystemComponent")) return false;
    const auto identity = Identify(effect);
    const auto function = MeshVisibility::NativeBoolFunction(effect, "SetOwnerNoSee", "bNewOwnerNoSee");
    if (!function || function->Size != 1) return false;
    const auto parameter = reinterpret_cast<SDK::FBoolProperty*>(function->ChildProperties);
    if (!parameter || !parameter->Owner.bIsUObject || parameter->Owner.Container.Object != function) return false;
    struct Parameters { bool bNewOwnerNoSee; } parameters{hidden};
    processEventHook.unsafe_call<void>(effect, function, &parameters);
    return Matches(identity, effect) && effect->bOwnerNoSee == hidden;
}

const char* Eligible(const CameraContext& context, SDK::UParticleSystemComponent*& effect) {
    effect = nullptr;
    if (ClassName(context.pawn) != "Pla603_C") return "other-player-form";
    double purple{};
    if (!GardenDiagnostics::Scalar(context.pawn, "bTsukuyomiForm", purple)) return "form-flag-unresolved";
    if (purple != 1) return "blue-form-retained";
    const auto object = GardenDiagnostics::ObjectProperty(context.pawn, "DashEffect2");
    if (!object || !HasClass(object, "ParticleSystemComponent") ||
        !Readable(object, sizeof(SDK::UParticleSystemComponent))) return "dash-component-unresolved";
    effect = reinterpret_cast<SDK::UParticleSystemComponent*>(object);
    // Both the direct Blueprint reference and the real owner/attachment chain
    // are required. Never enumerate all nearby/world particles to hide them.
    if (effect->Outer != context.pawn || !MeshVisibility::BelongsTo(effect, context.pawn, context.world) ||
        !IndexedObject(context.pawn->Mesh) || effect->AttachParent != context.pawn->Mesh)
        return "dash-owner-or-attachment-differs";
    auto socket = effect->AttachSocketName.ToString();
    for (auto& c : socket) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (socket != "eff_102") return "purple-socket-differs";
    const auto particle = effect->Template;
    if (!IndexedObject(particle) || !HasClass(particle, "ParticleSystem") ||
        Name(particle) != "pl_603_11" || !IndexedObject(particle->Outer) ||
        particle->Outer->Name.GetRawString() != "/Game/Design/Effect/Particles/chara/PL/pl_603_11")
        return "purple-template-differs";
    // The verified original TickMovement synchronizes this native active flag
    // with PlayerMovement.IsDashingForEffect for this exact component.
    if (!effect->bIsActive) return "dash-inactive";
    return nullptr;
}
} // namespace DashEffectVisibility

bool DashEffectRestore(const char* reason) {
    using namespace DashEffectVisibility;
    restoring = true;
    if (!entry.owned) { entry = {}; restoring = false; return true; }
    SDK::UObject* object{};
    const auto state = Resolve(entry.component, object);
    if (state == ObjectState::Retired) {
        Log("DASH_EFFECT_RETIRE reason=%s; original component replaced/destroyed, no write", reason);
        entry = {}; restoring = false; return true;
    }
    SDK::UObject* pawn{};
    SDK::UObject* world{};
    if (state != ObjectState::Live || Resolve(entry.pawn, pawn) != ObjectState::Live ||
        Resolve(entry.world, world) != ObjectState::Live || !HasClass(pawn, "PlayerBase_C") ||
        !Readable(pawn, sizeof(SDK::APlayerBase_C)) || !Readable(object, sizeof(SDK::UParticleSystemComponent)) ||
        !MeshVisibility::BelongsTo(object, reinterpret_cast<SDK::APlayerBase_C*>(pawn), reinterpret_cast<SDK::UWorld*>(world))) {
        Status("restore-pending");
        return false;
    }
    const auto effect = reinterpret_cast<SDK::UParticleSystemComponent*>(object);
    // Restoration needs the saved component's real owner, not the current
    // DashEffect2 reference or current template: those may already be replaced.
    if (effect->bOwnerNoSee && !SetOwnerNoSee(effect, entry.originalOwner)) {
        Status("restore-setter-pending", effect);
        return false;
    }
    Log("DASH_EFFECT_RESTORE reason=%s component=%p originalOwnerNoSee=%d; native activation/template retained",
        reason, effect, entry.originalOwner);
    entry = {};
    restoring = false;
    return true;
}

void DashEffectUpdate(const CameraContext& supplied) {
    using namespace DashEffectVisibility;
    CameraContext context{};
    if (!requested.load() || !applied || restorationPending || !Context(supplied.manager, context) ||
        !BoundTo(context) || context.pawn != supplied.pawn || ViewBlock(context)) {
        DashEffectRestore("first-person-not-active");
        return;
    }
    if (restoring && !DashEffectRestore("retry-before-apply")) return;
    SDK::UParticleSystemComponent* effect{};
    if (const auto reason = Eligible(context, effect)) {
        Status(reason, effect);
        DashEffectRestore(reason);
        return;
    }
    if (entry.owned && (!Matches(entry.component, effect) || !Matches(entry.pawn, context.pawn) || !Matches(entry.world, context.world))) {
        if (!DashEffectRestore("dash-identity-changed")) return;
    }
    if (effect->bOwnerNoSee) return; // A native true is never newly claimed.
    const auto componentIdentity = Identify(effect);
    const auto pawnIdentity = Identify(context.pawn);
    const auto worldIdentity = Identify(context.world);
    if (!componentIdentity.pointer || !pawnIdentity.pointer || !worldIdentity.pointer) {
        Status("apply-identity-unresolved", effect);
        return;
    }
    entry = {componentIdentity, pawnIdentity, worldIdentity, false, true};
    if (!SetOwnerNoSee(effect, true)) {
        Status("apply-setter-failed", effect);
        DashEffectRestore("apply-failed");
        return;
    }
    Log("DASH_EFFECT_APPLY pawn=%s component=%p template=pl_603_11 socket=Eff_102 OwnerNoSee=0->1; purple dash only, blue/common/attack/world effects retained",
        Name(context.pawn).c_str(), effect);
}
