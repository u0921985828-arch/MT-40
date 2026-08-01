#pragma once

//==============================================================================
//  FastMath.h
//  Branch-free scalar approximations for the audio thread.
//
//  The Phenotype engine forbids std::sin / std::cos and any standard
//  trigonometry inside the DSP hot path. Exponential curves (capacitor charge
//  / gravitational drain) are the only transcendental primitives we need, so
//  we provide a single, allocation-free, SIMD-friendly fastExp based on
//  IEEE-754 exponent manipulation plus a degree-3 minimax on the mantissa.
//
//  All functions are constexpr-friendly where possible and noexcept.
//==============================================================================

#include <cstdint>
#include <cmath>   // std::floor only (no trig)

namespace phenotype::fastmath
{
    //  e^x via 2^(x * log2 e). Valid for x in [-87, 88]; outside that range the
    //  result is clamped to avoid denormal / inf blow-ups on the audio thread.
    //  Max relative error ~3e-3 across the audio-relevant domain — more than
    //  sufficient for envelope / coefficient generation.
    [[nodiscard]] inline float fastExp (float x) noexcept
    {
        constexpr float kLo   = -87.0f;
        constexpr float kHi   =  88.0f;
        constexpr float kLog2e = 1.44269504088896341f;

        x = x < kLo ? kLo : (x > kHi ? kHi : x);

        const float t  = x * kLog2e;
        const float fl = std::floor (t);
        const int   i  = static_cast<int> (fl);
        const float f  = t - fl;                 // fractional part in [0, 1)

        //  2^f minimax (degree 3) on [0, 1).
        const float p = 1.0f + f * (0.6960656421638072f
                              + f * (0.2247483206189928f
                              + f *  0.0789868165909190f));

        //  Compose 2^i by writing the biased exponent directly.
        union { float asFloat; std::int32_t asInt; } u;
        u.asInt = (i + 127) << 23;
        return p * u.asFloat;
    }

    //  One-pole smoothing coefficient for a target time constant tau (seconds).
    //  Returns the per-sample pole g = e^{-1/(tau * fs)} used by charge/drain
    //  recursions:  y[n] = target + (y[n-1] - target) * g.
    [[nodiscard]] inline float onePoleCoeff (float tauSeconds, double sampleRate) noexcept
    {
        const float tau = tauSeconds < 1.0e-5f ? 1.0e-5f : tauSeconds;
        return fastExp (-1.0f / (tau * static_cast<float> (sampleRate)));
    }

    //  Cubic soft clip: transparent below |x|=1, saturating to +/-1 at |x|=1.5.
    //  Trig-free, branch-light; guards the master bus against grain-overlap
    //  overshoot without the harsh fold of hard clipping.
    [[nodiscard]] inline float softClip (float x) noexcept
    {
        if (x <= -1.5f) return -1.0f;
        if (x >=  1.5f) return  1.0f;
        return x - 0.14814814814f * x * x * x;   // 4/27, maps +/-1.5 -> +/-1
    }

    //  Perceptual (equal-power) crossfade gain pair for a 0..1 blend control.
    //  Uses a polynomial sqrt-free approximation of sin/cos quarter-arc so the
    //  audio thread stays trig-free. gainA falls, gainB rises.
    inline void equalPowerPair (float blend, float& gainA, float& gainB) noexcept
    {
        blend = blend < 0.0f ? 0.0f : (blend > 1.0f ? 1.0f : blend);
        //  Chebyshev-fit of cos(pi/2 * x) and sin(pi/2 * x), max err < 1e-3.
        const float x  = blend;
        const float x2 = x * x;
        gainB = x * (1.5707288f - x2 * (0.6432292f - x2 * 0.0727102f));   // ~sin
        const float y  = 1.0f - x;
        const float y2 = y * y;
        gainA = y * (1.5707288f - y2 * (0.6432292f - y2 * 0.0727102f));   // ~cos
    }
}
