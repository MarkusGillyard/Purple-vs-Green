#pragma once

// Kay Lousberg, KayKit Character Animations 1.1 (CC0 1.0).
// Reproduce with Tools/Art/extract_cast_motion.py.
namespace PvG::KayKit
{
struct FCastSample
{
    float Lift;
    float Reach;
    float Sweep;
};

inline constexpr int CastSampleCount = 9;

inline constexpr FCastSample Shoot[CastSampleCount] = {
    {0.00000f, 0.00000f, 0.00000f},
    {0.28437f, 0.37997f, -0.17728f},
    {0.30128f, 0.62801f, -0.19915f},
    {0.24629f, 0.55806f, -0.18676f},
    {0.27717f, 0.52378f, -0.20735f},
    {0.28135f, 0.39900f, -0.21569f},
    {0.18769f, 0.22446f, -0.14968f},
    {0.06000f, 0.07086f, -0.05045f},
    {0.00000f, 0.00000f, 0.00000f},
};

inline constexpr FCastSample Spellcasting[CastSampleCount] = {
    {0.00000f, 0.00000f, 0.00000f},
    {-0.04282f, -0.05721f, 0.05052f},
    {-0.12770f, -0.11129f, 0.10448f},
    {-0.21618f, -0.09673f, 0.07490f},
    {-0.25812f, -0.07919f, -0.00063f},
    {-0.24367f, -0.02097f, -0.08619f},
    {-0.18486f, 0.02104f, -0.12987f},
    {-0.06897f, 0.00906f, -0.06605f},
    {0.00000f, 0.00000f, 0.00000f},
};

inline constexpr FCastSample SpellcastingLong[CastSampleCount] = {
    {0.00000f, 0.00000f, 0.00000f},
    {0.37991f, 0.46924f, -0.08661f},
    {0.13072f, 0.39122f, -0.05462f},
    {0.32806f, 0.48782f, -0.12365f},
    {0.43544f, 0.57467f, -0.08691f},
    {0.30580f, 0.25164f, -0.08868f},
    {0.42325f, 0.18244f, -0.08098f},
    {0.06105f, 0.03394f, -0.01645f},
    {0.00000f, 0.00000f, 0.00000f},
};

inline constexpr FCastSample Raise[CastSampleCount] = {
    {0.00000f, 0.00000f, 0.00000f},
    {0.56910f, 0.29639f, -0.12989f},
    {1.07715f, 0.50390f, -0.23852f},
    {1.03872f, 0.53919f, -0.23050f},
    {1.02933f, 0.53557f, -0.23169f},
    {0.99698f, 0.51489f, -0.23092f},
    {0.81703f, 0.41424f, -0.20842f},
    {0.31002f, 0.17477f, -0.07790f},
    {0.00000f, 0.00000f, 0.00000f},
};
}
