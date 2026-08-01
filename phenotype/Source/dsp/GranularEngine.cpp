//==============================================================================
//  GranularEngine.cpp
//==============================================================================

#include "GranularEngine.h"
#include <algorithm>
#include <cmath>

namespace phenotype::dsp
{
    void GranularEngine::prepare (double newSampleRate, int /*maxBlockSize*/)
    {
        sampleRate = newSampleRate;
        sourceLen  = static_cast<int> (kSourceSeconds * newSampleRate);

        //  Off-thread allocation only.
        sourceA.assign (static_cast<size_t> (sourceLen), 0.0f);
        sourceB.assign (static_cast<size_t> (sourceLen), 0.0f);

        modulator.prepare (newSampleRate);
        gainPole = fastmath::onePoleCoeff (0.005f, newSampleRate);  // ~5 ms de-zip
        reset();
    }

    void GranularEngine::reset() noexcept
    {
        writeHead      = 0;
        grainClock     = 0.0f;
        smoothedGain   = 0.0f;
        liveGrainCount = 0;
        modulator.reset();
        for (auto& g : grains)
            g.active = false;
    }

    //  Linear-interpolated read from a source ring buffer at fractional pos.
    float GranularEngine::readSource (const std::vector<float>& buf, float pos) const noexcept
    {
        if (sourceLen <= 1)
            return 0.0f;

        //  Wrap into [0, sourceLen).
        float wrapped = pos;
        while (wrapped >= static_cast<float> (sourceLen)) wrapped -= static_cast<float> (sourceLen);
        while (wrapped < 0.0f)                            wrapped += static_cast<float> (sourceLen);

        const int   i0 = static_cast<int> (wrapped);
        const int   i1 = (i0 + 1 == sourceLen) ? 0 : i0 + 1;
        const float f  = wrapped - static_cast<float> (i0);
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
    }

    int GranularEngine::spawnGrain (const ParameterSnapshot& p, float modValue) noexcept
    {
        //  Find a free slot (linear scan over a fixed pool — bounded, lock-free).
        for (int i = 0; i < kMaxGrains; ++i)
        {
            if (! grains[i].active)
            {
                //  Position modulated by the capillary source + spray jitter.
                const float modPos  = (p.position + (modValue - 0.5f) * p.modDepth);
                const float jitter  = (nextRandom() - 0.5f) * p.spray;
                float readCentre    = (modPos + jitter);
                readCentre = readCentre - std::floor (readCentre);            // wrap 0..1
                const float startSample = readCentre * static_cast<float> (sourceLen);

                //  Pitch: normalised 0..1 -> -12..+12 semitones -> ratio.
                const float semiA = (p.pitchA - 0.5f) * 24.0f;
                const float semiB = (p.pitchB - 0.5f) * 24.0f;
                const float incA  = fastmath::fastExp (semiA * 0.0577622650f); // ln2/12
                const float incB  = fastmath::fastExp (semiB * 0.0577622650f);

                //  Grain length.
                const float ms      = kMinGrainMs + p.grainSize * (kMaxGrainMs - kMinGrainMs);
                const float lenSamp = ms * 0.001f * static_cast<float> (sampleRate);

                //  Genotype blend, also nudged by the modulator.
                float ab = p.crossBlend + (modValue - 0.5f) * p.modDepth * 0.5f;
                ab = ab < 0.0f ? 0.0f : (ab > 1.0f ? 1.0f : ab);

                grains[i].trigger (startSample, startSample, incA, incB,
                                   lenSamp, 0.6f, ab);
                return i;
            }
        }
        return -1;   // pool exhausted; drop the grain (no allocation)
    }

    void GranularEngine::process (const float* inL, const float* inR,
                                  float* outL, float* outR,
                                  int numSamples) noexcept
    {
        const ParameterSnapshot p = hub.snapshot();

        //  Push control values into the modulator (cheap; recompute is guarded).
        modulator.setCaudal        (p.caudal);
        modulator.setDensidadSuelo (p.soilDensity);
        modulator.setSaturation    (p.saturation);

        //  Grains-per-second from density (2 .. 200 gr/s).
        const float grainsPerSec = 2.0f + p.grainDensity * 198.0f;
        const float spawnPeriod  = static_cast<float> (sampleRate) / grainsPerSec;

        for (int n = 0; n < numSamples; ++n)
        {
            //  1) Capture incoming genome into the ring buffers.
            sourceA[(size_t) writeHead] = inL ? inL[n] : 0.0f;
            sourceB[(size_t) writeHead] = inR ? inR[n] : (inL ? inL[n] : 0.0f);
            if (++writeHead >= sourceLen) writeHead = 0;

            //  2) Advance the non-linear modulator.
            const float modValue = modulator.processSample();

            //  3) Schedule new grains.
            grainClock -= 1.0f;
            if (grainClock <= 0.0f)
            {
                spawnGrain (p, modValue);
                grainClock += spawnPeriod;
            }

            //  4) Render the active grain pool.
            float accL = 0.0f, accR = 0.0f;
            int   live = 0;
            for (auto& g : grains)
            {
                if (! g.active)
                    continue;

                ++live;
                const float w = Grain::window (g.phase);

                const float a = readSource (sourceA, g.readPosA);
                const float b = readSource (sourceB, g.readPosB);

                float gA, gB;
                fastmath::equalPowerPair (g.blend, gA, gB);
                const float s = (a * gA + b * gB) * w * g.amp;

                //  Diploid spatialisation: A leans left, B leans right.
                accL += s * (0.5f + 0.5f * gA);
                accR += s * (0.5f + 0.5f * gB);

                g.readPosA += g.incA;
                g.readPosB += g.incB;
                g.phase    += g.phaseInc;
                if (g.phase >= 1.0f)
                    g.active = false;
            }
            liveGrainCount = live;

            //  Per-sample gain smoothing removes zipper noise on automation.
            smoothedGain = p.outputGain + (smoothedGain - p.outputGain) * gainPole;
            if (outL) outL[n] = fastmath::softClip (accL * smoothedGain);
            if (outR) outR[n] = fastmath::softClip (accR * smoothedGain);
        }
    }
}
