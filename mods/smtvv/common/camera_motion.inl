// Native CustomPawn ground predicate, not ACharacter/MovementMode: SMTVV's
// PlayerBase inherits CustomPawn and uses PlayerMovement at 0x8F8.
ObjectIdentity groundFunctionIdentity;

SDK::UFunction* GroundFunction(SDK::APlayerBase_C* pawn) {
    SDK::UObject* cached{};
    if (Resolve(groundFunctionIdentity, cached) == ObjectState::Live) return reinterpret_cast<SDK::UFunction*>(cached);
    auto type = static_cast<SDK::UStruct*>(pawn->Class);
    for (unsigned depth = 0; type && depth < 24; ++depth) {
        if (!IndexedObject(type) || !Readable(type, sizeof(SDK::UStruct))) return nullptr;
        if (Name(type) == "CustomPawn") {
            auto field = type->Children;
            for (unsigned n = 0; field && n < 1024; ++n) {
                if (!IndexedObject(field) || !Readable(field, sizeof(SDK::UField))) return nullptr;
                if (Name(field) == "IsMovingOnGround") {
                    if (!HasClass(field, "Function") || field->Outer != type || !Readable(field, sizeof(SDK::UFunction))) return nullptr;
                    const auto function = reinterpret_cast<SDK::UFunction*>(field);
                    if (!(function->FunctionFlags & 0x400) || !Executable(reinterpret_cast<void*>(function->ExecFunction))) return nullptr;
                    const auto result = reinterpret_cast<SDK::FBoolProperty*>(function->ChildProperties);
                    if (!Readable(result, sizeof(*result)) || !Readable(result->ClassPrivate, sizeof(SDK::FFieldClass)) ||
                        result->ClassPrivate->Name.ToString() != "BoolProperty" || result->Name.ToString() != "ReturnValue" ||
                        result->Next || result->ArrayDim != 1 || result->ElementSize != 1 || result->Offset != 0 ||
                        result->ByteOffset != 0 || result->FieldSize != 1 || result->FieldMask != 0xff ||
                        !(result->PropertyFlags & 0x400)) return nullptr;
                    groundFunctionIdentity = Identify(function);
                    Log("GAIT native getter verified: CustomPawn.IsMovingOnGround");
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

struct GaitState {
    SDK::FVector previous{};
    bool initialized{};
    float phase{};
    float offset{};
};
GaitState gaitState;

void ResetGait() {
    gaitState = {};
    publishedBob.store(0);
    publishedSpeed.store(0);
}

float GaitOffset(const CameraContext& context, float dt, float amplitude) {
    const auto root = context.pawn->RootComponent;
    if (!foregroundGame.load() || !std::isfinite(dt) || dt < 0.00001f || dt > .12f ||
        !IndexedObject(root) || !Readable(root, sizeof(SDK::USceneComponent)) || root->AttachParent) {
        ResetGait();
        publishedGround.store("paused / invalid sample");
        return 0;
    }
    const auto position = root->RelativeLocation;
    if (!std::isfinite(position.X) || !std::isfinite(position.Y) || !std::isfinite(position.Z)) { ResetGait(); return 0; }
    const auto getter = GroundFunction(context.pawn);
    if (!getter) {
        ResetGait();
        publishedGround.store("native ground getter unresolved: gait disabled");
        return 0;
    }
    bool grounded = false;
    processEventHook.unsafe_call<void>(context.pawn, getter, &grounded);
    publishedGround.store(grounded ? "grounded (native CustomPawn)" : "airborne (native CustomPawn)");
    const auto previous = gaitState.previous;
    const bool hadPosition = gaitState.initialized;
    gaitState.previous = position;
    gaitState.initialized = true;
    if (!hadPosition || !grounded || amplitude <= 0) {
        gaitState.phase = 0;
        gaitState.offset = 0;
        publishedBob.store(0);
        publishedSpeed.store(0);
        return 0;
    }
    const float distance = std::hypot(position.X - previous.X, position.Y - previous.Y);
    const float speed = distance / dt;
    if (!std::isfinite(speed) || speed > 3000 || distance > 200 || std::abs(position.Z - previous.Z) > 200) {
        ResetGait();
        publishedGround.store("teleport / discontinuity: gait reset");
        return 0;
    }
    publishedSpeed.store(speed);
    constexpr float tau = 6.28318530718f;
    constexpr float maxFrequency = 2.4f;
    if (speed > 3) gaitState.phase = std::fmod(gaitState.phase + tau * std::min(distance / 180.0f, maxFrequency * dt), tau);
    const float movementWeight = std::clamp((speed - 3.0f) / 180.0f, 0.0f, 1.0f);
    const float target = amplitude * movementWeight * std::sin(gaitState.phase);
    gaitState.offset += (target - gaitState.offset) * (1.0f - std::exp(-18.0f * dt));
    gaitState.offset = std::clamp(gaitState.offset, -amplitude, amplitude);
    publishedBob.store(gaitState.offset);
    return gaitState.offset;
}
