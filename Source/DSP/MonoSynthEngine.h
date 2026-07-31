#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "MoogOscillator.h"
#include "MoogLadderFilter.h"
#include "EnvelopeGenerator.h"
#include "NoiseGenerator.h"
#include "SynthParameters.h"

/**
    Monophonic Minimoog-style synth engine.

    Authentic behaviour: single voice, last-note priority (a stack of held
    keys), legato glide with single-triggered envelopes (retrigger only when
    starting from silence). Three oscillators + noise feed the mixer, which can
    overdrive the ladder filter (growl); Osc 3 / Noise form the modulation bus
    routed to pitch and/or cutoff by the mod wheel. Analog drift detunes the
    oscillators slightly for the classic fatness.
*/
class MonoSynthEngine
{
public:
    MonoSynthEngine() = default;

    void setParameters (const SynthParameters* p) noexcept { params = p; }
    void setExternalBend (float semitones) noexcept { externalBendSemitones = semitones; }
    void prepare (double sampleRate, int samplesPerBlock);
    void reset() noexcept;

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          const juce::MidiBuffer& midiMessages,
                          int startSample, int numSamples);

    /** True while the amplitude envelope is still producing sound (for poly
        voice allocation / stealing). */
    bool isActive() const noexcept { return ampEnv.isActive(); }

private:
    void handleMidi (const juce::MidiMessage& m);
    void pushNote (int note, float velocity);
    void releaseNote (int note);
    void updateBlockParameters();
    float midiNoteToHz (float noteNumber) const noexcept;
    inline float renderSample();

    const SynthParameters* params { nullptr };
    double sampleRate { 44100.0 };

    MoogOscillator    osc1, osc2, osc3;
    NoiseGenerator    noise;
    MoogLadderFilter  filter;
    EnvelopeGenerator filterEnv, ampEnv;

    std::vector<int> heldNotes;          // last-note priority stack
    float targetNoteNumber  { 60.0f };
    float glidingNoteNumber { 60.0f };
    float velocityGain { 1.0f };
    float pitchBendSemitones { 0.0f };
    float externalBendSemitones { 0.0f }; // from the on-screen pitch wheel
    float modWheelMidi { 0.0f };

    float feedbackSample { 0.0f };
    float lastOsc3 { 0.0f };
    float lastNoise { 0.0f };
    double driftPhase[3] { 0.0, 0.0, 0.0 };

    struct Cached
    {
        float masterTune = 0.0f;
        float glideCoef = 1.0f;
        bool  glideOn = false;
        float bendRange = 2.0f;
        float modWheel = 0.0f;
        float modMix = 0.0f;
        bool  modOscOn = false, modFilterOn = false;

        int   osc1Wave = 2, osc1Range = 3;
        int   osc2Wave = 2, osc2Range = 3;
        int   osc3Wave = 2, osc3Range = 3;
        float osc2Detune = 0.0f, osc3Detune = 0.0f;
        bool  osc3Kb = true;

        float osc1Vol = 0, osc2Vol = 0, osc3Vol = 0, noiseVol = 0, extVol = 0;
        bool  osc1On = true, osc2On = false, osc3On = false, noiseOn = false, extOn = false;

        float cutoff = 1000.0f, reso = 0.0f, contour = 0.0f, keyTrack = 0.0f;
        float drift = 0.0f;

        bool  sampleOn = false; int sampleSel = 0; float sampleVol = 0.0f;
    } c;

    double samplePhase { 0.0 };  // wavetable read phase (0..1)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MonoSynthEngine)
};
