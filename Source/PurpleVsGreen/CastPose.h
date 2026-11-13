#pragma once

#include "ArenaRules.h"

namespace PvG
{
struct CastPose
{
    float Gather = 0.f;
    float Release = 0.f;
    float Phase = 0.f;
};

constexpr float Ease(float Value)
{
    const float T = std::clamp(Value, 0.f, 1.f);
    return T * T * (3.f - 2.f * T);
}

constexpr CastPose SampleCast(Spell Active, bool Charging, double TimeFromRelease)
{
    constexpr std::array<float, 4> FollowThrough{.060f, .160f, .205f, .250f};
    const int Slot = Index(Active);
    const float Windup = Arsenal[Slot].Windup;
    const float RecoveryTime = FollowThrough[Slot];
    if (Charging && Windup > 0.f)
    {
        const float Progress = std::clamp(1.f + static_cast<float>(TimeFromRelease) / Windup, 0.f, 1.f);
        return {Ease(Progress), 0.f, Progress * .45f};
    }

    const float Age = static_cast<float>(TimeFromRelease);
    if (Age < 0.f || Age >= RecoveryTime)
        return {};
    const float Drive = std::min(.032f, RecoveryTime * .32f);
    const float Recovery = 1.f - Ease((Age - Drive) / (RecoveryTime - Drive));
    return {Windup > 0.f ? 1.f - Ease(Age / Drive) : 0.f,
        Ease(Age / Drive) * Recovery,
        Windup > 0.f ? .45f + .55f * Age / RecoveryTime : Age / RecoveryTime};
}
}
