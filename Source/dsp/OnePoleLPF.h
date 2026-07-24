#pragma once

#include <cmath>

// ---------------------------------------------------------------------------
// OnePoleLPF — 1st-order (6 dB/oct) IIR low-pass.
//
// Models the static hardware RC low-pass that sits after the raw DCO output
// (§3.2). One-pole, no resonance; the coefficient is derived from the analog
// RC time constant so cutoff behaves musically across the sample-rate range.
// ---------------------------------------------------------------------------
class OnePoleLPF
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        setCutoff (currentCutoff);
        reset();
    }

    void reset() noexcept { z1 = 0.0f; }

    void setCutoff (double fc) noexcept
    {
        currentCutoff = fc;
        if (fs <= 0.0) return;
        // Bilinear-ish one-pole: a = exp(-2*pi*fc/fs)
        const double a = std::exp (-2.0 * M_PI * fc / fs);
        b = static_cast<float> (1.0 - a);
        coeffA = static_cast<float> (a);
    }

    inline float process (float x) noexcept
    {
        z1 = b * x + coeffA * z1;
        return z1;
    }

private:
    double fs = 44100.0;
    double currentCutoff = 4000.0;
    float coeffA = 0.0f;
    float b = 1.0f;
    float z1 = 0.0f;
};
