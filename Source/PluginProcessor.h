#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cstdint>
#include <vector>
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

    /** Pitch-wheel value from the on-screen wheel, -1..+1. */
    void setUiPitchBend (float normalised) noexcept { uiPitchBend.store (normalised); }

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

    // Voice bank: each MonoSynthEngine is one monophonic voice. Mono mode uses
    // voices[0] with the full MIDI stream (authentic last-note priority); poly
    // mode allocates note-ons across the bank and sums the voices.
    static constexpr int maxVoices = 8;
    std::array<MonoSynthEngine, maxVoices> voices;
    std::array<juce::MidiBuffer, maxVoices> voiceMidi;
    int      voiceNote[maxVoices];       // note each voice currently holds, -1 = free
    uint64_t voiceAge[maxVoices]  { 0 }; // allocation order (for stealing)
    uint64_t ageCounter { 0 };
    bool     wasPoly { false };

    int allocateVoice (int note) noexcept;
    int findVoiceForNote (int note) const noexcept;

    // Perform layer: rewrites the incoming MIDI (chord expansion + arpeggiator)
    // before it reaches the voice dispatch. Global performance mode, not a preset.
    void applyPerform (juce::MidiBuffer& midi, int numSamples);
    std::vector<int> perfHeld;          // held notes, in press order
    double   perfToNextStep { 0.0 };    // samples remaining until next arp step
    double   perfToGateOff  { 0.0 };    // samples remaining until arp note release
    int      perfArpNote    { -1 };     // currently sounding arp note (-1 = none)
    int      perfArpPos     { 0 };      // sequence position
    uint32_t perfRandSeed   { 0x9E3779B9u };
    bool     perfWasArp     { false };
    double   currentSampleRate { 44100.0 };

    std::atomic<float> uiPitchBend { 0.0f };

    // Master output: analog tone shaping -> 4x oversampled tube stage -> DC block.
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    static constexpr size_t oversamplingFactor = 2; // 2^2 = 4x
    float dcX1[2] { 0.0f, 0.0f };
    float dcY1[2] { 0.0f, 0.0f };
    float lpWarm[2] { 0.0f, 0.0f };   // low-shelf (warmth) one-pole state
    float lpAir[2]  { 0.0f, 0.0f };   // high-shelf (air) one-pole state
    float coefWarm { 0.02f };
    float coefAir  { 0.4f };

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
