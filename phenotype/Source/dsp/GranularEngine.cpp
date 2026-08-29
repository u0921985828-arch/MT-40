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

        //  Off-thread allocation only. One buffer per band-limited mip.
        for (int m = 0; m < kNumMips; ++m)
        {
            sourceA[(size_t) m].assign (static_cast<size_t> (sourceLen), 0.0f);
            sourceB[(size_t) m].assign (static_cast<size_t> (sourceLen), 0.0f);
        }

        modulator.prepare (newSampleRate);
        gainPole     = fastmath::onePoleCoeff (0.005f, newSampleRate);  // ~5 ms de-zip
        attackCoeff  = fastmath::onePoleCoeff (0.008f, newSampleRate);  // ~8 ms attack
        releaseCoeff = fastmath::onePoleCoeff (0.200f, newSampleRate);  // ~200 ms release

        //  DC blocker pole: y[n] = x[n]-x[n-1] + R*y[n-1], R = e^{-2*pi*fc/fs}.
        dcR = fastmath::fastExp (-2.0f * 3.14159265f * 10.0f / static_cast<float> (newSampleRate));

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
        heldCount      = 0;
        arpClock       = 0.0f;
        arpIndex       = 0;
        arpCurrentNote = -1;
        dcX1L = dcY1L = dcX1R = dcY1R = 0.0f;
        sustainPedal = false;
        modWheel     = 0.0f;
        svfL.reset();
        svfR.reset();
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
    //  Mip index for a given playback increment: mip m is alias-free up to
    //  inc = 2^m, so pick m = ceil(log2(inc)) (0 for inc <= 1), clamped.
    int GranularEngine::mipForInc (float inc) noexcept
    {
        int   m = 0;
        float t = 1.0f;
        while (m < kNumMips - 1 && inc > t) { ++m; t *= 2.0f; }
        return m;
    }

    void GranularEngine::fillGenome() noexcept
    {
        if (sourceLen <= 0)
            return;

        constexpr double kPi = 3.14159265358979323846;
        const double w0     = 2.0 * kPi * kGenomeHz / sampleRate;
        const int    nyqK   = static_cast<int> ((sampleRate * 0.5) / kGenomeHz) - 1;
        const int    kFull  = nyqK < 1 ? 1 : (nyqK > 64 ? 64 : nyqK);

        //  Synthesise each mip additively with harmonics limited per octave so
        //  it stays alias-free up to 2^m (mip m keeps k <= kFull >> m). Total
        //  cost is ~2x mip 0 (the harmonic count halves each octave), and this
        //  preserves per-mip brightness better than a cascaded low-pass.
        for (int m = 0; m < kNumMips; ++m)
        {
            const int kMax = std::max (1, kFull >> m);
            float peakA = 1.0e-6f, peakB = 1.0e-6f;
            auto& aBuf = sourceA[(size_t) m];
            auto& bBuf = sourceB[(size_t) m];
            for (int n = 0; n < sourceLen; ++n)
            {
                const double ph = w0 * n;
                double sa = 0.0, sb = 0.0;
                for (int k = 1; k <= kMax; ++k)      sa += std::sin (ph * k) / k;   // saw
                for (int k = 1; k <= kMax; k += 2)   sb += std::sin (ph * k) / k;   // square
                aBuf[(size_t) n] = static_cast<float> (sa);
                bBuf[(size_t) n] = static_cast<float> (sb);
                peakA = std::max (peakA, std::fabs (aBuf[(size_t) n]));
                peakB = std::max (peakB, std::fabs (bBuf[(size_t) n]));
            }
            const float na = 0.9f / peakA, nb = 0.9f / peakB;
            for (int n = 0; n < sourceLen; ++n) { aBuf[(size_t) n] *= na; bBuf[(size_t) n] *= nb; }
        }
    }

    //  Derive octave mips 1.. from mip 0 by repeated band-limiting. Each step
    //  halves the pass-band with a zero-phase (forward+reverse) one-pole cascade,
    //  which is enough to tame imaging on arbitrary loaded samples. Off-thread.
    void GranularEngine::buildMips (MipSet& mips) noexcept
    {
        if (sourceLen <= 1)
            return;

        for (int m = 1; m < kNumMips; ++m)
        {
            //  Start from the previous (already band-limited) mip and lowpass it
            //  another octave. Cutoff halves each step; 4 zero-phase passes give
            //  a steep, phase-neutral roll-off.
            auto& dst = mips[(size_t) m];
            dst = mips[(size_t) (m - 1)];
            const float fc    = 0.5f * static_cast<float> (sampleRate) / static_cast<float> (1 << m);
            const float coeff = fastmath::onePoleCoeff (1.0f / (2.0f * 3.14159265f * std::max (20.0f, fc)),
                                                        sampleRate);
            for (int pass = 0; pass < 4; ++pass)
            {
                float z = dst[0];
                for (int n = 0; n < sourceLen; ++n) { z = dst[(size_t) n] + (z - dst[(size_t) n]) * coeff; dst[(size_t) n] = z; }
                z = dst[(size_t) (sourceLen - 1)];
                for (int n = sourceLen - 1; n >= 0; --n) { z = dst[(size_t) n] + (z - dst[(size_t) n]) * coeff; dst[(size_t) n] = z; }
            }
        }
    }

    //  Cubic Hermite (Catmull-Rom) read from a source ring buffer at fractional
    //  pos. A 4-point cubic rejects far more imaging/aliasing than linear when
    //  grains are pitched, so pitched material stays smooth instead of gritty.
    float GranularEngine::readSource (const MipSet& mips, float pos, int mip) const noexcept
    {
        const int mi = mip < 0 ? 0 : (mip >= kNumMips ? kNumMips - 1 : mip);
        const std::vector<float>& buf = mips[(size_t) mi];
        if (sourceLen <= 3)
            return sourceLen <= 0 ? 0.0f : buf[0];

        const float len = static_cast<float> (sourceLen);

        //  Callers keep readPos wrapped in the advance step, so a branchy guard
        //  replaces the per-read division (hot path: 2 reads/grain/sample).
        float wrapped = pos;
        if (wrapped >= len) wrapped -= len;
        else if (wrapped < 0.0f) wrapped += len;

        const int   i1 = static_cast<int> (wrapped);
        const float f  = wrapped - static_cast<float> (i1);

        int i0 = i1 - 1; if (i0 < 0)          i0 += sourceLen;
        int i2 = i1 + 1; if (i2 >= sourceLen) i2 -= sourceLen;
        int i3 = i1 + 2; if (i3 >= sourceLen) i3 -= sourceLen;

        const float x0 = buf[(size_t) i0];
        const float x1 = buf[(size_t) i1];
        const float x2 = buf[(size_t) i2];
        const float x3 = buf[(size_t) i3];

        //  Catmull-Rom coefficients (tangents = centred differences).
        const float c0 = x1;
        const float c1 = 0.5f * (x2 - x0);
        const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }

    //  pitchMul / ampMul carry the per-note ratio and envelope in instrument
    //  mode; both are 1.0 in effect mode.
    int GranularEngine::spawnGrain (const ParameterSnapshot& p, float modValue,
                                    float pitchMul, float ampMul, float panPos) noexcept
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

                //  Grain stereo position: diploid lean (A left, B right) plus the
                //  caller's unison spread, clamped into [0,1].
                float pan = panPos + (ab - 0.5f) * 0.4f;
                pan = pan < 0.0f ? 0.0f : (pan > 1.0f ? 1.0f : pan);

                //  Anti-alias: choose the band-limited mip for each read rate.
                //  Effect mode captures live audio only into mip 0, so read it.
                const int mipA = instrumentMode ? mipForInc (incA) : 0;
                const int mipB = instrumentMode ? mipForInc (incB) : 0;

                grains[i].trigger (startSample, startSample, incA, incB,
                                   lenSamp, 0.6f * ampMul, ab, pan, mipA, mipB);
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

    void GranularEngine::triggerVoice (int midiNote, float velocity) noexcept
    {
        auto& v = voices[(size_t) allocateVoice (midiNote)];
        const bool wasFree = (v.note < 0);
        v.note  = midiNote;
        v.vel   = velocity < 0.0f ? 0.0f : (velocity > 1.0f ? 1.0f : velocity);
        v.ratio = fastmath::fastExp (static_cast<float> (midiNote - 60) * 0.0577622650f);
        if (wasFree)
            v.curRatio = v.ratio;   // fresh voice starts on-pitch (glide only legato)
        v.gate  = true;             // env ramps up from its current level (click-free on steal)
        v.sustained = false;
    }

    void GranularEngine::releaseVoice (int midiNote) noexcept
    {
        for (auto& v : voices)
            if (v.note == midiNote)
            {
                if (sustainPedal) v.sustained = true;   // pedal holds it
                else              v.gate      = false;  // enter release
            }
    }

    void GranularEngine::setSustain (bool down) noexcept
    {
        sustainPedal = down;
        if (! down)
            for (auto& v : voices)
                if (v.sustained) { v.gate = false; v.sustained = false; }   // release on pedal-up
    }

    void GranularEngine::noteOn (int midiNote, float velocity) noexcept
    {
        const int q = quantize (midiNote);
        if (arpEnabled) { arpAddHeld (q, velocity); return; }
        triggerVoice (q, velocity);
    }

    void GranularEngine::noteOff (int midiNote) noexcept
    {
        const int q = quantize (midiNote);
        if (arpEnabled) { arpRemoveHeld (q); return; }
        releaseVoice (q);
    }

    //  Nearest-pitch-class quantiser (root C). Chromatic when scale == 0.
    int GranularEngine::quantize (int midiNote) const noexcept
    {
        if (scale <= 0)
            return midiNote;

        //  Bit masks of allowed semitones within an octave.
        static constexpr unsigned masks[] = {
            0x0FFF, // 0 chromatic (unused here)
            0x0AB5, // 1 major       {0,2,4,5,7,9,11}
            0x05AD, // 2 minor (nat) {0,2,3,5,7,8,10}
            0x0295, // 3 pent major  {0,2,4,7,9}
            0x06AD, // 4 dorian      {0,2,3,5,7,9,10}
        };
        const int n = static_cast<int> (sizeof (masks) / sizeof (masks[0]));
        const unsigned mask = masks[scale >= n ? n - 1 : scale];

        const int pc = ((midiNote % 12) + 12) % 12;
        for (int d = 0; d < 12; ++d)
        {
            if (mask & (1u << ((pc + d) % 12))) return midiNote + d;
            if (mask & (1u << ((pc - d + 12) % 12))) return midiNote - d;
        }
        return midiNote;
    }

    void GranularEngine::loadGenomeFromSample (const float* mono, int numSamples) noexcept
    {
        if (mono == nullptr || numSamples <= 0 || sourceLen <= 0)
            return;

        float peak = 1.0e-6f;
        auto& a0 = sourceA[0];
        auto& b0 = sourceB[0];
        for (int n = 0; n < sourceLen; ++n)
        {
            const float s = mono[n % numSamples];   // loop to fill
            a0[(size_t) n] = s;
            b0[(size_t) n] = s;
            const float a = s < 0.0f ? -s : s;
            if (a > peak) peak = a;
        }
        const float norm = 0.9f / peak;
        for (int n = 0; n < sourceLen; ++n) { a0[(size_t) n] *= norm; b0[(size_t) n] *= norm; }

        //  Band-limit the higher mips off the loaded waveform (anti-alias).
        buildMips (sourceA);
        buildMips (sourceB);
    }

    void GranularEngine::allNotesOff() noexcept
    {
        heldCount = 0;
        arpCurrentNote = -1;
        for (auto& v : voices)
            v.gate = false;
    }

    //==========================================================================
    //  Arpeggiator
    //==========================================================================
    void GranularEngine::setArp (bool enabled, float rateHz) noexcept
    {
        if (! enabled && arpEnabled && arpCurrentNote >= 0)
        {
            releaseVoice (arpCurrentNote);   // silence the running step on disable
            arpCurrentNote = -1;
        }
        arpEnabled = enabled;
        arpRateHz  = rateHz < 0.1f ? 0.1f : rateHz;
    }

    void GranularEngine::arpAddHeld (int note, float vel) noexcept
    {
        for (int i = 0; i < heldCount; ++i)
            if (heldNote[i] == note) { heldVel[i] = vel; return; }   // already held
        if (heldCount >= kMaxHeld)
            return;
        //  Sorted ascending insert (up pattern).
        int pos = heldCount;
        while (pos > 0 && heldNote[pos - 1] > note)
        {
            heldNote[pos] = heldNote[pos - 1];
            heldVel[pos]  = heldVel[pos - 1];
            --pos;
        }
        heldNote[pos] = note;
        heldVel[pos]  = vel;
        ++heldCount;
    }

    void GranularEngine::arpRemoveHeld (int note) noexcept
    {
        for (int i = 0; i < heldCount; ++i)
        {
            if (heldNote[i] == note)
            {
                for (int j = i; j < heldCount - 1; ++j)
                {
                    heldNote[j] = heldNote[j + 1];
                    heldVel[j]  = heldVel[j + 1];
                }
                --heldCount;
                return;
            }
        }
    }

    void GranularEngine::arpStep() noexcept
    {
        if (arpCurrentNote >= 0)
        {
            releaseVoice (arpCurrentNote);
            arpCurrentNote = -1;
        }
        if (heldCount <= 0)
            return;

        if (arpIndex >= heldCount || arpIndex < 0)
            arpIndex = 0;
        const int   note = heldNote[arpIndex];
        const float vel  = heldVel[arpIndex];
        triggerVoice (note, vel);
        arpCurrentNote = note;

        //  Advance the step cursor per pattern.
        switch (arpMode)
        {
            case 1: // down
                arpIndex = (arpIndex - 1 + heldCount) % heldCount;
                break;
            case 2: // up-down (bounce, no repeated endpoints)
                if (heldCount == 1) { arpIndex = 0; }
                else
                {
                    arpIndex += arpDir;
                    if (arpIndex >= heldCount - 1) { arpIndex = heldCount - 1; arpDir = -1; }
                    else if (arpIndex <= 0)        { arpIndex = 0;             arpDir =  1; }
                }
                break;
            case 3: // random
                arpIndex = static_cast<int> (nextRandom() * static_cast<float> (heldCount));
                if (arpIndex >= heldCount) arpIndex = heldCount - 1;
                break;
            default: // up
                arpIndex = (arpIndex + 1) % heldCount;
                break;
        }
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

        //  Arpeggiator + scale control.
        if (instrumentMode)
        {
            setArp     (p.arpOn > 0.5f, 0.5f + p.arpRate * 19.5f);
            setArpMode (static_cast<int> (p.arpMode * 3.999f));
            setArpSync (p.arpSync > 0.5f, p.arpRate);   // arpRate doubles as sync division
            setScale   (static_cast<int> (p.scaleType * 4.999f));
        }

        //  Grains-per-second from density (2 .. 200 gr/s).
        const float grainsPerSec = 2.0f + p.grainDensity * 198.0f;
        const float spawnPeriod  = static_cast<float> (sampleRate) / grainsPerSec;

        //  Overlap normalisation: many concurrent grains sum incoherently, so
        //  the bus RMS grows ~sqrt(overlap). Scaling each grain by 1/sqrt(overlap)
        //  keeps loudness constant across density and stops the summed cloud from
        //  slamming the soft-clipper into distortion.
        const float grainMs      = kMinGrainMs + p.grainSize * (kMaxGrainMs - kMinGrainMs);
        const float grainLenSec  = grainMs * 0.001f;
        const float overlap      = grainsPerSec * grainLenSec;
        const float overlapGain  = 1.0f / std::sqrt (overlap < 1.0f ? 1.0f : overlap);
        const float srcLenF      = static_cast<float> (sourceLen);

        //  --- Unison stack (per block) ----------------------------------------
        const int   nUnison    = 1 + static_cast<int> (p.unison * 6.0f + 0.5f);   // 1..7
        const float unisonGain = 1.0f / std::sqrt (static_cast<float> (nUnison));
        const float detuneCents = p.unisonDetune * 50.0f;                          // spread

        //  --- Tone stage (per block) ------------------------------------------
        //  Cutoff: log map ~20 Hz .. ~20 kHz. Resonance -> Q -> damping k = 1/Q.
        const float baseCutoff = 20.0f * fastmath::fastExp (p.filterCutoff * 6.9077f);
        const float filterQ    = 0.5f + p.filterReso * 9.5f;
        const float kDamp      = 1.0f / filterQ;
        const float fType      = p.filterType;
        const float modOctaves = p.filterMod * 3.0f;             // capillary -> cutoff (±oct)
        const float driveGain  = 1.0f + p.drive * 7.0f;
        const float driveMakeup= 1.0f / fastmath::fastTanh (driveGain);
        const float width      = p.stereoWidth * 2.0f;           // 0 (mono) .. 2 (wide)
        const float lnHalf     = -0.6931471806f;                 // ln(1/2) for octave scaling

        for (int n = 0; n < numSamples; ++n)
        {
            //  1) Effect mode captures the incoming genome; instrument mode keeps
            //     the internal wavetable genome and advances the note envelopes.
            if (! instrumentMode)
            {
                //  Live capture goes to mip 0; effect-mode grains read mip 0.
                sourceA[0][(size_t) writeHead] = inL ? inL[n] : 0.0f;
                sourceB[0][(size_t) writeHead] = inR ? inR[n] : (inL ? inL[n] : 0.0f);
                if (++writeHead >= sourceLen) writeHead = 0;
            }
            else
            {
                advanceVoices();

                //  Arpeggiator clock: sequence held notes at arpRateHz.
                if (arpEnabled)
                {
                    arpClock -= 1.0f;
                    if (arpClock <= 0.0f)
                    {
                        arpStep();
                        const float rate = arpSync ? arpSyncedRate (hostBpm, arpDiv01) : arpRateHz;
                        arpClock += static_cast<float> (sampleRate) / (rate < 0.1f ? 0.1f : rate);
                    }
                }
            }

            //  2) Advance the non-linear modulator.
            const float modValue = modulator.processSample();

            //  3) Schedule new grains. In instrument mode each grain is bound to
            //     a sounding voice (pitch ratio + envelope); if nothing sounds,
            //     no grain is spawned.
            grainClock -= 1.0f;
            if (grainClock <= 0.0f)
            {
                float pitchMul = 1.0f;
                float baseAmp  = 0.0f;
                bool  doSpawn  = false;

                if (instrumentMode)
                {
                    const int vi = pickVoice();
                    if (vi >= 0)
                    {
                        const auto& v = voices[(size_t) vi];
                        pitchMul = v.curRatio * bendRatio;
                        baseAmp  = v.env * (0.15f + 0.85f * v.vel);
                        doSpawn  = true;
                    }
                }
                else
                {
                    pitchMul = 1.0f;
                    baseAmp  = 1.0f;
                    doSpawn  = true;
                }

                if (doSpawn)
                {
                    //  Stack nUnison detuned, stereo-spread grains per trigger.
                    for (int u = 0; u < nUnison; ++u)
                    {
                        const float t      = (nUnison > 1)
                                           ? (static_cast<float> (u) / static_cast<float> (nUnison - 1) - 0.5f)
                                           : 0.0f;
                        const float detune = fastmath::fastExp (t * 2.0f * detuneCents * 0.0005776226f);
                        const float pan    = 0.5f + t * 0.8f;
                        spawnGrain (p, modValue, pitchMul * detune,
                                    baseAmp * overlapGain * unisonGain, pan);
                    }
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

                const float a = readSource (sourceA, g.readPosA, g.mipA);
                const float b = readSource (sourceB, g.readPosB, g.mipB);

                float gA, gB;
                fastmath::equalPowerPair (g.blend, gA, gB);
                const float s = (a * gA + b * gB) * w * g.amp;

                //  Constant-power stereo placement from the grain's pan (which
                //  already folds in the diploid lean + unison spread).
                float pl, pr;
                fastmath::equalPowerPair (g.pan, pl, pr);
                accL += s * pl;
                accR += s * pr;

                g.readPosA += g.incA;
                g.readPosB += g.incB;
                if (g.readPosA >= srcLenF) g.readPosA -= srcLenF;   // keep in [0,len)
                if (g.readPosB >= srcLenF) g.readPosB -= srcLenF;
                g.phase    += g.phaseInc;
                if (g.phase >= 1.0f)
                    g.active = false;
            }
            liveGrainCount = live;

            //  --- Tone stage: drive -> ZDF filter -> stereo width -------------
            //  Analog-style saturation (odd-harmonic warmth, level-normalised).
            float tL = fastmath::fastTanh (accL * driveGain) * driveMakeup;
            float tR = fastmath::fastTanh (accR * driveGain) * driveMakeup;

            //  Capillary-modulated cutoff, per sample, shared L/R; mod wheel
            //  lifts the cutoff up to +2 octaves (1.386 = 2*ln2).
            const float cutoff = baseCutoff
                * fastmath::fastExp ((modValue - 0.5f) * modOctaves * (-lnHalf) * 2.0f)
                * fastmath::fastExp (modWheel * 1.386294f);
            const float g = SVF::gForCutoff (cutoff, static_cast<float> (sampleRate));
            tL = svfL.process (tL, g, kDamp, fType);
            tR = svfR.process (tR, g, kDamp, fType);

            //  Mid/side stereo width.
            const float mid  = (tL + tR) * 0.5f;
            const float side = (tL - tR) * 0.5f * width;
            const float sL   = mid + side;
            const float sR   = mid - side;

            //  Per-sample gain smoothing removes zipper noise on automation.
            smoothedGain = p.outputGain + (smoothedGain - p.outputGain) * gainPole;

            //  DC / subsonic blocker, then gentle soft-clip as a safety ceiling.
            const float xL = sL * smoothedGain;
            const float yL = xL - dcX1L + dcR * dcY1L;
            dcX1L = xL; dcY1L = yL;

            const float xR = sR * smoothedGain;
            const float yR = xR - dcX1R + dcR * dcY1R;
            dcX1R = xR; dcY1R = yR;

            if (outL) outL[n] = fastmath::softClip (yL);
            if (outR) outR[n] = fastmath::softClip (yR);
        }
    }
}
