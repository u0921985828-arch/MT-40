#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MoogOscillator.h"
#include "MoogLadderFilter.h"
#include "EnvelopeGenerator.h"
#include "NoiseGenerator.h"
#include "SynthParameters.h"

/** Sound accepted by every SynthVoice (the whole keyboard range). */
struct MoogSound : public juce::SynthesiserSound
{
    bool appliesToNote (int)    override { return true; }
    bool appliesToChannel (int) override { return true; }
};

/**
    A single virtual-analog voice: three oscillators, a noise source, the
    ladder filter, and the two ADSR envelopes (filter contour + loudness),
    plus per-voice glide and a resonant feedback path.
*/
class SynthVoice : public juce::SynthesiserVoice
{
public:
    SynthVoice() = default;

    void setParameters (const SynthParameters* p) noexcept { params = p; }

    void prepareToPlay (double sampleRate, int samplesPerBlock, int numChannels);

    bool canPlaySound (juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<MoogSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;

    void stopNote (float velocity, bool allowTailOff) override;

    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                          int startSample, int numSamples) override;

private:
    void updateBlockParameters();
    float midiNoteToHz (float noteNumber) const noexcept;

    const SynthParameters* params { nullptr };

    double sampleRate { 44100.0 };
    juce::AudioBuffer<float> voiceBuffer;   // mono scratch buffer

    MoogOscillator    osc1, osc2, osc3;
    NoiseGenerator    noise;
    MoogLadderFilter  filter;
    EnvelopeGenerator filterEnv, ampEnv;

    // Note / pitch state.
    float targetNoteNumber { 60.0f };
    float glidingNoteNumber { 60.0f };
    float velocityGain { 1.0f };
    float pitchBendSemitones { 0.0f };
    int   pitchWheelPosition { 8192 };

    float feedbackSample { 0.0f };  // last output, fed back into the filter input

    // Cached per-block parameter snapshot.
    struct Cached
    {
        float masterTune = 0.0f;
        bool  glideOn = false;
        float glideCoef = 1.0f;
        float bendRange = 2.0f;

        int   osc1Wave = 1,  osc1Range = 3;
        int   osc2Wave = 1,  osc2Range = 3;
        int   osc3Wave = 1,  osc3Range = 3;
        float osc2Detune = 0, osc2Coarse = 0;
        float osc3Detune = 0, osc3Coarse = 0;
        bool  osc3Lfo = false;
        float osc3ModDepth = 0.0f;

        float osc1Vol = 0, osc2Vol = 0, osc3Vol = 0, noiseVol = 0, feedbackDrive = 0;
        bool  osc1On = true, osc2On = false, osc3On = false, noiseOn = false, feedbackOn = false;

        float cutoff = 1000.0f, reso = 0.0f, contour = 0.0f;
        float keyTrack = 0.0f;
    } c;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthVoice)
};
