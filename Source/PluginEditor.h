#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

/**
    WebView-based plugin editor.

    The entire UI is an embedded HTML/CSS/JS front-end (served from binary
    resources) rendered by a juce::WebBrowserComponent.  Every APVTS parameter
    is bridged to the web page through a relay + parameter attachment, and the
    on-screen keyboard talks back to the processor via native functions.
*/
class MoogSynthAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit MoogSynthAudioProcessorEditor (MoogSynthAudioProcessor&);
    ~MoogSynthAudioProcessorEditor() override;

    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url) const;

    // Pushes the latest waveform + spectrum frame to the WebView.
    void timerCallback() override;

    MoogSynthAudioProcessor& processorRef;

    // Visualiser DSP.
    static constexpr int fftOrder = 10;
    static constexpr int fftSize  = 1 << fftOrder; // 1024
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize>     scopeSamples {};
    std::array<float, fftSize * 2> fftData {};
    float meterDisplay[2] { 0.0f, 0.0f }; // peak-hold with decay

    // One relay + attachment per parameter, grouped by control type.
    juce::OwnedArray<juce::WebSliderRelay>        sliderRelays;
    juce::OwnedArray<juce::WebToggleButtonRelay>  toggleRelays;
    juce::OwnedArray<juce::WebComboBoxRelay>      comboRelays;

    juce::OwnedArray<juce::WebSliderParameterAttachment>       sliderAttachments;
    juce::OwnedArray<juce::WebToggleButtonParameterAttachment> toggleAttachments;
    juce::OwnedArray<juce::WebComboBoxParameterAttachment>     comboAttachments;

    std::unique_ptr<juce::WebBrowserComponent> webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MoogSynthAudioProcessorEditor)
};
