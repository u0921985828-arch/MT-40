#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/**
    Band-limited virtual-analog oscillator.

    Five classic Minimoog waveforms are generated from a single phase
    accumulator.  Aliasing on the discontinuous shapes (saw / square / pulse)
    is suppressed with PolyBLEP (Polynomial Band-Limited step) residuals; the
    triangle is produced by leaky-integrating the band-limited square, so it is
    aliasing-free as well.
*/
class MoogOscillator
{
public:
    enum class Waveform
    {
        triangle = 0,
        saw,
        square,
        widePulse,
        narrowPulse
    };

    MoogOscillator() = default;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        reset();
    }

    void reset() noexcept
    {
        phase        = 0.0;
        triIntegrator = 0.0f;
    }

    void setWaveform (Waveform w) noexcept        { waveform = w; }
    void setFrequency (double freqHz) noexcept    { frequency = freqHz; }

    /** Generates the next sample.  Output is nominally in [-1, 1]. */
    inline float getNextSample() noexcept
    {
        const double dt = frequency / fs;   // normalised phase increment (cycles/sample)
        const float  t  = (float) phase;    // current phase in [0, 1)

        float value = 0.0f;

        switch (waveform)
        {
            case Waveform::saw:
                value = naiveSaw (t) - (float) polyBlep (t, dt);
                break;

            case Waveform::square:
                value = pulse (t, dt, 0.5f);
                break;

            case Waveform::widePulse:
                value = pulse (t, dt, 0.65f);
                break;

            case Waveform::narrowPulse:
                value = pulse (t, dt, 0.85f);
                break;

            case Waveform::triangle:
            {
                // Integrate the band-limited square to obtain a clean triangle.
                const float sq = pulse (t, dt, 0.5f);
                triIntegrator = (float) (4.0 * dt) * sq + (1.0f - (float) dt) * triIntegrator;
                value = triIntegrator;
                break;
            }
        }

        advance (dt);
        return value;
    }

    /** Advances the phase without producing output (used by the LFO path). */
    inline float getNextLfoSample() noexcept
    {
        // A pure, alias-free triangle/sine-ish shape is plenty for modulation.
        const double dt = frequency / fs;
        const float  s  = std::sin (juce::MathConstants<float>::twoPi * (float) phase);
        advance (dt);
        return s;
    }

    double getPhase() const noexcept { return phase; }

private:
    inline void advance (double dt) noexcept
    {
        phase += dt;
        while (phase >= 1.0) phase -= 1.0;
        while (phase <  0.0) phase += 1.0;
    }

    static inline float naiveSaw (float t) noexcept
    {
        return 2.0f * t - 1.0f;
    }

    // Band-limited rectangular pulse of the given duty cycle (pulseWidth).
    inline float pulse (float t, double dt, float pulseWidth) noexcept
    {
        float value = (t < pulseWidth) ? 1.0f : -1.0f;

        // Correct the two discontinuities: rising edge at 0, falling edge at pw.
        value += (float) polyBlep (t, dt);

        float t2 = t + (1.0f - pulseWidth);
        if (t2 >= 1.0f) t2 -= 1.0f;
        value -= (float) polyBlep (t2, dt);

        return value;
    }

    // PolyBLEP residual for a unit step at phase 0.
    static inline double polyBlep (double t, double dt) noexcept
    {
        if (dt <= 0.0)
            return 0.0;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    double   fs        { 44100.0 };
    double   phase     { 0.0 };
    double   frequency { 440.0 };
    float    triIntegrator { 0.0f };
    Waveform waveform  { Waveform::saw };
};
