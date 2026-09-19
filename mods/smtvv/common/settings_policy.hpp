#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace CameraTuning {
struct Values { float eye, fov, gait; };
constexpr std::uint32_t Defaults = 60u | (50u << 7) | (20u << 14);
inline std::uint32_t Encode(float eye, float fov, float gait) {
    if (!std::isfinite(eye)) eye = 40;
    if (!std::isfinite(fov)) fov = 110;
    if (!std::isfinite(gait)) gait = 5;
    const auto e = static_cast<unsigned>(std::lround(std::clamp(eye, -20.0f, 100.0f) + 20));
    const auto f = static_cast<unsigned>(std::lround(std::clamp(fov, 60.0f, 140.0f) - 60));
    const auto g = static_cast<unsigned>(std::lround(std::clamp(gait, 0.0f, 10.0f) * 4));
    return e | (f << 7) | (g << 14);
}
inline Values Decode(std::uint32_t bits) {
    return {static_cast<float>(std::min(bits & 127u, 120u)) - 20,
        static_cast<float>(std::min((bits >> 7) & 127u, 80u)) + 60,
        static_cast<float>(std::min((bits >> 14) & 63u, 40u)) / 4};
}
inline std::uint32_t Adjust(std::uint32_t bits, unsigned field, bool decrease, bool coarse) {
    auto v = Decode(bits);
    const float sign = decrease ? -1.0f : 1.0f;
    if (field == 0) v.eye += sign * (coarse ? 5 : 1);
    if (field == 1) v.fov += sign * (coarse ? 5 : 1);
    if (field == 2) v.gait += sign * (coarse ? 1 : .25f);
    return Encode(v.eye, v.fov, v.gait);
}
enum class Action { None, Toggle, Reset, Eye, Fov, Bob };
inline Action Chord(unsigned key, bool shift, bool ctrl, bool alt) {
    if (alt) return Action::None;
    if (key == 0) return shift ? Action::None : ctrl ? Action::Reset : Action::Toggle;
    return key == 1 ? Action::Eye : key == 2 ? Action::Fov : key == 3 ? Action::Bob : Action::None;
}
}
