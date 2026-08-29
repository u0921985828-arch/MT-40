//==============================================================================
//  PluginProcessor.cpp
//==============================================================================

#include "PluginProcessor.h"
#include "PhenotypeWebEditor.h"
#include "Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace phenotype
{
    PhenotypeAudioProcessor::PhenotypeAudioProcessor()
        : AudioProcessor (BusesProperties()
              .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
              .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PHENOTYPE", params::createLayout())
    {
        //  Resolve raw atomic pointers once (audio thread reads these directly).
        for (size_t i = 0; i < params::kDefs.size(); ++i)
            rawParams[i] = apvts.getRawParameterValue (params::kDefs[i].id);

        //  Phenotype is a MIDI-driven granular instrument: granulate the internal
        //  wavetable genome per note rather than live input.
        granular.setInstrumentMode (true);
    }

    void PhenotypeAudioProcessor::syncParametersToEngine() noexcept
    {
        auto& hub = granular.params();
        for (size_t i = 0; i < params::kDefs.size(); ++i)
            hub.set (params::kDefs[i].id, rawParams[i]->load (std::memory_order_relaxed));
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
                                                juce::MidiBuffer& midi)
    {
        juce::ScopedNoDenormals noDenormals;

        const int numSamples = buffer.getNumSamples();
        const int numCh      = buffer.getNumChannels();

        float* left  = buffer.getWritePointer (0);
        float* right = numCh > 1 ? buffer.getWritePointer (1) : left;

        //  Pull host/UI parameter state into the lock-free engine hub.
        syncParametersToEngine();

        //  Feed host tempo to the engine for the tempo-synced arpeggiator.
        if (auto* ph = getPlayHead())
            if (const auto pos = ph->getPosition())
                if (const auto bpm = pos->getBpm())
                    granular.setHostBpm (*bpm);

        //  Sample-accurate MIDI: render the granular cloud in segments split at
        //  each event so note timing is not quantised to the block boundary.
        int pos = 0;
        for (const auto meta : midi)
        {
            const int ts = juce::jlimit (0, numSamples, meta.samplePosition);
            if (ts > pos)
            {
                granular.process (left + pos, right + pos, left + pos, right + pos, ts - pos);
                pos = ts;
            }

            const auto m = meta.getMessage();
            if (m.isNoteOn())            granular.noteOn  (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff())      granular.noteOff (m.getNoteNumber());
            else if (m.isAllNotesOff()
                  || m.isAllSoundOff())  granular.allNotesOff();
            else if (m.isPitchWheel())
            {
                //  14-bit wheel (centre 8192) -> +/- kBendSemitones.
                constexpr float kBendSemitones = 2.0f;
                const float norm = (m.getPitchWheelValue() - 8192) / 8192.0f;
                granular.setPitchBend (norm * kBendSemitones);
            }
        }
        if (pos < numSamples)
            granular.process (left + pos, right + pos, left + pos, right + pos, numSamples - pos);

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

    //==========================================================================
    //  Factory presets (program interface)
    //==========================================================================
    int PhenotypeAudioProcessor::getNumPrograms()
    {
        return static_cast<int> (presets::kFactory.size());
    }

    const juce::String PhenotypeAudioProcessor::getProgramName (int index)
    {
        if (index >= 0 && index < getNumPrograms())
            return presets::kFactory[(size_t) index].name;
        return {};
    }

    void PhenotypeAudioProcessor::setCurrentProgram (int index)
    {
        if (index < 0 || index >= getNumPrograms())
            return;
        currentProgram = index;

        //  Start from defaults, then apply the preset's overrides.
        for (const auto& d : params::kDefs)
            if (auto* p = apvts.getParameter (d.id))
                p->setValueNotifyingHost (d.defaultValue);

        for (const auto& kv : presets::kFactory[(size_t) index].overrides)
            if (kv.id != nullptr)
                if (auto* p = apvts.getParameter (kv.id))
                    p->setValueNotifyingHost (kv.value);
    }

    //==========================================================================
    //  Sample genome loader
    //==========================================================================
    bool PhenotypeAudioProcessor::loadSampleFile (const juce::File& file)
    {
        juce::AudioFormatManager fmt;
        fmt.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fmt.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            return false;

        const int len = static_cast<int> (juce::jmin (reader->lengthInSamples,
                                                       (juce::int64) (reader->sampleRate * 8.0)));
        juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
        reader->read (&buf, 0, len, 0, true, true);

        //  Fold to mono for the genome loader.
        juce::AudioBuffer<float> mono (1, len);
        mono.clear();
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            mono.addFrom (0, 0, buf, ch, 0, len, 1.0f / (float) buf.getNumChannels());

        granular.loadGenomeFromSample (mono.getReadPointer (0), len);
        return true;
    }

    juce::AudioProcessorEditor* PhenotypeAudioProcessor::createEditor()
    {
       #if PHENOTYPE_NATIVE_EDITOR
        //  Legacy-host fallback (e.g. FL Studio 11): the embedded WebView can't
        //  serve the modern WebGL UI on old Trident/WebView2 hosts, so expose a
        //  native generic editor with a labelled slider per APVTS parameter.
        //  Fully automatable; works in every host.
        return new juce::GenericAudioProcessorEditor (*this);
       #else
        return new PhenotypeWebEditor (*this);
       #endif
    }

    void PhenotypeAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
    {
        //  Persist the full parameter tree (host preset / session recall).
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, dest);
    }

    void PhenotypeAudioProcessor::setStateInformation (const void* data, int size)
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new phenotype::PhenotypeAudioProcessor();
}
