#pragma once

//==============================================================================
//  GranularEngine.h
//
//  Diploid (two-chromosome) granular cross-synthesis engine.
//
//  Two pre-allocated source ring buffers capture the incoming stereo genome
//  (A = left, B = right). A fixed pool of Grain voices is scheduled from the
//  capillary modulator; each grain fractionally reads BOTH buffers and blends
//  them, producing continuous cross-synthesis between the two chromosomes.
//
//  Lock-free contract:
//    * All buffers allocated in prepare(); processBlock() never allocates.
//    * Parameters arrive via the atomic ParameterHub, snapshotted once/block.
//    * No std::sin, no locks, no exceptions on the audio thread.
//==============================================================================

#include "Grain.h"
#include "CapillaryModulator.h"
#include "ParameterHub.h"
#include <vector>
#include <cstdint>

namespace phenotype::dsp
{
    class GranularEngine
    {
    public:
        static constexpr int   kMaxGrains        = 128;
        static constexpr int   kMaxVoices        = 16;
        static constexpr float kSourceSeconds    = 4.0f;   // capture / genome window
        static constexpr float kMinGrainMs       = 8.0f;
        static constexpr float kMaxGrainMs       = 400.0f;
        static constexpr float kGenomeHz         = 261.6256f; // C4 -> note 60 == ratio 1

        GranularEngine() = default;

        //  Allocation is permitted here (called off the audio thread).
        void prepare (double sampleRate, int maxBlockSize);

        void reset() noexcept;

        //  Real-time entry point. In effect mode reads `numSamples` of stereo
        //  input from (inL,inR) as the genome; in instrument mode the input is
        //  ignored and the internal wavetable genome is granulated per note.
        //  Writes the granulated cross-synthesis to (outL,outR). Buffers may
        //  alias in place. Allocation-free.
        void process (const float* inL, const float* inR,
                      float* outL, float* outR,
                      int numSamples) noexcept;

        //  --- Melodic (instrument) interface ----------------------------------
        //  Switches between granulating live input (effect) and granulating the
        //  internal wavetable genome under MIDI control (instrument).
        void setInstrumentMode (bool shouldBeInstrument) noexcept { instrumentMode = shouldBeInstrument; }
        [[nodiscard]] bool isInstrumentMode() const noexcept { return instrumentMode; }

        void noteOn  (int midiNote, float velocity) noexcept;
        void noteOff (int midiNote) noexcept;
        void allNotesOff() noexcept;
        [[nodiscard]] int activeVoices() const noexcept;

        //  Global pitch bend (semitones) and portamento (seconds, 0 = off).
        void setPitchBend (float semitones) noexcept
        {
            bendRatio = fastmath::fastExp (semitones * 0.0577622650f);
        }
        void setGlideTime (float seconds) noexcept
        {
            glideCoeff = seconds <= 0.0f ? 0.0f : fastmath::onePoleCoeff (seconds, sampleRate);
        }

        //  Bridge to the shared atomic parameter store.
        ParameterHub& params() noexcept { return hub; }

        //  Snapshot of visualiser telemetry for the UI (grain activity, phase).
        [[nodiscard]] float capillaryLevel() const noexcept { return modulator.getCurrentLevel(); }
        [[nodiscard]] int   activeGrains()   const noexcept { return liveGrainCount; }

    private:
        //  One held note. Pitch is baked as a playback ratio relative to the
        //  genome's root; amplitude follows a gated attack/release envelope.
        struct MelodyVoice
        {
            int   note     = -1;    // -1 == free
            float ratio    = 1.0f;  // target 2^((note-60)/12)
            float curRatio = 1.0f;  // glided current ratio
            float vel      = 0.0f;  // 0..1
            float env      = 0.0f;  // current envelope level
            bool  gate     = false; // key held
        };

        int   spawnGrain (const ParameterSnapshot& p, float modValue,
                          float pitchMul, float ampMul) noexcept;
        float readSource (const std::vector<float>& buf, float pos) const noexcept;
        void  fillGenome() noexcept;                 // band-limited wavetables -> source A/B
        void  advanceVoices() noexcept;              // per-sample envelope integration
        int   pickVoice() noexcept;                  // round-robin over sounding voices
        int   allocateVoice (int note) noexcept;     // free slot, else steal

        //  Fast, deterministic, allocation-free RNG (xorshift32) for spray.
        [[nodiscard]] float nextRandom() noexcept
        {
            rngState ^= rngState << 13;
            rngState ^= rngState >> 17;
            rngState ^= rngState << 5;
            return static_cast<float> (rngState & 0xFFFFFF) / 16777216.0f;
        }

        double sampleRate  = 44100.0;
        int    sourceLen   = 0;
        int    writeHead   = 0;

        std::vector<float> sourceA;   // chromosome A ring
        std::vector<float> sourceB;   // chromosome B ring

        Grain  grains[kMaxGrains];
        int    liveGrainCount = 0;

        CapillaryModulator modulator;
        ParameterHub       hub;

        MelodyVoice voices[kMaxVoices];
        bool     instrumentMode = false;
        float    attackCoeff  = 0.0f; // one-pole attack pole
        float    releaseCoeff = 0.0f; // one-pole release pole
        float    bendRatio    = 1.0f; // global pitch-bend multiplier
        float    glideCoeff   = 0.0f; // portamento pole (0 = instant)
        int      voiceRR      = 0;    // round-robin cursor for grain assignment

        float    grainClock   = 0.0f; // fractional samples until next spawn
        float    smoothedGain = 0.0f; // de-zippered master gain
        float    gainPole     = 0.0f; // one-pole coeff (~5 ms)
        uint32_t rngState     = 0x9E3779B9u;
    };
}
