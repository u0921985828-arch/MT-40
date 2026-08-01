//==============================================================================
//  PluginProcessor.cpp
//==============================================================================

#include "PluginProcessor.h"
#include "PhenotypeWebEditor.h"

namespace phenotype
{
    PhenotypeAudioProcessor::PhenotypeAudioProcessor()
        : AudioProcessor (BusesProperties()
              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
              .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    void PhenotypeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        granular.prepare (sampleRate, samplesPerBlock);
        fifo.fill (0.0f);
        magFront.fill (0.0f);
        fifoIndex = 0;
        magReady.store (false);
    }

    bool PhenotypeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto in  = layouts.getMainInputChannelSet();
        const auto out = layouts.getMainOutputChannelSet();
        if (in != out) return false;
        return out == juce::AudioChannelSet::stereo()
            || out == juce::AudioChannelSet::mono();
    }

    void PhenotypeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        const int numSamples = buffer.getNumSamples();
        const int numCh      = buffer.getNumChannels();

        float* left  = buffer.getWritePointer (0);
        float* right = numCh > 1 ? buffer.getWritePointer (1) : left;

        //  Granular cross-synthesis in place (allocation-free).
        granular.process (left, right, left, right, numSamples);

        //  Feed a mono sum into the analysis FIFO for the UI spectrum.
        for (int n = 0; n < numSamples; ++n)
        {
            const float mono = 0.5f * (left[n] + right[n]);
            fifo[(size_t) fifoIndex] = mono;
            if (++fifoIndex >= kFftSize)
            {
                fifoIndex = 0;
                pushToFft (fifo.data(), kFftSize);
            }
        }
    }

    void PhenotypeAudioProcessor::pushToFft (const float* mono, int) noexcept
    {
        std::copy (mono, mono + kFftSize, fftScratch.begin());
        std::fill (fftScratch.begin() + kFftSize, fftScratch.end(), 0.0f);

        window.multiplyWithWindowingTable (fftScratch.data(), kFftSize);
        fft.performFrequencyOnlyForwardTransform (fftScratch.data());

        //  Normalise to 0..1 with a soft log scaling for display.
        constexpr float norm = 2.0f / kFftSize;
        for (int i = 0; i < kNumBins; ++i)
        {
            const float mag = fftScratch[(size_t) i] * norm;
            const float dB   = juce::Decibels::gainToDecibels (mag, -100.0f);
            magFront[(size_t) i] = juce::jlimit (0.0f, 1.0f, (dB + 100.0f) / 100.0f);
        }
        magReady.store (true, std::memory_order_release);
    }

    void PhenotypeAudioProcessor::copyFftFrame (float* dest) const noexcept
    {
        std::copy (magFront.begin(), magFront.end(), dest);
    }

    juce::AudioProcessorEditor* PhenotypeAudioProcessor::createEditor()
    {
        return new PhenotypeWebEditor (*this);
    }

    void PhenotypeAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
    {
        //  Serialise the atomic hub snapshot as JSON for host persistence.
        const auto snap = granular.params().snapshot();
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("caudal",       snap.caudal);
        obj->setProperty ("soilDensity",  snap.soilDensity);
        obj->setProperty ("saturation",   snap.saturation);
        obj->setProperty ("grainDensity", snap.grainDensity);
        obj->setProperty ("grainSize",    snap.grainSize);
        obj->setProperty ("position",     snap.position);
        obj->setProperty ("spray",        snap.spray);
        obj->setProperty ("pitchA",       snap.pitchA);
        obj->setProperty ("pitchB",       snap.pitchB);
        obj->setProperty ("crossBlend",   snap.crossBlend);
        obj->setProperty ("modDepth",     snap.modDepth);
        obj->setProperty ("outputGain",   snap.outputGain);

        const auto json = juce::JSON::toString (juce::var (obj));
        dest.replaceAll (json.toRawUTF8(), json.getNumBytesAsUTF8());
    }

    void PhenotypeAudioProcessor::setStateInformation (const void* data, int size)
    {
        const auto json = juce::String::createStringFromData (data, size);
        const auto v    = juce::JSON::parse (json);
        if (auto* obj = v.getDynamicObject())
            for (auto& kv : obj->getProperties())
                granular.params().set (kv.name.toString().toRawUTF8(),
                                       static_cast<float> ((double) kv.value));
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new phenotype::PhenotypeAudioProcessor();
}
