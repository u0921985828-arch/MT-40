#pragma once

//==============================================================================
//  PhenotypeWebEditor.h
//
//  JUCE 8 WebView-hosted editor. Bridges the React/WebGL frontend to the DSP
//  backend through:
//    * a resource provider that serves the embedded UI bundle (BinaryData),
//    * a native function "phenotypeSend" the UI calls to push JSON params,
//    * a timer that emits JSON telemetry frames back to the UI.
//==============================================================================

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "ipc/MessageDispatcher.h"

namespace phenotype
{
    class PhenotypeWebEditor : public juce::AudioProcessorEditor,
                               private juce::Timer
    {
    public:
        explicit PhenotypeWebEditor (PhenotypeAudioProcessor&);
        ~PhenotypeWebEditor() override;

        void resized() override;

    private:
        void timerCallback() override;

        //  Serves the embedded SPA. Returns std::nullopt for unknown paths.
        std::optional<juce::WebBrowserComponent::Resource> provide (const juce::String& url);

        //  Native entry point invoked from JS: window.__JUCE__ .backend ...
        void onMessageFromUi (const juce::Array<juce::var>& args,
                              juce::WebBrowserComponent::NativeFunctionCompletion completion);

        static const char* mimeForExtension (const juce::String& ext) noexcept;

        PhenotypeAudioProcessor& processorRef;
        ipc::MessageDispatcher   dispatcher;

        juce::WebBrowserComponent webView;

        //  Telemetry staging (UI thread only).
        std::array<float, PhenotypeAudioProcessor::kNumBins> fftFrame {};

        static constexpr int kTelemetryHz = 30;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhenotypeWebEditor)
    };
}
