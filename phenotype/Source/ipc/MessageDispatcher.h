#pragma once

//==============================================================================
//  MessageDispatcher.h
//
//  Bidirectional asynchronous IPC between the JUCE WebView UI and the backend,
//  using pure JSON. Two directions:
//
//    UI  -> HOST : parameter edits applied to the APVTS via setValueNotifyingHost
//                  so the DAW records automation and the audio thread (which
//                  reads cached atomics) picks them up on the next block.
//    HOST -> UI  : telemetry frames (FFT, capillary phase, grain activity) and
//                  parameter snapshots (for host automation / preset recall).
//
//  UI-thread only; owns no audio state.
//==============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Parameters.h"

namespace phenotype::ipc
{
    class MessageDispatcher
    {
    public:
        explicit MessageDispatcher (juce::AudioProcessorValueTreeState& stateToUse) noexcept
            : apvts (stateToUse) {}

        //  --- Inbound (UI -> HOST) --------------------------------------------
        //    { "type": "param", "id": "grainSize", "value": 0.42 }
        //    { "type": "batch", "params": { "grainSize": 0.4, "spray": 0.2 } }
        void handleFromUi (const juce::var& message) noexcept
        {
            if (! message.isObject())
                return;

            const auto type = message.getProperty ("type", {}).toString();

            if (type == "param")
            {
                applyParam (message.getProperty ("id", {}).toString(),
                            static_cast<float> ((double) message.getProperty ("value", 0.0)));
            }
            else if (type == "batch")
            {
                const auto params = message.getProperty ("params", {});
                if (auto* obj = params.getDynamicObject())
                    for (auto& kv : obj->getProperties())
                        applyParam (kv.name.toString(), static_cast<float> ((double) kv.value));
            }
        }

        void handleFromUi (const juce::String& jsonText) noexcept
        {
            handleFromUi (juce::JSON::parse (jsonText));
        }

        //  --- Outbound builders (HOST -> UI) ----------------------------------
        static juce::var buildTelemetry (const float* fft, int numBins,
                                         float capillaryLevel, int activeGrains) noexcept
        {
            auto* root = new juce::DynamicObject();
            root->setProperty ("type",         "telemetry");
            root->setProperty ("capillary",    capillaryLevel);
            root->setProperty ("activeGrains", activeGrains);

            juce::Array<juce::var> bins;
            bins.ensureStorageAllocated (numBins);
            for (int i = 0; i < numBins; ++i)
                bins.add (fft[i]);
            root->setProperty ("fft", bins);
            return juce::var (root);
        }

        //  Full parameter snapshot so the UI can mirror host/preset changes.
        juce::var buildParamSnapshot() const noexcept
        {
            auto* params = new juce::DynamicObject();
            for (const auto& d : params::kDefs)
                if (auto* raw = apvts.getRawParameterValue (d.id))
                    params->setProperty (d.id, raw->load (std::memory_order_relaxed));

            auto* root = new juce::DynamicObject();
            root->setProperty ("type",   "params");
            root->setProperty ("params", juce::var (params));
            return juce::var (root);
        }

    private:
        void applyParam (const juce::String& id, float normalised) noexcept
        {
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
        }

        juce::AudioProcessorValueTreeState& apvts;
    };
}
