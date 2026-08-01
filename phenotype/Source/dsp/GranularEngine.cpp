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
        gainPole     = fastmath::onePoleCoeff (0.005f, newSampleRate);  // ~5 ms de-zip
        attackCoeff  = fastmath::onePoleCoeff (0.008f, newSampleRate);  // ~8 ms attack
        releaseCoeff = fastmath::onePoleCoeff (0.200f, newSampleRate);  // ~200 ms release

        fillGenome();   // internal band-limited wavetables (instrument mode)
        reset();
    }

    void GranularEngine::reset() noexcept
    {
        writeHead      = 0;
        grainClock     = 0.0f;
        smoothedGain   = 0.0f;
        liveGrainCount = 0;
        voiceRR        = 0;
        modulator.reset();
        for (auto& g : grains)
            g.active = false;
        for (auto& v : voices)
            v = MelodyVoice{};
    }

    //  Fills source A/B with a couple of contrasting band-limited timbres at the
    //  genome root (C4). Additive synthesis with std::sin here is fine — this is
    //  off the audio thread. A = sawtooth (all harmonics), B = square (odd only),
    //  giving the diploid cross-synthesis two distinct genotypes to blend.
    void GranularEngine::fillGenome() noexcept
    {
        if (sourceLen <= 0)
            return;

        constexpr double kPi = 3.14159265358979323846;
        const double w0   = 2.0 * kPi * kGenomeHz / sampleRate;
        const int    nyq  = static_cast<int> ((sampleRate * 0.5) / kGenomeHz) - 1;
        const int    kMax = nyq < 1 ? 1 : (nyq > 64 ? 64 : nyq);

        float peakA = 1.0e-6f, peakB = 1.0e-6f;
        for (int n = 0; n < sourceLen; ++n)
        {
            const double ph = w0 * n;
            double sa = 0.0, sb = 0.0;
            for (int k = 1; k <= kMax; ++k)      sa += std::sin (ph * k) / k;   // saw
            for (int k = 1; k <= kMax; k += 2)   sb += std::sin (ph * k) / k;   // square
            sourceA[(size_t) n] = static_cast<float> (sa);
            sourceB[(size_t) n] = static_cast<float> (sb);
            peakA = std::max (peakA, std::fabs (sourceA[(size_t) n]));
            peakB = std::max (peakB, std::fabs (sourceB[(size_t) n]));
        }
        const float na = 0.9f / peakA, nb = 0.9f / peakB;
        for (int n = 0; n < sourceLen; ++n)
        {
            sourceA[(size_t) n] *= na;
            sourceB[(size_t) n] *= nb;
        }
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

    //  pitchMul / ampMul carry the per-note ratio and envelope in instrument
    //  mode; both are 1.0 in effect mode.
    int GranularEngine::spawnGrain (const ParameterSnapshot& p, float modValue,
                                    float pitchMul, float ampMul) noexcept
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

                //  Pitch: knob detune (-12..+12 semis) per chromosome, scaled by
                //  the note ratio (pitchMul).
                const float semiA = (p.pitchA - 0.5f) * 24.0f;
                const float semiB = (p.pitchB - 0.5f) * 24.0f;
                const float incA  = fastmath::fastExp (semiA * 0.0577622650f) * pitchMul; // ln2/12
                const float incB  = fastmath::fastExp (semiB * 0.0577622650f) * pitchMul;

                //  Grain length.
                const float ms      = kMinGrainMs + p.grainSize * (kMaxGrainMs - kMinGrainMs);
                const float lenSamp = ms * 0.001f * static_cast<float> (sampleRate);

                //  Genotype blend, also nudged by the modulator.
                float ab = p.crossBlend + (modValue - 0.5f) * p.modDepth * 0.5f;
                ab = ab < 0.0f ? 0.0f : (ab > 1.0f ? 1.0f : ab);

                grains[i].trigger (startSample, startSample, incA, incB,
                                   lenSamp, 0.6f * ampMul, ab);
                return i;
            }
        }
        return -1;   // pool exhausted; drop the grain (no allocation)
    }

    //==========================================================================
    //  Melodic (instrument) voice management
    //==========================================================================
    void GranularEngine::advanceVoices() noexcept
    {
        for (auto& v : voices)
        {
            if (v.note < 0 && v.env <= 1.0e-4f)
                continue;

            const float target = v.gate ? 1.0f : 0.0f;
            const float coeff  = v.gate ? attackCoeff : releaseCoeff;
            v.env = target + (v.env - target) * coeff;

            //  Portamento: glide the current ratio toward the target.
            v.curRatio = v.ratio + (v.curRatio - v.ratio) * glideCoeff;

            if (! v.gate && v.env <= 1.0e-4f)   // fully released -> free the slot
                v = MelodyVoice{};
        }
    }

    int GranularEngine::pickVoice() noexcept
    {
        for (int t = 0; t < kMaxVoices; ++t)
        {
            voiceRR = (voiceRR + 1) % kMaxVoices;
            const auto& v = voices[(size_t) voiceRR];
            if (v.note >= 0 || v.env > 1.0e-4f)
                return voiceRR;
        }
        return -1;   // nothing sounding -> silence
    }

    int GranularEngine::allocateVoice (int note) noexcept
    {
        int   freeSlot = -1;
        int   steal    = 0;
        float lowest   = 2.0f;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            if (voices[(size_t) i].note == note) return i;                 // retrigger
            if (voices[(size_t) i].note == -1 && freeSlot < 0) freeSlot = i;
            if (voices[(size_t) i].env < lowest) { lowest = voices[(size_t) i].env; steal = i; }
        }
        return freeSlot >= 0 ? freeSlot : steal;
    }

    void GranularEngine::noteOn (int midiNote, float velocity) noexcept
    {
        auto& v = voices[(size_t) allocateVoice (midiNote)];
        const bool wasFree = (v.note < 0);
        v.note  = midiNote;
        v.vel   = velocity < 0.0f ? 0.0f : (velocity > 1.0f ? 1.0f : velocity);
        v.ratio = fastmath::fastExp (static_cast<float> (midiNote - 60) * 0.0577622650f);
        if (wasFree)
            v.curRatio = v.ratio;   // fresh voice starts on-pitch (glide only legato)
        v.gate  = true;             // env ramps up from its current level (click-free on steal)
    }

    void GranularEngine::noteOff (int midiNote) noexcept
    {
        for (auto& v : voices)
            if (v.note == midiNote)
                v.gate = false;   // enter release
    }

    void GranularEngine::allNotesOff() noexcept
    {
        for (auto& v : voices)
            v.gate = false;
    }

    int GranularEngine::activeVoices() const noexcept
    {
        int n = 0;
        for (const auto& v : voices)
            if (v.note >= 0 || v.env > 1.0e-4f)
                ++n;
        return n;
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
            //  1) Effect mode captures the incoming genome; instrument mode keeps
            //     the internal wavetable genome and advances the note envelopes.
            if (! instrumentMode)
            {
                sourceA[(size_t) writeHead] = inL ? inL[n] : 0.0f;
                sourceB[(size_t) writeHead] = inR ? inR[n] : (inL ? inL[n] : 0.0f);
                if (++writeHead >= sourceLen) writeHead = 0;
            }
            else
            {
                advanceVoices();
            }

            //  2) Advance the non-linear modulator.
            const float modValue = modulator.processSample();

            //  3) Schedule new grains. In instrument mode each grain is bound to
            //     a sounding voice (pitch ratio + envelope); if nothing sounds,
            //     no grain is spawned.
            grainClock -= 1.0f;
            if (grainClock <= 0.0f)
            {
                if (instrumentMode)
                {
                    const int vi = pickVoice();
                    if (vi >= 0)
                    {
                        const auto& v = voices[(size_t) vi];
                        const float amp = v.env * (0.15f + 0.85f * v.vel);   // velocity sensitivity
                        spawnGrain (p, modValue, v.curRatio * bendRatio, amp);
                    }
                }
                else
                {
                    spawnGrain (p, modValue, 1.0f, 1.0f);
                }
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
