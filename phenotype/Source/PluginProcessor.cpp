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

        //  Preset registry: built-in factory + any DLC banks in the user dir.
        library.seedFactory();
        library.scan (PresetLibrary::userPresetDir());
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
        doubleScratch.setSize (2, samplesPerBlock, false, false, true);   // preallocate
        fifo.fill (0.0f);
        magBuf[0].fill (0.0f);
        magBuf[1].fill (0.0f);
        fifoIndex = 0;
        magIndex.store (0, std::memory_order_relaxed);
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
            else if (m.isSustainPedalOn())        granular.setSustain (true);
            else if (m.isSustainPedalOff())       granular.setSustain (false);
            else if (m.isController() && m.getControllerNumber() == 1)   // mod wheel
                granular.setModWheel (m.getControllerValue() * (1.0f / 127.0f));
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

    //  Double-precision hosts: convert into the pre-allocated float scratch,
    //  run the single float engine path, convert back. No audio-thread alloc
    //  (scratch is sized in prepareToPlay; setSize avoids reallocation).
    void PhenotypeAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer,
                                                juce::MidiBuffer& midi)
    {
        const int numCh = juce::jmin (2, buffer.getNumChannels());
        const int numSamples = buffer.getNumSamples();
        doubleScratch.setSize (juce::jmax (1, numCh), numSamples, false, false, true);

        for (int c = 0; c < numCh; ++c)
        {
            const double* src = buffer.getReadPointer (c);
            float* dst = doubleScratch.getWritePointer (c);
            for (int i = 0; i < numSamples; ++i) dst[i] = static_cast<float> (src[i]);
        }

        processBlock (doubleScratch, midi);

        for (int c = 0; c < numCh; ++c)
        {
            const float* src = doubleScratch.getReadPointer (c);
            double* dst = buffer.getWritePointer (c);
            for (int i = 0; i < numSamples; ++i) dst[i] = static_cast<double> (src[i]);
        }
    }

    void PhenotypeAudioProcessor::pushToFft (const float* mono, int) noexcept
    {
        std::copy (mono, mono + kFftSize, fftScratch.begin());
        std::fill (fftScratch.begin() + kFftSize, fftScratch.end(), 0.0f);

        window.multiplyWithWindowingTable (fftScratch.data(), kFftSize);
        fft.performFrequencyOnlyForwardTransform (fftScratch.data());

        //  Write the inactive frame, then publish it (release).
        const int w = magIndex.load (std::memory_order_relaxed) ^ 1;
        auto& dst = magBuf[(size_t) w];
        constexpr float norm = 2.0f / kFftSize;
        for (int i = 0; i < kNumBins; ++i)
        {
            const float mag = fftScratch[(size_t) i] * norm;
            const float dB  = juce::Decibels::gainToDecibels (mag, -100.0f);
            dst[(size_t) i] = juce::jlimit (0.0f, 1.0f, (dB + 100.0f) / 100.0f);
        }
        magIndex.store (w, std::memory_order_release);
    }

    void PhenotypeAudioProcessor::copyFftFrame (float* dest) const noexcept
    {
        const int r = magIndex.load (std::memory_order_acquire);
        const auto& src = magBuf[(size_t) r];
        std::copy (src.begin(), src.end(), dest);
    }

    //==========================================================================
    //  Factory presets (program interface)
    //==========================================================================
    int PhenotypeAudioProcessor::getNumPrograms()
    {
        return juce::jmax (1, library.size());
    }

    const juce::String PhenotypeAudioProcessor::getProgramName (int index)
    {
        return library.nameAt (index);
    }

    void PhenotypeAudioProcessor::setCurrentProgram (int index)
    {
        const auto* e = library.at (index);
        if (e == nullptr)
            return;
        currentProgram = index;

        //  Start from defaults, then apply the preset's overrides.
        for (const auto& d : params::kDefs)
            if (auto* p = apvts.getParameter (d.id))
                p->setValueNotifyingHost (d.defaultValue);

        for (const auto& kv : e->params)
            if (auto* p = apvts.getParameter (kv.first))
                p->setValueNotifyingHost (kv.second);

        //  HQ genome: load the preset's own sample, or fall back to the built-in
        //  wavetable genome when the preset carries none.
        if (e->sample != juce::File())
            loadSampleFile (e->sample);
        else
            granular.useBuiltinGenome();
    }

    void PhenotypeAudioProcessor::rescanLibrary()
    {
        library.clear();
        library.seedFactory();
        library.scan (PresetLibrary::userPresetDir());
        if (currentProgram >= library.size())
            currentProgram = 0;
        updateHostDisplay();
    }

    bool PhenotypeAudioProcessor::importBank (const juce::File& source)
    {
        const auto dest = PresetLibrary::userPresetDir();
        bool ok = false;

        if (source.isDirectory())
        {
            //  Copy the whole DLC folder (bank + its samples) into the library.
            ok = source.copyDirectoryTo (dest.getChildFile (source.getFileName()));
        }
        else if (source.hasFileExtension ("phbank"))
        {
            //  Copy the .phbank into its own folder, bringing a sibling
            //  "samples" directory along if present.
            const auto folder = dest.getChildFile (source.getFileNameWithoutExtension());
            folder.createDirectory();
            ok = source.copyFileTo (folder.getChildFile (source.getFileName()));
            const auto sib = source.getParentDirectory().getChildFile ("samples");
            if (sib.isDirectory())
                sib.copyDirectoryTo (folder.getChildFile ("samples"));
        }

        if (ok)
            rescanLibrary();
        return ok;
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
