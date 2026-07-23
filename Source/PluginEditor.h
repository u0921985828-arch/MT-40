#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "UI/LookAndFeelMoog.h"
#include "UI/Components/ControllerPanel.h"
#include "UI/Components/OscillatorPanel.h"
#include "UI/Components/MixerPanel.h"
#include "UI/Components/FilterPanel.h"
#include "UI/Components/OutputPanel.h"

/**
    Main plugin editor.  Lays out the five Minimoog-style control sections on a
    walnut-framed dark-metal chassis.
*/
class MoogSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MoogSynthAudioProcessorEditor (MoogSynthAudioProcessor&);
    ~MoogSynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MoogSynthAudioProcessor& processorRef;
    LookAndFeelMoog lookAndFeel;

    ControllerPanel  controllers;
    OscillatorPanel  oscillators;
    MixerPanel       mixer;
    FilterPanel      filter;
    OutputPanel      output;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoogSynthAudioProcessorEditor)
};
