#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/PolyphonicSynthesizer.h"

/**
    Main audio processor for the Moog-style virtual-analog synthesiser.

    Owns the APVTS (parameter state), the polyphonic synth engine, and the
    final master-volume / soft-limiter output stage.
*/
class MoogSynthAudioProcessor : public juce::AudioProcessor
{
public:
    MoogSynthAudioProcessor();
    ~MoogSynthAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    PolyphonicSynthesizer synth;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoogSynthAudioProcessor)
};
