#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// LFSRNoise — Linear-Feedback Shift Register pseudo-random white noise.
//
// A 32-bit maximal-length Galois LFSR (taps 32,22,2,1). This is the shared
// noise primitive used by the snare "wires" (§4.2) and the hi-hat generator
// (§4.3). Using an LFSR rather than a library RNG keeps the grainy, slightly
// periodic character of the original analog/CPU noise source.
// ---------------------------------------------------------------------------
class LFSRNoise
{
public:
    explicit LFSRNoise (uint32_t seed = 0xACE1u) : state (seed ? seed : 0xACE1u) {}

    void reset (uint32_t seed = 0xACE1u) noexcept { state = seed ? seed : 0xACE1u; }

    // Advance one step, return a bipolar sample in [-1, 1).
    inline float nextSample() noexcept
    {
        const uint32_t lsb = state & 1u;
        state >>= 1;
        if (lsb) state ^= 0x80200003u; // Galois feedback mask
        // Map top 24 bits to [-1, 1)
        const uint32_t v = state & 0x00FFFFFFu;
        return (static_cast<float> (v) / 8388608.0f) - 1.0f;
    }

private:
    uint32_t state;
};
