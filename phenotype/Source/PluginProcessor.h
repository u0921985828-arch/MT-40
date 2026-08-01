#pragma once

//==============================================================================
//  PluginProcessor.h  —  Phenotype granular cross-synthesis backend.
//==============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/GranularEngine.h"
#include "Parameters.h"

namespace phenotype
{
    class PhenotypeAudioProcessor : public juce::AudioProcessor
    {
    public:
        static constexpr int kFftOrder = 11;                 // 2048-point FFT
        static constexpr int kFftSize  = 1 << kFftOrder;
        static constexpr int kNumBins  = kFftSize / 2;

        PhenotypeAudioProcessor();
        ~PhenotypeAudioProcessor() override = default;

        //  --- AudioProcessor ---------------------------------------------------
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return "Phenotype"; }
        bool acceptsMidi()  const override { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        int  getNumPrograms() override { return 1; }
        int  getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock&) override;
        void setStateInformation (const void*, int) override;

        //  --- Engine / telemetry access for the editor ------------------------
        dsp::GranularEngine& engine() noexcept { return granular; }

        //  Host-facing parameter tree (automation + persistence + UI binding).
        juce::AudioProcessorValueTreeState& state() noexcept { return apvts; }

        //  Copies the latest normalised FFT frame into `dest` (kNumBins floats).
        //  Thread-safe snapshot for the UI timer.
        void copyFftFrame (float* dest) const noexcept;

    private:
        void pushToFft (const float* mono, int numSamples) noexcept;

        //  Copies the current APVTS values into the lock-free engine hub.
        //  Called once per block on the audio thread (12 atomic loads).
        void syncParametersToEngine() noexcept;

        juce::AudioProcessorValueTreeState apvts;

        //  Cached raw atomic pointers into the APVTS, resolved once at
        //  construction so the audio thread never does a string lookup.
        std::array<std::atomic<float>*, params::kDefs.size()> rawParams {};

        dsp::GranularEngine granular;

        //  --- Analysis (audio thread writes, UI thread reads) -----------------
        juce::dsp::FFT                       fft { kFftOrder };
        juce::dsp::WindowingFunction<float>  window { kFftSize,
                                                      juce::dsp::WindowingFunction<float>::hann };
        std::array<float, kFftSize>          fifo {};
        std::array<float, kFftSize * 2>      fftScratch {};
        int                                  fifoIndex = 0;

        //  Double-buffered magnitude frame published lock-free to the UI.
        std::array<float, kNumBins>          magFront {};
        std::atomic<bool>                    magReady { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhenotypeAudioProcessor)
    };
}
