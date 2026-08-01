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
                               private juce::Timer,
                               private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        explicit PhenotypeWebEditor (PhenotypeAudioProcessor&);
        ~PhenotypeWebEditor() override;

        void resized() override;

    private:
        void timerCallback() override;

        //  APVTS::Listener — may fire on the audio thread during automation, so
        //  we only raise a flag here and flush to the WebView from the timer.
        void parameterChanged (const juce::String&, float) override
        {
            paramsDirty.store (true, std::memory_order_relaxed);
        }

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

        //  Set by parameterChanged (any thread), consumed by the timer.
        std::atomic<bool> paramsDirty { true };   // true -> push initial snapshot

        static constexpr int kTelemetryHz = 30;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhenotypeWebEditor)
    };
}
