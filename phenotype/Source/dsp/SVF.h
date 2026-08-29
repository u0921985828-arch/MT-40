#pragma once

//==============================================================================
//  SVF.h
//
//  Zero-delay-feedback state-variable filter (Andy Simper / Cytomic topology,
//  "SvfLinearTrapOptimised2"). One instance = one audio channel. Simultaneous
//  low-/band-/high-pass outputs from a single 2-pole core; the multimode select
//  blends them so a single "type" control sweeps LP -> BP -> HP.
//
//  Trap-integrator ZDF stays stable and musical at high resonance, unlike a
//  naive biquad swept in real time. Coefficients are derived from a pre-warped
//  g = tan(pi * fc / fs) (via fastmath::fastTan, so still trig-free) and a
//  damping k = 1/Q. Everything here is allocation-free, branch-light, noexcept.
//==============================================================================

#include "FastMath.h"

namespace phenotype::dsp
{
    struct SVF
    {
        float ic1eq = 0.0f;   // integrator 1 state
        float ic2eq = 0.0f;   // integrator 2 state

        void reset() noexcept { ic1eq = 0.0f; ic2eq = 0.0f; }

        //  Pre-warp a cutoff (Hz) into the trap-integrator g coefficient.
        [[nodiscard]] static float gForCutoff (float cutoffHz, float sampleRate) noexcept
        {
            constexpr float kPi = 3.14159265358979f;
            //  Keep pi*fc/fs comfortably below pi/2 so tan stays finite/accurate.
            float wc = kPi * cutoffHz / sampleRate;
            wc = wc < 1.0e-4f ? 1.0e-4f : (wc > 1.45f ? 1.45f : wc);
            return fastmath::fastTan (wc);
        }

        //  Process one sample. `g` from gForCutoff(), `k` = 1/Q (damping),
        //  `type01` in [0,1] sweeps low (0) -> band (0.5) -> high (1). The
        //  low/band/high mix keeps unity-ish level across the sweep.
        [[nodiscard]] float process (float v0, float g, float k, float type01) noexcept
        {
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;

            const float v3 = v0 - ic2eq;
            const float v1 = a1 * ic1eq + a2 * v3;
            const float v2 = ic2eq + a2 * ic1eq + a3 * v3;

            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;

            const float low  = v2;
            const float band = v1;
            const float high = v0 - k * v1 - v2;

            //  Crossfade LP->BP->HP as type01 sweeps 0 -> 0.5 -> 1.
            if (type01 <= 0.5f)
            {
                const float t = type01 * 2.0f;          // 0..1 : low -> band
                return low + (band - low) * t;
            }
            const float t = (type01 - 0.5f) * 2.0f;     // 0..1 : band -> high
            return band + (high - band) * t;
        }
    };
}
