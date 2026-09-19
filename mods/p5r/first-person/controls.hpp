// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace p5fp {

constexpr float Pi = 3.14159265358979323846f;
constexpr float Degrees = Pi / 180.f;
constexpr float PitchLimit = 80.f * Degrees;

// Angles and angular velocities are radians and radians/second.
struct Look { float yaw{}, pitch{}, vYaw{}, vPitch{}; };
// Relative mouse distance is independent of frame duration and stick response.
inline void StepMouse(Look& look,int dx,int dy) noexcept {
    constexpr float radiansPerCount=.08f*Degrees;
    look.yaw=std::remainder(look.yaw-static_cast<float>(dx)*radiansPerCount,2.f*Pi);
    look.pitch=std::clamp(look.pitch-static_cast<float>(dy)*radiansPerCount,-PitchLimit,PitchLimit);
    if(dx || dy)look.vYaw=look.vPitch=0;
}
struct LookSettings {
    float deadzone = .03f;      // native pad input already removes its hardware deadzone
    float curve = 1.2f;
    float yawRate = 120.f;       // degrees/second
    float pitchRate = 78.f;     // degrees/second
    float smoothingSeconds = .02f;
    bool invertY = false;
};

inline Look SeedLook(const std::array<float, 16>& view) noexcept {
    Look result{};
    if (!std::isfinite(view[2]) || !std::isfinite(view[6]) || !std::isfinite(view[10]))
        return result;
    result.yaw = std::atan2(-view[2], -view[10]);
    result.pitch = std::clamp(std::asin(std::clamp(-view[6], -1.f, 1.f)),
                              -PitchLimit, PitchLimit);
    return result;
}

// Column-major world-to-view. The native camera's third row is -forward.
inline std::array<float, 16> AimView(const Look& look,
                                     const std::array<float, 3>& eye) noexcept {
    const float sy = std::sin(look.yaw), cy = std::cos(look.yaw);
    const float sp = std::sin(look.pitch), cp = std::cos(look.pitch);
    const std::array<float, 3> right{-cy, 0.f, sy};
    const std::array<float, 3> up{-sy * sp, cp, -cy * sp};
    const std::array<float, 3> back{-sy * cp, -sp, -cy * cp};
    std::array<float, 16> result{};
    for (int column = 0; column != 3; ++column) {
        result[column * 4] = right[column];
        result[column * 4 + 1] = up[column];
        result[column * 4 + 2] = back[column];
        result[12] -= right[column] * eye[column];
        result[13] -= up[column] * eye[column];
        result[14] -= back[column] * eye[column];
    }
    result[15] = 1.f;
    return result;
}

inline void StepLook(Look& look, float x, float y, float dt,
                     const LookSettings& settings) noexcept {
    if (!std::isfinite(dt) || dt <= 0.f || dt > .1f) {
        look.vYaw = look.vPitch = 0.f;
        return;
    }
    if (!std::isfinite(x)) x = 0.f;
    if (!std::isfinite(y)) y = 0.f;
    x = std::clamp(x, -1.f, 1.f);
    y = std::clamp(y, -1.f, 1.f);
    const float length = std::sqrt(x * x + y * y);
    const float deadzone = std::isfinite(settings.deadzone)
        ? std::clamp(settings.deadzone, 0.f, .95f) : .03f;
    const float curve = std::isfinite(settings.curve)
        ? std::clamp(settings.curve, .1f, 5.f) : 1.2f;
    if (length > deadzone) {
        const float response = std::pow((std::min(length, 1.f) - deadzone) /
                                        (1.f - deadzone), curve);
        x *= response / length;
        y *= response / length;
    } else {
        x = y = 0.f;
    }
    const float yawRate = std::isfinite(settings.yawRate)
        ? std::clamp(settings.yawRate, 0.f, 720.f) : 120.f;
    const float pitchRate = std::isfinite(settings.pitchRate)
        ? std::clamp(settings.pitchRate, 0.f, 720.f) : 78.f;
    const float targetYaw = -x * yawRate * Degrees;
    const float targetPitch = (settings.invertY ? y : -y) * pitchRate * Degrees;
    const float tau = std::isfinite(settings.smoothingSeconds)
        ? std::clamp(settings.smoothingSeconds, 0.f, .5f) : .02f;

    // Solve dv/dt=(target-v)/tau and integrate angle analytically. A constant
    // stick input therefore produces the same turn at 30, 60 and 120 FPS.
    const auto integrate = [dt, tau](float& angle, float& velocity, float target) {
        if (tau < .00001f) {
            velocity = target;
            angle += target * dt;
            return;
        }
        const float decay = std::exp(-dt / tau);
        const float difference = velocity - target;
        angle += target * dt + difference * tau * (1.f - decay);
        velocity = target + difference * decay;
    };
    integrate(look.yaw, look.vYaw, targetYaw);
    integrate(look.pitch, look.vPitch, targetPitch);
    look.yaw = std::remainder(look.yaw, 2.f * Pi);
    if (look.pitch > PitchLimit) {
        look.pitch = PitchLimit;
        if (look.vPitch > 0.f) look.vPitch = 0.f;
    } else if (look.pitch < -PitchLimit) {
        look.pitch = -PitchLimit;
        if (look.vPitch < 0.f) look.vPitch = 0.f;
    }
}

struct Gait { float phase{}, envelope{}, offset{}; };

inline void ResetGait(Gait& gait) noexcept { gait = {}; }

// Position changes are game-world units (centimetres in the tested scenes).
// This heuristic is not a native grounded test: the caller must exclude menus,
// cinematics and known traversal states before setting eligible=true.
inline void StepGait(Gait& gait, float planarDistance, float verticalDelta,
                     float dt, float amplitude, bool eligible) noexcept {
    if (!eligible || !std::isfinite(dt) || dt <= 0.f || dt > .1f ||
        !std::isfinite(planarDistance) || planarDistance < 0.f || planarDistance > 150.f ||
        !std::isfinite(verticalDelta) ||
        std::fabs(verticalDelta) > std::max(3.f, planarDistance * .75f) ||
        !std::isfinite(amplitude) || amplitude <= 0.f) {
        ResetGait(gait);
        return;
    }
    const bool moving = planarDistance > .001f;
    const float target = moving ? 1.f : 0.f;
    const float tau = moving ? .10f : .14f;
    gait.envelope = target + (gait.envelope - target) * std::exp(-dt / tau);
    if (moving) {
        const float cycles = std::min(planarDistance / 90.f, 2.4f * dt);
        gait.phase = std::fmod(gait.phase + cycles * 2.f * Pi, 2.f * Pi);
    }
    if (!moving && gait.envelope < .0001f) {
        ResetGait(gait);
        return;
    }
    gait.offset = amplitude * gait.envelope * std::sin(gait.phase);
}

} // namespace p5fp
