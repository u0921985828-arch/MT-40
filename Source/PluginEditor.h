#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include <optional>

// ---------------------------------------------------------------------------
// ST40AudioProcessorEditor — JUCE 8 *native WebView* GUI.
//
// The control surface is authored in HTML/CSS/JS (see ui/index.html) and
// rendered inside a juce::WebBrowserComponent with native integration. Each
// §6 parameter is bridged to the front-end through a typed relay + parameter
// attachment, so the HTML widgets stay in sync with the APVTS in both
// directions (and with host automation).
//
// The HTML/JS assets — including JUCE's own front-end library — are embedded
// as BinaryData and served by an in-process resource provider (no web server,
// no network access).
// ---------------------------------------------------------------------------
class ST40AudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit ST40AudioProcessorEditor (ST40AudioProcessor&);
    ~ST40AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // A WebBrowserComponent that refuses to navigate away from the embedded UI.
    struct SinglePageBrowser : juce::WebBrowserComponent
    {
        using WebBrowserComponent::WebBrowserComponent;
        bool pageAboutToLoad (const juce::String& url) override;
    };

    void timerCallback() override; // pushes transport state to the UI

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url) const;
    juce::WebBrowserComponent::Options makeOptions();

    ST40AudioProcessor& processor;

    // --- Relays (front-end <-> back-end bridges). Names MUST match the
    //     getSliderState/getToggleState/getComboBoxState names in the JS. ---
    juce::WebSliderRelay       masterRelay  { "master_vca_gain" };
    juce::WebSliderRelay       rhythmRelay  { "rhythm_bus_gain" };
    juce::WebSliderRelay       bassRelay    { "bass_osc_gain" };
    juce::WebSliderRelay       tempoRelay   { "host_bpm" };
    juce::WebToggleButtonRelay vibratoRelay { "global_lfo_on" };
    juce::WebToggleButtonRelay sustainRelay { "env_release_mult" };
    juce::WebComboBoxRelay      modeRelay   { "kbd_mode" };
    juce::WebComboBoxRelay      rhythmSelRelay { "active_rhythm_idx" };
    juce::WebComboBoxRelay      presetRelay { "active_patch_idx" };

    std::unique_ptr<SinglePageBrowser> webView;

    // --- Parameter attachments (constructed after webView / relays) -------
    std::unique_ptr<juce::WebSliderParameterAttachment>       masterAtt, rhythmAtt, bassAtt, tempoAtt;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> vibratoAtt, sustainAtt;
    std::unique_ptr<juce::WebComboBoxParameterAttachment>     modeAtt, rhythmSelAtt, presetAtt;

    int lastTransport = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ST40AudioProcessorEditor)
};
