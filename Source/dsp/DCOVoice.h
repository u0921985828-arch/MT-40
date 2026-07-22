#pragma once

#include "OnePoleLPF.h"
#include "TwoStageEnvelope.h"
#include "Presets.h"
#include <cstdint>
#include <cmath>

// ---------------------------------------------------------------------------
// DCOVoice — a single melodic voice of the MT-40 CV synthesis engine (§3.1).
//
// Each voice = two multiplexed digital pulse waves (independent phase
// accumulators) with per-preset duty cycles, summed together. The two pulses
// carry a small fixed pitch offset (detune) which produces the inherent MT-40
// chorused fatness. The summed pulses pass through the static analog LPF
// (§3.2) and are shaped by the two-stage VCA envelope (§3.3).
//
// A global 5.5 Hz vibrato LFO (§6, CC1) can be applied to pitch.
// ---------------------------------------------------------------------------
class DCOVoice
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        lpf.prepare (sampleRate);
        env.prepare (sampleRate);
    }

    void setPreset (const MelodicPreset& p) noexcept
    {
        preset = &p;
        lpf.setCutoff (p.lpfCutoff);
        env.setTimes (p.attack, p.release);
    }

    void setReleaseMultiplier (double m) noexcept { env.setReleaseMultiplier (m); }

    // note: MIDI note number. velocity: 0..1.
    void noteOn (int note, float velocity) noexcept
    {
        midiNote = note;
        vel = velocity;
        baseFreq = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        phase1 = phase2 = 0.0;
        lpf.reset();
        env.setTimes (preset->attack, preset->release);
        env.noteOn();
        active = true;
        age = 0;
    }

    void noteOff() noexcept { env.noteOff(); }

    bool isActive() const noexcept { return active; }
    int getNote() const noexcept { return midiNote; }
    uint64_t getAge() const noexcept { return age; }
    void tickAge() noexcept { ++age; }

    // vibratoDepthSemis: applied when global vibrato is on (0 otherwise).
    // lfoValue: shared -1..1 LFO sample.
    inline float render (float lfoValue, float vibratoDepthSemis) noexcept
    {
        if (! active || preset == nullptr) return 0.0f;

        const double detuneRatio = std::pow (2.0, preset->detuneCents / 1200.0);
        const double vibRatio = std::pow (2.0, (lfoValue * vibratoDepthSemis) / 12.0);

        const double f1 = baseFreq * vibRatio;
        const double f2 = baseFreq * detuneRatio * vibRatio;

        const double inc1 = f1 / fs;
        const double inc2 = f2 / fs;

        // Two multiplexed pulse waves (bipolar).
        const float p1 = (phase1 < preset->duty1) ? 1.0f : -1.0f;
        const float p2 = (phase2 < preset->duty2) ? 1.0f : -1.0f;

        phase1 += inc1; if (phase1 >= 1.0) phase1 -= 1.0;
        phase2 += inc2; if (phase2 >= 1.0) phase2 -= 1.0;

        float raw = 0.5f * (p1 + p2);
        raw = lpf.process (raw);

        const float e = env.getNextSample();
        if (! env.isActive()) active = false;

        return raw * e * vel * preset->gain;
    }

    void reset() noexcept
    {
        active = false;
        env.reset();
        lpf.reset();
        phase1 = phase2 = 0.0;
    }

private:
    const MelodicPreset* preset = nullptr;
    OnePoleLPF lpf;
    TwoStageEnvelope env;

    double fs = 44100.0;
    double baseFreq = 440.0;
    double phase1 = 0.0, phase2 = 0.0;
    float vel = 1.0f;
    int midiNote = -1;
    bool active = false;
    uint64_t age = 0;
};
