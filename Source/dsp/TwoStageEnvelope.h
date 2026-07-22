#pragma once

#include <cmath>

// ---------------------------------------------------------------------------
// TwoStageEnvelope — Casio "CV synthesis" VCA envelope (§3.3).
//
// This is NOT a standard ADSR. It has:
//   * Attack : linear ramp 0 -> 1
//   * Decay/Release : exponential curve toward 0
//
// While a note is held the envelope sustains at the level reached by the end
// of attack (no separate decay stage in the original CV scheme — the tone
// simply holds, then releases exponentially on note-off). The physical
// "Sustain" toggle multiplies the release time constant by a fixed factor.
// ---------------------------------------------------------------------------
class TwoStageEnvelope
{
public:
    enum class Stage { Idle, Attack, Sustain, Release };

    void prepare (double sampleRate) noexcept { fs = sampleRate; }

    void setTimes (double attackSeconds, double releaseSeconds) noexcept
    {
        attackTime = attackSeconds;
        releaseTime = releaseSeconds;
        recalcAttack();
        recalcRelease();
    }

    // 1.0 = short (normal), 3.0 = long (sustain toggle on).
    void setReleaseMultiplier (double m) noexcept
    {
        releaseMultiplier = m;
        recalcRelease();
    }

    void noteOn() noexcept
    {
        stage = Stage::Attack;
    }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }

    void reset() noexcept { level = 0.0f; stage = Stage::Idle; }

    bool isActive() const noexcept { return stage != Stage::Idle; }

    inline float getNextSample() noexcept
    {
        switch (stage)
        {
            case Stage::Attack:
                level += attackInc;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::Sustain; }
                break;

            case Stage::Sustain:
                // Hold at peak while key is down.
                break;

            case Stage::Release:
                level *= releaseCoeff;
                if (level <= 1.0e-4f) { level = 0.0f; stage = Stage::Idle; }
                break;

            case Stage::Idle:
            default:
                break;
        }
        return level;
    }

private:
    void recalcAttack() noexcept
    {
        const double n = attackTime * fs;
        attackInc = (n > 1.0) ? static_cast<float> (1.0 / n) : 1.0f;
    }

    void recalcRelease() noexcept
    {
        const double t = releaseTime * releaseMultiplier;
        // exp coefficient for ~-60 dB over t seconds
        releaseCoeff = (t > 0.0)
            ? static_cast<float> (std::exp (-6.9077553 / (t * fs)))
            : 0.0f;
    }

    double fs = 44100.0;
    double attackTime = 0.005;
    double releaseTime = 0.3;
    double releaseMultiplier = 1.0;

    float attackInc = 0.01f;
    float releaseCoeff = 0.999f;
    float level = 0.0f;
    Stage stage = Stage::Idle;
};
