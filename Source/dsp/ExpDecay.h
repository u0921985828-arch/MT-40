#pragma once

#include <cmath>

// ---------------------------------------------------------------------------
// ExpDecay — one-shot exponential decay envelope for percussion VCAs.
//
// Triggered instantaneously to 1.0, then decays exponentially. Used by the
// snare noise tail, hi-hat closed/open VCAs, and the Sleng Teng bass pluck.
// The choke-group logic (§4.3) resets the level to 0 with hardReset().
// ---------------------------------------------------------------------------
class ExpDecay
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; recalc(); }

    // Time to decay ~-60 dB.
    void setDecay (double seconds) noexcept { decayTime = seconds; recalc(); }

    void trigger() noexcept { level = 1.0f; }

    void hardReset() noexcept { level = 0.0f; } // choke

    bool isActive() const noexcept { return level > 1.0e-4f; }

    float value() const noexcept { return level; }

    inline float getNextSample() noexcept
    {
        const float out = level;
        level *= coeff;
        if (level < 1.0e-5f) level = 0.0f;
        return out;
    }

private:
    void recalc() noexcept
    {
        coeff = (decayTime > 0.0)
            ? static_cast<float> (std::exp (-6.9077553 / (decayTime * fs)))
            : 0.0f;
    }

    double fs = 44100.0;
    double decayTime = 0.1;
    float coeff = 0.999f;
    float level = 0.0f;
};
