#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
    Analog-style ADSR envelope generator.

    Segment shapes use the classic one-pole "RC charge" curves of a real
    envelope generator rather than straight lines, which gives the snappy Moog
    feel.  Times are supplied in milliseconds; the sustain level is linear
    (0..1).  The release switch behaviour (ADS vs ADSR) is handled by the caller
    by choosing the release time.
*/
class EnvelopeGenerator
{
public:
    enum class Stage { idle, attack, decay, sustain, release };

    EnvelopeGenerator() = default;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        recalculate();
    }

    void setAttackMs  (float ms) noexcept { attackMs  = juce::jmax (0.1f, ms); recalculate(); }
    void setDecayMs   (float ms) noexcept { decayMs   = juce::jmax (0.1f, ms); recalculate(); }
    void setSustain   (float s)  noexcept { sustainLevel = juce::jlimit (0.0f, 1.0f, s); }
    void setReleaseMs (float ms) noexcept { releaseMs = juce::jmax (0.1f, ms); recalculate(); }

    void noteOn() noexcept
    {
        stage = Stage::attack;
    }

    void noteOff() noexcept
    {
        if (stage != Stage::idle)
            stage = Stage::release;
    }

    /** Immediately silences the envelope (used when a voice is stolen). */
    void reset() noexcept
    {
        stage = Stage::idle;
        level = 0.0f;
    }

    bool isActive() const noexcept { return stage != Stage::idle; }

    inline float getNextSample() noexcept
    {
        switch (stage)
        {
            case Stage::idle:
                level = 0.0f;
                break;

            case Stage::attack:
                // Charge toward slightly above 1 so we cross 1.0 in finite time.
                level += attackCoef * (attackTarget - level);
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::decay;
                }
                break;

            case Stage::decay:
                level += decayCoef * (sustainLevel - decayTarget - level);
                if (level <= sustainLevel)
                {
                    level = sustainLevel;
                    stage = Stage::sustain;
                }
                break;

            case Stage::sustain:
                level = sustainLevel;
                if (sustainLevel <= 0.0f)
                    stage = Stage::idle;
                break;

            case Stage::release:
                level += releaseCoef * (releaseTarget - level);
                if (level <= 0.00001f)
                {
                    level = 0.0f;
                    stage = Stage::idle;
                }
                break;
        }

        return level;
    }

private:
    static float coefFor (float timeMs, double sampleRate) noexcept
    {
        const double samples = juce::jmax (1.0, timeMs * 0.001 * sampleRate);
        // One-pole coefficient that reaches ~99% of the target over `samples`.
        return (float) (1.0 - std::exp (-5.0 / samples));
    }

    void recalculate() noexcept
    {
        attackCoef  = coefFor (attackMs,  fs);
        decayCoef   = coefFor (decayMs,   fs);
        releaseCoef = coefFor (releaseMs, fs);
    }

    double fs { 44100.0 };

    float attackMs  { 5.0f };
    float decayMs   { 200.0f };
    float sustainLevel { 0.7f };
    float releaseMs { 200.0f };

    float attackCoef  { 0.01f };
    float decayCoef   { 0.01f };
    float releaseCoef { 0.01f };

    // Targets are pushed slightly past the boundary so the exponential
    // segments terminate in bounded time.
    static constexpr float attackTarget  = 1.2f;
    static constexpr float decayTarget   = 0.05f;
    static constexpr float releaseTarget = -0.05f;

    Stage stage { Stage::idle };
    float level { 0.0f };
};
