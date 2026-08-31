#pragma once

//==============================================================================
//  PluginProcessor.h  —  Phenotype granular cross-synthesis backend.
//==============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/GranularEngine.h"
#include "Parameters.h"
#include "PresetLibrary.h"

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
        void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
        bool supportsDoublePrecisionProcessing() const override { return true; }

        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override { return true; }

        const juce::String getName() const override { return "Phenotype"; }
        bool acceptsMidi()  const override { return true; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        //  Note release (~1.4 s to silence on the AR pole) + grains up to 0.4 s
        //  + filter ring; report a realistic tail so hosts don't cut it offline.
        double getTailLengthSeconds() const override { return 1.5; }

        int  getNumPrograms() override;
        int  getCurrentProgram() override { return currentProgram; }
        void setCurrentProgram (int) override;
        const juce::String getProgramName (int) override;
        void changeProgramName (int, const juce::String&) override {}

        //  Load a user audio file as the granular genome (off the audio thread).
        bool loadSampleFile (const juce::File&);
        //  Load an embedded HQ genome (BinaryData, "<name>.wav") — the factory
        //  palette. Returns false if the resource isn't found.
        bool loadEmbeddedGenome (const juce::String& name);

        //  --- Preset library / DLC banks --------------------------------------
        //  Re-seed the factory and rescan the user preset dir for .phbank files.
        void rescanLibrary();
        //  Copy a bank (a .phbank file or a folder containing one) into the user
        //  library and rescan. Returns true on success.
        bool importBank (const juce::File& source);
        //  Number of preset banks/entries currently registered.
        int  libraryCount() const noexcept { return library.size(); }

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
        int currentProgram = 0;

        //  Cached raw atomic pointers into the APVTS, resolved once at
        //  construction so the audio thread never does a string lookup.
        std::array<std::atomic<float>*, params::kDefs.size()> rawParams {};

        dsp::GranularEngine granular;

        //  Preset registry: compiled factory + scanned .phbank DLC banks.
        PresetLibrary library;

        //  Pre-allocated float scratch for the double-precision entry point.
        juce::AudioBuffer<float> doubleScratch;

        //  --- Analysis (audio thread writes, UI thread reads) -----------------
        juce::dsp::FFT                       fft { kFftOrder };
        juce::dsp::WindowingFunction<float>  window { kFftSize,
                                                      juce::dsp::WindowingFunction<float>::hann };
        std::array<float, kFftSize>          fifo {};
        std::array<float, kFftSize * 2>      fftScratch {};
        int                                  fifoIndex = 0;

        //  Lock-free SPSC double buffer: audio writes the inactive frame and
        //  publishes its index (release); the UI reads the active one (acquire).
        std::array<std::array<float, kNumBins>, 2> magBuf {};
        std::atomic<int>                           magIndex { 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhenotypeAudioProcessor)
    };
}
