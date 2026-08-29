#pragma once

//==============================================================================
//  Grain.h
//
//  A single granular voice. Diploid: each grain reads from BOTH source buffers
//  (chromosome A / chromosome B) and cross-fades between them, so every grain
//  is itself a micro cross-synthesis event. Trivially-copyable POD — lives in a
//  fixed pre-allocated pool inside GranularEngine, never heap-allocated.
//==============================================================================

#include "FastMath.h"

namespace phenotype::dsp
{
    struct Grain
    {
        bool  active   = false;
        float readPosA = 0.0f;   // fractional read head into source A
        float readPosB = 0.0f;   // fractional read head into source B
        float incA     = 1.0f;   // per-sample advance (pitch) for A
        float incB     = 1.0f;   // per-sample advance (pitch) for B
        float phase    = 0.0f;   // window phase 0..1
        float phaseInc = 0.0f;   // 1 / grainLengthSamples
        float amp      = 1.0f;   // grain gain
        float blend    = 0.5f;   // A/B mix (0 = all A, 1 = all B)
        float pan      = 0.5f;   // stereo position (0 = left, 1 = right)

        void trigger (float startA, float startB,
                      float pitchA, float pitchB,
                      float lengthSamples, float gain, float ab,
                      float panPos) noexcept
        {
            active   = true;
            readPosA = startA;
            readPosB = startB;
            incA     = pitchA;
            incB     = pitchB;
            phase    = 0.0f;
            phaseInc = lengthSamples > 1.0f ? 1.0f / lengthSamples : 1.0f;
            amp      = gain;
            blend    = ab;
            pan      = panPos;
        }

        //  Raised-cosine window approximated with a trig-free parabola-squared
        //  (Welch^2): w = (4 * p * (1 - p))^2. Smooth, zero at the edges.
        [[nodiscard]] static float window (float p) noexcept
        {
            const float w = 4.0f * p * (1.0f - p);
            return w * w;
        }
    };
}
