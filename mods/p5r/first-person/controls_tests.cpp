// SPDX-License-Identifier: GPL-3.0-or-later
// Pure controls fixtures: no hooks, game process, files or saves are accessed.
#include "controls.hpp"
#include <cstdio>
#include <stdexcept>
#include <limits>

namespace {
void Require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
bool Near(float a, float b, float epsilon = .00002f) {
    return std::fabs(a - b) <= epsilon;
}
p5fp::Look SimulateLook(int fps) {
    p5fp::Look result{};
    p5fp::LookSettings settings{};
    for (int i = 0; i < fps; ++i)
        p5fp::StepLook(result, .7f, -.3f, 1.f / fps, settings);
    for (int i = 0; i < fps / 2; ++i)
        p5fp::StepLook(result, 0.f, 0.f, 1.f / fps, settings);
    return result;
}
p5fp::Gait SimulateGait(int fps) {
    p5fp::Gait result{};
    for (int i = 0; i < fps; ++i)
        p5fp::StepGait(result, 112.5f / fps, 0.f, 1.f / fps, 3.f, true);
    return result;
}
} // namespace

int main() {
    using namespace p5fp;
    try {
        const std::array<float, 3> eye{12.f, 165.f, -23.f};
        for (const float yaw : {-2.8f, -.6f, 0.f, .4f, 2.9f}) {
            for (const float pitch : {-.9f, 0.f, .8f}) {
                const Look source{yaw, pitch, 0.f, 0.f};
                const auto view = AimView(source, eye);
                const auto seeded = SeedLook(view);
                Require(Near(seeded.yaw, yaw) && Near(seeded.pitch, pitch), "look/view roundtrip");
                for (int row = 0; row != 3; ++row) {
                    Require(Near(view[row] * eye[0] + view[4 + row] * eye[1] +
                                 view[8 + row] * eye[2] + view[12 + row], 0.f, .00004f),
                            "camera eye maps to origin");
                    float norm = 0.f;
                    for (int column = 0; column != 3; ++column)
                        norm += view[column * 4 + row] * view[column * 4 + row];
                    Require(Near(norm, 1.f), "view basis remains unit length");
                }
                Require(view[3] == 0.f && view[7] == 0.f && view[11] == 0.f && view[15] == 1.f,
                        "homogeneous view matrix");
            }
        }
        {
            std::array<float, 16> identity{};
            identity[0] = identity[5] = identity[10] = identity[15] = 1.f;
            const auto result = AimView(SeedLook(identity), {0.f, 0.f, 0.f});
            for (int i = 0; i != 16; ++i) Require(Near(identity[i], result[i]), "identity native view roundtrip");
        }
        LookSettings settings{};
        settings.smoothingSeconds = 0.f;
        {
            Look right{}, left{}, up{}, down{};
            StepLook(right, 1.f, 0.f, .05f, settings);
            StepLook(left, -1.f, 0.f, .05f, settings);
            StepLook(up, 0.f, -1.f, .05f, settings);
            StepLook(down, 0.f, 1.f, .05f, settings);
            Require(right.yaw < 0.f && left.yaw > 0.f, "horizontal input convention");
            Require(up.pitch > 0.f && down.pitch < 0.f, "non-inverted vertical input convention");
            // At yaw=0 native camera forward=+Z and screen-right=-X.
            auto view = AimView(right, {0.f, 0.f, 0.f});
            Require(-view[2] < 0.f && -view[10] > 0.f, "right turn follows camera screen-right");
            settings.invertY = true;
            Look inverted{};
            StepLook(inverted, 0.f, 1.f, .05f, settings);
            Require(inverted.pitch > 0.f, "optional Y inversion");
            settings.invertY = false;
        }
        {
            Look tiny{}, diagonal{};
            StepLook(tiny, .01f, .01f, .05f, settings);
            Require(tiny.yaw == 0.f && tiny.pitch == 0.f, "radial deadzone rejects drift");
            StepLook(diagonal, 1.f, 1.f, .05f, settings);
            Require(Near(diagonal.vYaw / (settings.yawRate * Degrees), -std::sqrt(.5f)) &&
                    Near(diagonal.vPitch / (settings.pitchRate * Degrees), -std::sqrt(.5f)),
                    "diagonal stick length is capped");
        }
        {
            const auto a = SimulateLook(30), b = SimulateLook(60), c = SimulateLook(120);
            Require(Near(a.yaw, b.yaw) && Near(a.yaw, c.yaw) &&
                    Near(a.pitch, b.pitch) && Near(a.pitch, c.pitch), "look integration at 30/60/120 FPS");
        }
        {
            Look look{};
            for (int i = 0; i < 100; ++i) StepLook(look, 0.f, -1.f, .05f, settings);
            Require(Near(look.pitch, PitchLimit) && look.vPitch == 0.f, "pitch upper clamp stops velocity");
            for (int i = 0; i < 100; ++i) StepLook(look, 0.f, 1.f, .05f, settings);
            Require(Near(look.pitch, -PitchLimit) && look.vPitch == 0.f, "pitch lower clamp stops velocity");
            StepLook(look, 1.f, 0.f, .05f, settings);
            const float previousYaw = look.yaw, previousPitch = look.pitch;
            StepLook(look, 1.f, 1.f, 2.f, settings);
            Require(look.yaw == previousYaw && look.pitch == previousPitch && look.vYaw == 0.f && look.vPitch == 0.f,
                    "long stall drops look momentum without jumping");
            StepLook(look, 1.f, 1.f, std::numeric_limits<float>::quiet_NaN(), settings);
            Require(look.yaw == previousYaw && look.pitch == previousPitch, "invalid delta does not move look");
        }
        {
            const auto a = SimulateGait(30), b = SimulateGait(60), c = SimulateGait(120);
            Require(Near(a.offset, b.offset, .0001f) && Near(a.offset, c.offset, .0001f),
                    "distance gait agrees at 30/60/120 FPS");
            Require(a.offset > 2.9f && a.offset <= 3.f, "gait respects configured amplitude");
            Gait gait = a;
            float previous = std::fabs(gait.offset);
            for (int i = 0; i < 120; ++i) {
                StepGait(gait, 0.f, 0.f, 1.f / 60.f, 3.f, true);
                Require(std::fabs(gait.offset) <= previous + .00001f, "idle gait fades without oscillation");
                previous = std::fabs(gait.offset);
            }
            Require(gait.offset == 0.f && gait.phase == 0.f, "idle gait settles at neutral");
            StepGait(gait, 150.1f, 0.f, .05f, 3.f, true);
            Require(gait.offset == 0.f, "planar teleport has no added gait");
            gait = a;
            StepGait(gait, 2.f, 4.f, .05f, 3.f, true);
            Require(gait.offset == 0.f && gait.envelope == 0.f, "large vertical jump suppresses gait");
            gait = a;
            StepGait(gait, 2.f, 0.f, .05f, 3.f, false);
            Require(gait.offset == 0.f, "caller handoff suppresses gait");
            StepGait(gait, 100.f, 0.f, .05f, 3.f, true);
            Require(Near(gait.phase, 2.4f * .05f * 2.f * Pi), "gait frequency capped at 2.4 Hz");
            StepGait(gait, 1.f, 0.f, .05f, 0.f, true);
            Require(gait.offset == 0.f, "amplitude zero disables gait");
        }
        std::puts("PASS: native view/seed, stick controls, frame-rate integration, gait and handoff guards");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
