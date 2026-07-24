#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

/**
    WebView-based plugin editor.

    The entire UI is an embedded HTML/CSS/JS front-end (served from binary
    resources) rendered by a juce::WebBrowserComponent.  Every APVTS parameter
    is bridged to the web page through a relay + parameter attachment, and the
    on-screen keyboard talks back to the processor via native functions.
*/
class MoogSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MoogSynthAudioProcessorEditor (MoogSynthAudioProcessor&);
    ~MoogSynthAudioProcessorEditor() override = default;

    void resized() override;

private:
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url) const;

    MoogSynthAudioProcessor& processorRef;

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
