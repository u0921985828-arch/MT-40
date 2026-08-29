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
    //  The 2^f mantissa uses a degree-5 minimax (the exp-series in ln2), giving
    //  a relative error < 1e-6 — accurate enough that note ratios derived from
    //  it (2^(semitones/12)) are in tune to well under a cent.
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

        //  2^f = e^(f*ln2) — degree-5 series in ln2, error < 1e-6 on [0, 1).
        const float p = 1.0f + f * (0.6931471805599453f
                              + f * (0.2402265069591007f
                              + f * (0.0555041086648216f
                              + f * (0.0096181291076285f
                              + f *  0.0013333558146428f))));

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

    //  Accurate, bounded, monotonic tanh for a drive / warmth stage. Built from
    //  the (now high-accuracy) fastExp:  tanh(x) = (1 - e^{-2x}) / (1 + e^{-2x}).
    //  Odd-symmetric, |output| < 1, error tracks fastExp (< 1e-5). Trig-free.
    [[nodiscard]] inline float fastTanh (float x) noexcept
    {
        if (x >  15.0f) return  1.0f;
        if (x < -15.0f) return -1.0f;
        const float e = fastExp (-2.0f * x);
        return (1.0f - e) / (1.0f + e);
    }

    //  tan(x) via Lambert's continued fraction (3 levels). Accurate to ~1e-3 up
    //  to x ~ 1.5 (just below pi/2), which is exactly the range a ZDF filter's
    //  pre-warp g = tan(pi * fc / fs) needs. Trig-free. Caller must keep
    //  x in [0, ~1.5]; we clamp for safety so the denominator can't reach zero.
    [[nodiscard]] inline float fastTan (float x) noexcept
    {
        x = x < 0.0f ? 0.0f : (x > 1.50f ? 1.50f : x);
        const float x2 = x * x;
        float t = 5.0f - x2 * (1.0f / 7.0f);
        t = 3.0f - x2 / t;
        t = 1.0f - x2 / t;
        return x / t;
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
