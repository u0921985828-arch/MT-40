#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/MonoSynthEngine.h"
#include "DSP/SynthParameters.h"
#include "PresetManager.h"

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
    PresetManager& getPresetManager() noexcept { return presetManager; }

    /** Copies the most recent `numSamples` of output into `dest` (oldest first)
        for the visualiser. Safe to call from the message thread. */
    int readScope (float* dest, int numSamples) const noexcept;

    /** Latest block peak level (0..1) for the given output channel. */
    float getMeterLevel (int channel) const noexcept
    {
        return meter[(size_t) juce::jlimit (0, 1, channel)].load (std::memory_order_relaxed);
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    inline void pushScope (float sample) noexcept;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager { apvts };
    juce::MidiKeyboardState keyboardState;

    SynthParameters synthParams;
    MonoSynthEngine engine;

    // Master output: 4x oversampled soft limiter + DC blocker.
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    static constexpr size_t oversamplingFactor = 2; // 2^2 = 4x
    float dcX1[2] { 0.0f, 0.0f };
    float dcY1[2] { 0.0f, 0.0f };

    // Lock-free single-producer / single-consumer ring buffer feeding the
    // WebView oscilloscope + spectrum display.
    struct Scope
    {
        static constexpr int size = 4096;
        static constexpr int mask = size - 1;
        std::array<std::atomic<float>, size> buffer;
        std::atomic<int> writePos { 0 };
    } scope;

    std::atomic<float> meter[2] { { 0.0f }, { 0.0f } };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoogSynthAudioProcessor)
};
