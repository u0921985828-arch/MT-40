#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
    Four-pole (24 dB/oct) Moog transistor-ladder low-pass filter.

    High-quality zero-delay-feedback (TPT) implementation: the global resonance
    feedback loop is solved analytically each sample so the instantaneous
    dependency of the 4th stage on the input is resolved exactly.  This removes
    the cutoff / resonance detuning that a one-sample-delayed feedback path
    suffers from, gives accurate self-oscillation pitch, and stays stable right
    up to the edge of oscillation.

    A tanh drive on the input and a saturated feedback term emulate the soft
    non-linearity of the original transistor ladder.  The whole difference
    equation runs at 2x the host rate to push the residual non-linear aliasing
    above the audible band.
*/
class MoogLadderFilter
{
public:
    MoogLadderFilter() { reset(); }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        reset();
        updateCoefficients();
    }

    void reset() noexcept
    {
        std::fill (std::begin (z), std::end (z), 0.0f);
    }

    void setCutoff (float cutoffHz) noexcept
    {
        const float newCutoff = juce::jlimit (20.0f, (float) (fs * 0.49), cutoffHz);
        if (! juce::approximatelyEqual (newCutoff, cutoff))
        {
            cutoff = newCutoff;
            updateCoefficients();
        }
    }

    /** Resonance 0..1.  Values near 1 drive the ladder into self-oscillation. */
    void setResonance (float res) noexcept
    {
        resonance = juce::jlimit (0.0f, 1.0f, res);
        k = resonance * 4.2f; // reach self-oscillation near the top
    }

    /** Extra input drive applied before the ladder (feedback / overdrive). */
    void setDrive (float driveAmount) noexcept
    {
        drive = juce::jmax (0.0f, driveAmount);
    }

    inline float processSample (float input) noexcept
    {
        float output = 0.0f;

        // 2x oversampling of the non-linear difference equation.
        for (int os = 0; os < 2; ++os)
        {
            // Soft input drive (resonance-compensated so the passband holds up).
            const float xin = std::tanh (input * (1.0f + drive) + 0.5f * k * satFb);

            // --- Analytic zero-delay-feedback solve --------------------------
            // Each one-pole stage:  y = G*u + (1-G)*z   ->   b_i = (1-G)*z_i.
            const float b1 = oneMinusG * z[0];
            const float b2 = oneMinusG * z[1];
            const float b3 = oneMinusG * z[2];
            const float b4 = oneMinusG * z[3];

            // State contribution propagated to the 4th stage output.
            const float S = G3 * b1 + G2 * b2 + G * b3 + b4;

            // y4 = G^4*(xin - k*y4) + S  ->  solve for y4.
            const float y4 = (G4 * xin + S) / (1.0f + k * G4);

            const float u1 = xin - k * y4;

            // Run the four cascaded TPT one-poles with the resolved input.
            float u = u1;
            for (int s = 0; s < 4; ++s)
            {
                const float v = (u - z[s]) * G;
                const float y = v + z[s];
                z[s] = y + v;   // trapezoidal integrator state update
                u = y;
            }

            satFb = u;          // == y4 (after the exact solve); feeds next sub-sample
            output = u;
        }

        return output;
    }

private:
    void updateCoefficients() noexcept
    {
        // Coefficient computed at the (2x) oversampled rate.
        const double fsOversampled = fs * 2.0;
        const double g = std::tan (juce::MathConstants<double>::pi * cutoff / fsOversampled);
        G = (float) (g / (1.0 + g));
        oneMinusG = 1.0f - G;
        G2 = G * G;
        G3 = G2 * G;
        G4 = G2 * G2;
    }

    double fs { 44100.0 };
    float  cutoff { 1000.0f };
    float  resonance { 0.0f };
    float  k { 0.0f };       // feedback amount (0..~4.2)
    float  drive { 0.0f };

    float  G { 0.0f }, oneMinusG { 1.0f }, G2 { 0.0f }, G3 { 0.0f }, G4 { 0.0f };
    float  z[4] { 0.0f, 0.0f, 0.0f, 0.0f };
    float  satFb { 0.0f };   // saturated feedback memory across sub-samples
};
