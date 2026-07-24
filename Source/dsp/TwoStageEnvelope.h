#pragma once

#include <cmath>

// ---------------------------------------------------------------------------
// TwoStageEnvelope — melodic VCA envelope (§3.3).
//
// Attack-Decay-Sustain-Release. Percussive ST-40 tones (sustain 0) decay to
// silence while the key is held (xylophone / harpsichord / banjo "ping");
// sustained tones (sustain ~0.9-1) hold at the sustain level until note-off.
//   * Attack  : linear ramp 0 -> 1
//   * Decay   : exponential 1 -> sustain
//   * Sustain : hold (skipped straight to Idle when sustain == 0)
//   * Release : exponential -> 0
// The physical "Sustain" toggle multiplies the release time constant.
// ---------------------------------------------------------------------------
class TwoStageEnvelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void prepare (double sampleRate) noexcept { fs = sampleRate; }

    void setADSR (double attackSeconds, double decaySeconds,
                  double sustainLevel, double releaseSeconds) noexcept
    {
        attackTime  = attackSeconds;
        decayTime   = decaySeconds;
        sustain     = static_cast<float> (sustainLevel);
        releaseTime = releaseSeconds;
        recalcAttack();
        recalcDecay();
        recalcRelease();
    }

    // 1.0 = short (normal), 3.0 = long (sustain toggle on).
    void setReleaseMultiplier (double m) noexcept
    {
        releaseMultiplier = m;
        recalcRelease();
    }

    void noteOn()  noexcept { stage = Stage::Attack; }
    void noteOff() noexcept { if (stage != Stage::Idle) stage = Stage::Release; }
    void reset()   noexcept { level = 0.0f; stage = Stage::Idle; }

    bool isActive() const noexcept { return stage != Stage::Idle; }

    inline float getNextSample() noexcept
    {
        switch (stage)
        {
            case Stage::Attack:
                level += attackInc;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
                break;

            case Stage::Decay:
                level = sustain + (level - sustain) * decayCoeff;
                if (level - sustain <= 1.0e-4f)
                {
                    level = sustain;
                    stage = (sustain <= 1.0e-4f) ? Stage::Idle : Stage::Sustain;
                }
                break;

            case Stage::Sustain:
                break; // hold at sustain while key is down

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

    void recalcDecay() noexcept
    {
        decayCoeff = (decayTime > 0.0)
            ? static_cast<float> (std::exp (-6.9077553 / (decayTime * fs)))
            : 0.0f;
    }

    void recalcRelease() noexcept
    {
        const double t = releaseTime * releaseMultiplier;
        releaseCoeff = (t > 0.0)
            ? static_cast<float> (std::exp (-6.9077553 / (t * fs)))
            : 0.0f;
    }

    double fs = 44100.0;
    double attackTime = 0.005;
    double decayTime = 0.1;
    double releaseTime = 0.3;
    double releaseMultiplier = 1.0;

    float attackInc = 0.01f;
    float decayCoeff = 0.999f;
    float releaseCoeff = 0.999f;
    float sustain = 1.0f;
    float level = 0.0f;
    Stage stage = Stage::Idle;
};
