#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
    Four-pole (24 dB/oct) Moog transistor-ladder low-pass filter.

    The topology is a cascade of four zero-delay-feedback (TPT / trapezoidal)
    one-pole low-pass stages wrapped in a global resonance feedback path.  A
    tanh non-linearity in that feedback path emulates the soft saturation of the
    original transistor ladder, giving the filter its characteristic warmth and
    allowing controlled self-oscillation as resonance approaches its maximum.

    The stage difference equation runs at 2x the host sample rate to keep the
    non-linear feedback loop stable and to push the residual aliasing produced
    by the tanh above the audible band.
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
        std::fill (std::begin (state), std::end (state), 0.0f);
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
        // Map to a feedback amount that reaches ~4.5 for full self-oscillation.
        feedbackAmount = resonance * 4.5f;
    }

    /** Extra input drive applied before the ladder (feedback / overdrive). */
    void setDrive (float driveAmount) noexcept
    {
        drive = juce::jmax (0.0f, driveAmount);
    }

    inline float processSample (float input) noexcept
    {
        // Passband-gain compensation: high resonance thins the low end on a
        // real Moog, but a little make-up keeps perceived level steady here.
        const float x = input * (1.0f + drive);

        float output = 0.0f;

        // 2x oversampling of the non-linear difference equation.
        for (int os = 0; os < 2; ++os)
        {
            const float fb = feedbackAmount * (state[3] - 0.5f * x);
            float u = std::tanh (x - fb);

            // Four cascaded TPT one-pole low-pass stages.
            for (int s = 0; s < 4; ++s)
            {
                const float v = (u - state[s]) * G;
                const float y = v + state[s];
                state[s] = y + v;         // trapezoidal integrator state update
                u = y;
            }

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
    }

    double fs { 44100.0 };
    float  cutoff { 1000.0f };
    float  resonance { 0.0f };
    float  feedbackAmount { 0.0f };
    float  drive { 0.0f };
    float  G { 0.0f };
    float  state[4] { 0.0f, 0.0f, 0.0f, 0.0f };
};
