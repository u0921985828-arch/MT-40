#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Parameters.h"

/**
    Thin cache of raw std::atomic<float> pointers into the APVTS.

    Reading a parameter through getRawParameterValue() once and then
    dereferencing the atomic in the audio thread is far cheaper than a string
    lookup per sample.  A single instance is owned by the synthesiser and shared
    (by const pointer) with every voice.
*/
struct SynthParameters
{
    void connect (juce::AudioProcessorValueTreeState& apvts)
    {
        masterVolume   = apvts.getRawParameterValue (ParamID::masterVolume);
        masterTune     = apvts.getRawParameterValue (ParamID::masterTune);
        glideOn        = apvts.getRawParameterValue (ParamID::glideOn);
        glideTime      = apvts.getRawParameterValue (ParamID::glideTime);
        pitchBendRange = apvts.getRawParameterValue (ParamID::pitchBendRange);

        osc1Wave  = apvts.getRawParameterValue (ParamID::osc1Wave);
        osc1Range = apvts.getRawParameterValue (ParamID::osc1Range);

        osc2Wave   = apvts.getRawParameterValue (ParamID::osc2Wave);
        osc2Range  = apvts.getRawParameterValue (ParamID::osc2Range);
        osc2Detune = apvts.getRawParameterValue (ParamID::osc2Detune);
        osc2Coarse = apvts.getRawParameterValue (ParamID::osc2Coarse);

        osc3Wave     = apvts.getRawParameterValue (ParamID::osc3Wave);
        osc3Range    = apvts.getRawParameterValue (ParamID::osc3Range);
        osc3Detune   = apvts.getRawParameterValue (ParamID::osc3Detune);
        osc3Coarse   = apvts.getRawParameterValue (ParamID::osc3Coarse);
        osc3LfoMode  = apvts.getRawParameterValue (ParamID::osc3LfoMode);
        osc3ModDepth = apvts.getRawParameterValue (ParamID::osc3ModDepth);

        mixOsc1Vol  = apvts.getRawParameterValue (ParamID::mixOsc1Vol);
        mixOsc2Vol  = apvts.getRawParameterValue (ParamID::mixOsc2Vol);
        mixOsc3Vol  = apvts.getRawParameterValue (ParamID::mixOsc3Vol);
        mixNoiseVol = apvts.getRawParameterValue (ParamID::mixNoiseVol);
        mixFeedback = apvts.getRawParameterValue (ParamID::mixFeedback);

        mixOsc1On     = apvts.getRawParameterValue (ParamID::mixOsc1On);
        mixOsc2On     = apvts.getRawParameterValue (ParamID::mixOsc2On);
        mixOsc3On     = apvts.getRawParameterValue (ParamID::mixOsc3On);
        mixNoiseOn    = apvts.getRawParameterValue (ParamID::mixNoiseOn);
        mixFeedbackOn = apvts.getRawParameterValue (ParamID::mixFeedbackOn);
        noiseType     = apvts.getRawParameterValue (ParamID::noiseType);

        filterCutoff   = apvts.getRawParameterValue (ParamID::filterCutoff);
        filterReso     = apvts.getRawParameterValue (ParamID::filterReso);
        filterEnv      = apvts.getRawParameterValue (ParamID::filterEnv);
        filterKeyTrack = apvts.getRawParameterValue (ParamID::filterKeyTrack);

        filterAttack  = apvts.getRawParameterValue (ParamID::filterAttack);
        filterDecay   = apvts.getRawParameterValue (ParamID::filterDecay);
        filterSustain = apvts.getRawParameterValue (ParamID::filterSustain);
        filterRelease = apvts.getRawParameterValue (ParamID::filterRelease);

        ampAttack  = apvts.getRawParameterValue (ParamID::ampAttack);
        ampDecay   = apvts.getRawParameterValue (ParamID::ampDecay);
        ampSustain = apvts.getRawParameterValue (ParamID::ampSustain);
        ampRelease = apvts.getRawParameterValue (ParamID::ampRelease);
    }

    std::atomic<float>* masterVolume   = nullptr;
    std::atomic<float>* masterTune     = nullptr;
    std::atomic<float>* glideOn        = nullptr;
    std::atomic<float>* glideTime      = nullptr;
    std::atomic<float>* pitchBendRange = nullptr;

    std::atomic<float>* osc1Wave  = nullptr;
    std::atomic<float>* osc1Range = nullptr;

    std::atomic<float>* osc2Wave   = nullptr;
    std::atomic<float>* osc2Range  = nullptr;
    std::atomic<float>* osc2Detune = nullptr;
    std::atomic<float>* osc2Coarse = nullptr;

    std::atomic<float>* osc3Wave     = nullptr;
    std::atomic<float>* osc3Range    = nullptr;
    std::atomic<float>* osc3Detune   = nullptr;
    std::atomic<float>* osc3Coarse   = nullptr;
    std::atomic<float>* osc3LfoMode  = nullptr;
    std::atomic<float>* osc3ModDepth = nullptr;

    std::atomic<float>* mixOsc1Vol  = nullptr;
    std::atomic<float>* mixOsc2Vol  = nullptr;
    std::atomic<float>* mixOsc3Vol  = nullptr;
    std::atomic<float>* mixNoiseVol = nullptr;
    std::atomic<float>* mixFeedback = nullptr;

    std::atomic<float>* mixOsc1On     = nullptr;
    std::atomic<float>* mixOsc2On     = nullptr;
    std::atomic<float>* mixOsc3On     = nullptr;
    std::atomic<float>* mixNoiseOn    = nullptr;
    std::atomic<float>* mixFeedbackOn = nullptr;
    std::atomic<float>* noiseType     = nullptr;

    std::atomic<float>* filterCutoff   = nullptr;
    std::atomic<float>* filterReso     = nullptr;
    std::atomic<float>* filterEnv      = nullptr;
    std::atomic<float>* filterKeyTrack = nullptr;

    std::atomic<float>* filterAttack  = nullptr;
    std::atomic<float>* filterDecay   = nullptr;
    std::atomic<float>* filterSustain = nullptr;
    std::atomic<float>* filterRelease = nullptr;

    std::atomic<float>* ampAttack  = nullptr;
    std::atomic<float>* ampDecay   = nullptr;
    std::atomic<float>* ampSustain = nullptr;
    std::atomic<float>* ampRelease = nullptr;
};
