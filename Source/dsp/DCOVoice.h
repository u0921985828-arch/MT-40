#pragma once

#include "OnePoleLPF.h"
#include "TwoStageEnvelope.h"
#include "ExpDecay.h"
#include "LFSRNoise.h"
#include "WaveBank.h"
#include "Presets.h"
#include <cstdint>
#include <cmath>

// ---------------------------------------------------------------------------
// DCOVoice — a single melodic voice of the ST-40 consonant-vowel engine (§3.1).
//
// The tonal body ("vowel") is an additive band-limited wavetable selected by
// harmonic family (WaveBank), read at the note frequency with a second,
// slightly detuned oscillator mixed in for the inherent ST-40 chorus. A short
// "consonant" transient (bright click for plucked/mallet tones, airy chiff for
// winds) is layered at the attack. The sum passes through a warmth low-pass and
// is shaped by the ADSR VCA (percussive tones ring out; sustained tones hold).
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
        consLpf.prepare (sampleRate);
        consLpf.setCutoff (1800.0);
        consEnv.prepare (sampleRate);
        WaveBank::instance(); // force the shared table build off the audio thread
    }

    void setPreset (const MelodicPreset& p) noexcept
    {
        preset = &p;
        tables = WaveBank::instance().family (p.family);
        tab = &tables->mip[0];
        lpf.setCutoff (p.lpfCutoff);
        env.setADSR (p.attack, p.decay, p.sustain, p.release);
        detuneRatio = std::pow (2.0, p.detuneCents / 1200.0);
        chorus = p.chorus;
        consType = p.consType;
        consLevel = p.consLevel;
        consEnv.setDecay (p.consDecay > 0.0f ? p.consDecay : 0.02);
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
        consLpf.reset();
        if (tables != nullptr) tab = &tables->mip[WaveBank::pickMip (baseFreq)];
        env.setADSR (preset->attack, preset->decay, preset->sustain, preset->release);
        env.noteOn();
        if (consLevel > 0.0f) consEnv.trigger();
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
        if (! active || preset == nullptr || tab == nullptr) return 0.0f;

        const double vibRatio = std::pow (2.0, (lfoValue * vibratoDepthSemis) / 12.0);
        const double inc = baseFreq * vibRatio / fs;

        phase1 += inc; if (phase1 >= 1.0) phase1 -= 1.0;
        float vowel = WaveBank::read (*tab, phase1);

        if (chorus > 0.0f)
        {
            phase2 += inc * detuneRatio; if (phase2 >= 1.0) phase2 -= 1.0;
            vowel = (vowel + chorus * WaveBank::read (*tab, phase2)) / (1.0f + chorus);
        }

        vowel = lpf.process (vowel);
        const float e = env.getNextSample();

        float cons = 0.0f;
        if (consLevel > 0.0f && consEnv.isActive())
        {
            const float nz = noise.nextSample();
            const float lp = consLpf.process (nz);
            cons = (consType == 2 ? lp : nz - lp) * consEnv.getNextSample() * consLevel;
        }

        if (! env.isActive() && ! consEnv.isActive()) active = false;

        return (vowel * e + cons) * vel * preset->gain;
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
    const WaveBank::Tables* tables = nullptr;
    const std::vector<float>* tab = nullptr;

    OnePoleLPF lpf;                 // warmth low-pass
    TwoStageEnvelope env;           // ADSR VCA
    OnePoleLPF consLpf;             // consonant tilt filter
    ExpDecay consEnv;               // consonant transient envelope
    LFSRNoise noise { 0x1234u };    // consonant noise source

    double fs = 44100.0;
    double baseFreq = 440.0;
    double phase1 = 0.0, phase2 = 0.0;
    double detuneRatio = 1.0;
    float chorus = 0.0f;
    int consType = 0;
    float consLevel = 0.0f;
    float vel = 1.0f;
    int midiNote = -1;
    bool active = false;
    uint64_t age = 0;
};
