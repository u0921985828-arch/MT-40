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

        //  Arpeggiator: when enabled, held notes are sequenced at rateHz (free)
        //  or tempo-synced. Modes: 0 up, 1 down, 2 up-down, 3 random.
        void setArp (bool enabled, float rateHz) noexcept;
        void setArpMode (int mode) noexcept { arpMode = mode < 0 ? 0 : (mode > 3 ? 3 : mode); }
        void setArpSync (bool synced, float division01) noexcept
        {
            arpSync = synced;
            arpDiv01 = division01 < 0.0f ? 0.0f : (division01 > 1.0f ? 1.0f : division01);
        }
        void setHostBpm (double bpm) noexcept { hostBpm = bpm > 1.0 ? bpm : 120.0; }

        //  Musical scale quantiser. 0 = chromatic (off), 1.. = major, minor,
        //  pentatonic, dorian. Root is C.
        void setScale (int scaleIndex) noexcept { scaleIndex = scaleIndex < 0 ? 0 : scaleIndex; scale = scaleIndex; }
        [[nodiscard]] int quantize (int midiNote) const noexcept;

        //  Replace the internal wavetable genome with a user sample (mono).
        //  Looped to fill and peak-normalised. Off the audio thread.
        void loadGenomeFromSample (const float* mono, int numSamples) noexcept;

        //  Tempo-synced arp rate: division01 -> {1/4,1/8,1/16,1/32} steps/beat.
        [[nodiscard]] static float arpSyncedRate (double bpm, float division01) noexcept
        {
            const int steps[4] = { 1, 2, 4, 8 };
            int idx = static_cast<int> (division01 * 3.999f);
            idx = idx < 0 ? 0 : (idx > 3 ? 3 : idx);
            return static_cast<float> (bpm / 60.0) * static_cast<float> (steps[idx]);
        }

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
        void  triggerVoice (int note, float vel) noexcept;  // sound a note now
        void  releaseVoice (int note) noexcept;             // release a sounding note
        void  arpAddHeld (int note, float vel) noexcept;
        void  arpRemoveHeld (int note) noexcept;
        void  arpStep() noexcept;

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

        static constexpr int kMaxHeld = 32;

        MelodyVoice voices[kMaxVoices];
        int      heldNote[kMaxHeld] {};
        float    heldVel[kMaxHeld]  {};
        int      heldCount     = 0;
        bool     arpEnabled    = false;
        float    arpRateHz     = 8.0f;
        float    arpClock      = 0.0f;
        int      arpIndex      = 0;
        int      arpCurrentNote = -1;
        int      arpMode       = 0;      // 0 up, 1 down, 2 up-down, 3 random
        int      arpDir        = 1;      // up-down direction
        bool     arpSync       = false;
        float    arpDiv01      = 0.5f;
        double   hostBpm       = 120.0;
        int      scale         = 0;      // 0 = chromatic

        bool     instrumentMode = false;
        float    attackCoeff  = 0.0f; // one-pole attack pole
        float    releaseCoeff = 0.0f; // one-pole release pole
        float    bendRatio    = 1.0f; // global pitch-bend multiplier
        float    glideCoeff   = 0.0f; // portamento pole (0 = instant)
        int      voiceRR      = 0;    // round-robin cursor for grain assignment

        float    grainClock   = 0.0f; // fractional samples until next spawn
        float    smoothedGain = 0.0f; // de-zippered master gain
        float    gainPole     = 0.0f; // one-pole coeff (~5 ms)

        //  Output DC / subsonic blocker (one-pole highpass, ~10 Hz).
        float    dcR    = 0.0f;
        float    dcX1L  = 0.0f, dcY1L = 0.0f;
        float    dcX1R  = 0.0f, dcY1R = 0.0f;

        uint32_t rngState     = 0x9E3779B9u;
    };
}
