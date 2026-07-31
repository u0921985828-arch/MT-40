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
        hpState = 0.0f;
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

    /** Mixer -> filter overdrive (0 = clean/linear input, up = growl). */
    void setInputDrive (float driveAmount) noexcept
    {
        inputDrive = juce::jlimit (0.0f, 1.0f, driveAmount);
    }

    /** Resonance-dependent bass-thinning (0 = full lows, 1 = strong thinning). */
    void setBassThin (float amount) noexcept
    {
        bassThin = juce::jlimit (0.0f, 1.0f, amount);
    }

    /** Analog "true-circuit" character (0..1), plus a per-voice tolerance seed
        (0..1). Models two real-world imperfections of a discrete transistor
        ladder: (1) component tolerance detunes the effective cutoff a little per
        unit, and (2) the ladder's saturation is *asymmetric* (the transistors
        clip harder on one polarity), which adds even-order harmonics — the warm,
        slightly tube-like edge a symmetric tanh can never produce. */
    void setAnalog (float amount, float seed) noexcept
    {
        analogAmt = juce::jlimit (0.0f, 1.0f, amount);
        const float s = seed * 2.0f - 1.0f;                 // -1..1, stable per voice
        tolG     = 1.0f + s * 0.02f * analogAmt;            // +-2% cutoff tolerance
        asym     = s * 0.30f * analogAmt;                    // asymmetric saturation bias
        tanhBias = std::tanh (asym);                         // keeps the feedback DC-centred
        updateCoefficients();
    }

    inline float processSample (float input) noexcept
    {
        // Non-linear input drive (the classic Minimoog "growl" when the mixer
        // pushes the filter input). Clean and unity when inputDrive == 0.
        float driven = input;
        if (inputDrive > 0.0001f)
            driven = std::tanh (driven * (1.0f + inputDrive * 5.0f));

        // Resonance-dependent bass-thinning via a one-pole high-pass blend.
        const float thin = bassThin * resonance;
        if (thin > 0.0001f)
        {
            hpState += hpCoef * (driven - hpState);
            driven -= thin * hpState;
        }

        float output = 0.0f;

        // 2x oversampling of the non-linear difference equation.
        for (int os = 0; os < 2; ++os)
        {
            // The through-path stays linear (accurate tuning, unity passband);
            // the soft transistor non-linearity lives in the resonance feedback.
            const float xin = driven;

            // --- Analytic zero-delay-feedback solve --------------------------
            // Each one-pole stage:  y = G*u + (1-G)*z   ->   b_i = (1-G)*z_i.
            const float b1 = oneMinusG * z[0];
            const float b2 = oneMinusG * z[1];
            const float b3 = oneMinusG * z[2];
            const float b4 = oneMinusG * z[3];

            // State contribution propagated to the 4th stage output.
            const float S = G3 * b1 + G2 * b2 + G * b3 + b4;

            // Linear predictor for the 4th-stage output, then saturate the
            // feedback: keeps self-oscillation perfectly in tune yet bounded.
            // With analog character engaged the saturation is asymmetric (even
            // harmonics); at zero it stays the exact symmetric tanh as before.
            const float y4 = (G4 * xin + S) / (1.0f + k * G4);
            const float fb = analogAmt > 0.0001f ? (std::tanh (y4 + asym) - tanhBias)
                                                  : std::tanh (y4);
            const float u1 = xin - k * fb;

            // Run the four cascaded TPT one-poles with the resolved input.
            float u = u1;
            for (int s = 0; s < 4; ++s)
            {
                const float v = (u - z[s]) * G;
                const float y = v + z[s];
                z[s] = y + v;   // trapezoidal integrator state update
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
        const double g = std::tan (juce::MathConstants<double>::pi * cutoff / fsOversampled)
                             * (double) tolG; // per-voice component tolerance on the cutoff
        G = (float) (g / (1.0 + g));
        oneMinusG = 1.0f - G;
        G2 = G * G;
        G3 = G2 * G;
        G4 = G2 * G2;

        // ~150 Hz one-pole for the bass-thinning high-pass.
        hpCoef = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 150.0 / fs));
    }

    double fs { 44100.0 };
    float  cutoff { 1000.0f };
    float  resonance { 0.0f };
    float  k { 0.0f };            // feedback amount (0..~4.2)
    float  inputDrive { 0.0f };   // mixer -> filter overdrive
    float  bassThin { 0.0f };     // resonance-dependent bass-thinning
    float  analogAmt { 0.0f };    // "true-circuit" character amount
    float  tolG { 1.0f };         // per-voice cutoff tolerance multiplier
    float  asym { 0.0f };         // asymmetric saturation bias (even harmonics)
    float  tanhBias { 0.0f };     // tanh(asym), removes the resulting DC offset
    float  hpState { 0.0f }, hpCoef { 0.03f };

    float  G { 0.0f }, oneMinusG { 1.0f }, G2 { 0.0f }, G3 { 0.0f }, G4 { 0.0f };
    float  z[4] { 0.0f, 0.0f, 0.0f, 0.0f };
};
