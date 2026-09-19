// SPDX-License-Identifier: GPL-3.0-or-later
// Same control vocabulary as SMTVV; P5R uses height above the player root.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace CameraTuning {
struct Values {float eye,fov,gait;};
constexpr std::uint32_t Defaults=65u|(50u<<7)|(20u<<14);
inline std::uint32_t Encode(float eye,float fov,float gait) {
    if(!std::isfinite(eye))eye=165;
    if(!std::isfinite(fov))fov=110;
    if(!std::isfinite(gait))gait=5;
    return static_cast<unsigned>(std::lround(std::clamp(eye,100.f,220.f)-100)) |
        (static_cast<unsigned>(std::lround(std::clamp(fov,60.f,140.f)-60))<<7) |
        (static_cast<unsigned>(std::lround(std::clamp(gait,0.f,10.f)*4))<<14);
}
inline Values Decode(std::uint32_t bits) {
    return {100.f+std::min(bits&127u,120u),60.f+std::min((bits>>7)&127u,80u),std::min((bits>>14)&63u,40u)/4.f};
}
inline std::uint32_t Adjust(std::uint32_t bits,unsigned field,bool decrease,bool coarse) {
    auto v=Decode(bits);const float sign=decrease?-1.f:1.f;
    if(field==0)v.eye+=sign*(coarse?5:1);
    if(field==1)v.fov+=sign*(coarse?5:1);
    if(field==2)v.gait+=sign*(coarse?1:.25f);
    return Encode(v.eye,v.fov,v.gait);
}
enum class Action {None,Toggle,Reset,Eye,Fov,Bob};
inline Action Chord(unsigned key,bool shift,bool ctrl,bool alt) {
    if(alt)return Action::None;
    if(key==0)return shift?Action::None:ctrl?Action::Reset:Action::Toggle;
    return key==1?Action::Eye:key==2?Action::Fov:key==3?Action::Bob:Action::None;
}
}
