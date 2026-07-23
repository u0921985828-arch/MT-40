#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthVoice.h"
#include "SynthParameters.h"

/**
    Polyphonic voice manager.

    Built on juce::Synthesiser (which handles MIDI note-on/off routing and
    voice stealing) and populated with a fixed pool of SynthVoice instances.
    All voices share a single, cached SynthParameters snapshot of the APVTS.
*/
class PolyphonicSynthesizer : public juce::Synthesiser
{
public:
    static constexpr int defaultNumVoices = 16;

    PolyphonicSynthesizer() = default;

    void setup (juce::AudioProcessorValueTreeState& apvts, int numVoices = defaultNumVoices)
    {
        sharedParams.connect (apvts);

        clearVoices();
        clearSounds();

        addSound (new MoogSound());

        for (int i = 0; i < numVoices; ++i)
        {
            auto* voice = new SynthVoice();
            voice->setParameters (&sharedParams);
            addVoice (voice);
        }
    }

    void prepareToPlay (double newSampleRate, int samplesPerBlock, int numChannels)
    {
        setCurrentPlaybackSampleRate (newSampleRate);

        for (int i = 0; i < getNumVoices(); ++i)
            if (auto* voice = dynamic_cast<SynthVoice*> (getVoice (i)))
                voice->prepareToPlay (newSampleRate, samplesPerBlock, numChannels);
    }

private:
    SynthParameters sharedParams;
};
