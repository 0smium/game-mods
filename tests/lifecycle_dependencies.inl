// External visibility/gait adapters only. Camera identity, context, bindings,
// field ownership, restoration, and ControlBefore/After are production code.
namespace TestVisibility {
SDK::APlayerBase_C* owner{};
bool pending{};
bool failApply{};
unsigned applies{};
unsigned restores{};
std::function<void(const CameraContext&)> onApply;
}

bool VisibilityRestore(const char*) {
    ++TestVisibility::restores;
    if (TestVisibility::pending) return false;
    TestVisibility::owner = nullptr;
    return true;
}

bool VisibilityApply(const CameraContext& c) {
    ++TestVisibility::applies;
    if (TestVisibility::onApply) TestVisibility::onApply(c);
    TestVisibility::owner = c.pawn;
    return !TestVisibility::failApply;
}

namespace TestGarden {
bool pending{};
unsigned restores{};
}
bool GardenVisibilityRestore(const char*) {
    ++TestGarden::restores;
    return !TestGarden::pending;
}

namespace TestDash {
bool pending{};
unsigned restores{}, updates{};
}
bool DashEffectRestore(const char*) {
    ++TestDash::restores;
    return !TestDash::pending;
}
void DashEffectUpdate(const CameraContext&) { ++TestDash::updates; }

void ResetGait() {}
float GaitOffset(const CameraContext&, float, float) { return 0; }
