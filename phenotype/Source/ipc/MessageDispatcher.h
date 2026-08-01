#pragma once

//==============================================================================
//  MessageDispatcher.h
//
//  Bidirectional asynchronous IPC between the JUCE WebView UI and the audio
//  backend, using pure JSON. Two directions:
//
//    UI  -> DSP : parameter edits & commands, parsed into the atomic hub.
//    DSP -> UI : telemetry frames (FFT magnitudes, capillary phase, grain
//                activity) serialised as JSON and emitted to the WebView.
//
//  The dispatcher owns no audio state; it holds a reference to the ParameterHub
//  and translates messages. All UI-thread only.
//==============================================================================

#include <juce_core/juce_core.h>
#include "../dsp/ParameterHub.h"

namespace phenotype::ipc
{
    class MessageDispatcher
    {
    public:
        explicit MessageDispatcher (dsp::ParameterHub& hubToUse) noexcept
            : hub (hubToUse) {}

        //  --- Inbound (UI -> DSP) ---------------------------------------------
        //  Expected shape:
        //    { "type": "param", "id": "grainSize", "value": 0.42 }
        //    { "type": "batch", "params": { "grainSize": 0.4, "spray": 0.2 } }
        void handleFromUi (const juce::var& message) noexcept
        {
            if (! message.isObject())
                return;

            const auto type = message.getProperty ("type", {}).toString();

            if (type == "param")
            {
                const auto id  = message.getProperty ("id", {}).toString();
                const auto val = static_cast<float> ((double) message.getProperty ("value", 0.0));
                hub.set (id.toRawUTF8(), val);
            }
            else if (type == "batch")
            {
                const auto params = message.getProperty ("params", {});
                if (auto* obj = params.getDynamicObject())
                    for (auto& kv : obj->getProperties())
                        hub.set (kv.name.toString().toRawUTF8(),
                                 static_cast<float> ((double) kv.value));
            }
        }

        //  Convenience for the WebView native-function path (string payload).
        void handleFromUi (const juce::String& jsonText) noexcept
        {
            handleFromUi (juce::JSON::parse (jsonText));
        }

        //  --- Outbound (DSP -> UI) --------------------------------------------
        //  Builds a telemetry frame. `fft` points at `numBins` normalised
        //  magnitudes (0..1). Returns a juce::var ready for emitEvent.
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

    private:
        dsp::ParameterHub& hub;
    };
}
