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
        static constexpr float kSourceSeconds    = 4.0f;   // capture window
        static constexpr float kMinGrainMs       = 8.0f;
        static constexpr float kMaxGrainMs       = 400.0f;

        GranularEngine() = default;

        //  Allocation is permitted here (called off the audio thread).
        void prepare (double sampleRate, int maxBlockSize);

        void reset() noexcept;

        //  Real-time entry point. Reads `numSamples` of stereo input from
        //  (inL,inR), writes the granulated cross-synthesis to (outL,outR).
        //  Buffers may alias in place. Allocation-free.
        void process (const float* inL, const float* inR,
                      float* outL, float* outR,
                      int numSamples) noexcept;

        //  Bridge to the shared atomic parameter store.
        ParameterHub& params() noexcept { return hub; }

        //  Snapshot of visualiser telemetry for the UI (grain activity, phase).
        [[nodiscard]] float capillaryLevel() const noexcept { return modulator.getCurrentLevel(); }
        [[nodiscard]] int   activeGrains()   const noexcept { return liveGrainCount; }

    private:
        int   spawnGrain (const ParameterSnapshot& p, float modValue) noexcept;
        float readSource (const std::vector<float>& buf, float pos) const noexcept;

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

        float    grainClock = 0.0f;   // fractional samples until next spawn
        uint32_t rngState   = 0x9E3779B9u;
    };
}
