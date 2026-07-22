#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr double kVibratoHz = 5.5;          // §6: vibrato on = 5.5 Hz
    constexpr float  kVibratoDepthSemis = 0.20f; // subtle pitch modulation
    constexpr double kSustainReleaseMult = 3.0;  // §3.3: 3x when sustain ON
    // Octave the Casio-Chord accompaniment plays in (above the split root).
    constexpr int kChordVoicingOctave = 0;
}

CasioMT40AudioProcessor::CasioMT40AudioProcessor()
    : AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    pMaster    = apvts.getRawParameterValue (ParamIDs::masterVolume);
    pRhythm    = apvts.getRawParameterValue (ParamIDs::rhythmVolume);
    pBass      = apvts.getRawParameterValue (ParamIDs::bassVolume);
    pTempo     = apvts.getRawParameterValue (ParamIDs::tempo);
    pVibrato   = apvts.getRawParameterValue (ParamIDs::vibrato);
    pSustain   = apvts.getRawParameterValue (ParamIDs::sustain);
    pMode      = apvts.getRawParameterValue (ParamIDs::kbdMode);
    pRhythmIdx = apvts.getRawParameterValue (ParamIDs::rhythmIdx);
    pPatch     = apvts.getRawParameterValue (ParamIDs::patchIdx);
}

void CasioMT40AudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    melodic.prepare (sr);
    rhythm.prepare (sr);
    keyboardCollector.reset (sr);
    lfoPhase = 0.0;
    currentPatch = -1;
    heldChordZoneNotes.clear();
    activeChordTones.clear();
    heldChordZoneNotes.reserve (16); // avoid audio-thread reallocation
    activeChordTones.reserve (8);
    updateFromParameters();
}

bool CasioMT40AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void CasioMT40AudioProcessor::updateFromParameters()
{
    const int patch = static_cast<int> (*pPatch);
    if (patch != currentPatch)
    {
        currentPatch = patch;
        const auto& presets = getMelodicPresets();
        const int idx = std::clamp (patch, 0, static_cast<int> (presets.size()) - 1);
        melodic.setPreset (presets[idx]);
    }

    melodic.setReleaseMultiplier (*pSustain > 0.5f ? kSustainReleaseMult : 1.0);

    rhythm.setTempo (*pTempo);
    rhythm.setRhythm (static_cast<int> (*pRhythmIdx));
    rhythm.setRhythmGain (*pRhythm);
    rhythm.setBassGain (*pBass);
}

// ---------------------------------------------------------------------------
// MIDI handling
// ---------------------------------------------------------------------------
void CasioMT40AudioProcessor::handleMidiMessage (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        handleNoteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        handleNoteOff (m.getNoteNumber());
    else if (m.isController())
        handleControlChange (m.getControllerNumber(), m.getControllerValue());
    else if (m.isProgramChange())
        apvts.getParameter (ParamIDs::patchIdx)
            ->setValueNotifyingHost (apvts.getParameter (ParamIDs::patchIdx)
                ->convertTo0to1 (static_cast<float> (m.getProgramChangeNumber())));
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        melodic.allNotesOff();
        heldChordZoneNotes.clear();
        activeChordTones.clear();
    }
}

void CasioMT40AudioProcessor::handleNoteOn (int note, float velocity)
{
    const int mode = static_cast<int> (*pMode); // 0 Off, 1 Play, 2 Chord

    if (mode == 2 && isChordZone (note))
    {
        // Chord/bass trigger zone: start the rhythm and update the chord.
        rhythm.noteOnInSplitZone();
        heldChordZoneNotes.push_back (note);
        recomputeChord();
        return;
    }

    if (mode == 0)
        return; // keyboard off: no melodic sound

    // Play mode, or the melodic zone in Chord mode.
    melodic.noteOn (note, velocity);
}

void CasioMT40AudioProcessor::handleNoteOff (int note)
{
    const int mode = static_cast<int> (*pMode);

    if (mode == 2 && isChordZone (note))
    {
        auto it = std::find (heldChordZoneNotes.begin(), heldChordZoneNotes.end(), note);
        if (it != heldChordZoneNotes.end())
            heldChordZoneNotes.erase (it);

        if (heldChordZoneNotes.empty())
            rhythm.allSplitNotesReleased();

        recomputeChord();
        return;
    }

    melodic.noteOff (note);
}

void CasioMT40AudioProcessor::recomputeChord()
{
    // Release the previous accompaniment voicing.
    for (int n : activeChordTones)
        melodic.noteOff (n);
    activeChordTones.clear();

    const auto chord = CasioChord::detect (heldChordZoneNotes);
    if (! chord.valid)
    {
        rhythm.setChordRoot (-1);
        return;
    }

    // Bass follows the chord root (dedicated monophonic voice, §2).
    rhythm.setChordRoot (chord.bassNote);

    // Voice the chord tones through the melodic pool as accompaniment, one
    // octave above the split root so they sit above the bass.
    for (int i = 0; i < chord.numTones; ++i)
    {
        const int n = chord.tones[i] + 12 * (1 + kChordVoicingOctave);
        melodic.noteOn (n, 0.6f);
        activeChordTones.push_back (n);
    }
}

void CasioMT40AudioProcessor::handleControlChange (int cc, int value)
{
    // Map the incoming CC to the corresponding APVTS parameter (§6).
    auto setNorm = [this] (const char* id, float norm)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (norm);
    };

    const float v01 = value / 127.0f;

    switch (cc)
    {
        case MidiCC::masterVolume: setNorm (ParamIDs::masterVolume, v01); break;
        case MidiCC::rhythmVolume: setNorm (ParamIDs::rhythmVolume, v01); break;
        case MidiCC::bassVolume:   setNorm (ParamIDs::bassVolume,   v01); break;
        case MidiCC::vibrato:      setNorm (ParamIDs::vibrato, value >= 64 ? 1.0f : 0.0f); break;
        case MidiCC::sustain:      setNorm (ParamIDs::sustain, value >= 64 ? 1.0f : 0.0f); break;

        case MidiCC::kbdMode:
        {
            // 0 Off, 1 Play, 2 Chord -> choice index normalised.
            const int idx = value < 43 ? 0 : (value < 86 ? 1 : 2);
            if (auto* p = apvts.getParameter (ParamIDs::kbdMode))
                p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (idx)));
            break;
        }

        case MidiCC::rhythmSelect:
        {
            const int idx = std::clamp (value * RhythmEngine::kNumRhythms / 128, 0,
                                        RhythmEngine::kNumRhythms - 1);
            if (auto* p = apvts.getParameter (ParamIDs::rhythmIdx))
                p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (idx)));
            break;
        }

        default: break;
    }
}

// ---------------------------------------------------------------------------
// Audio rendering
// ---------------------------------------------------------------------------
float CasioMT40AudioProcessor::renderNextSample()
{
    // Global vibrato LFO (§6, CC1: on = 5.5 Hz).
    float lfo = 0.0f;
    if (*pVibrato > 0.5f)
        lfo = static_cast<float> (std::sin (2.0 * M_PI * lfoPhase));
    lfoPhase += kVibratoHz / sampleRate;
    if (lfoPhase >= 1.0) lfoPhase -= 1.0;

    const float vibratoDepth = (*pVibrato > 0.5f) ? kVibratoDepthSemis : 0.0f;

    const float mel = melodic.render (lfo, vibratoDepth);
    const float rhy = rhythm.process(); // already includes rhythm+bass gains

    const float master = *pMaster;
    // Sum the busses, trim headroom, then soft-clip the master output. The
    // rhythm bus can transiently sum several simultaneous hits above unity;
    // the tanh stage models the MT-40's analog output saturation and keeps
    // the signal bounded to [-1, 1] instead of clipping harshly.
    const float mix = (mel * 0.45f + rhy * 0.6f) * master;
    return std::tanh (mix);
}

void CasioMT40AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    updateFromParameters();

    // Apply UI-driven transport controls (§5.2) on the audio thread.
    if (synchroPending.exchange (false))
        rhythm.pressSynchro();
    if (startStopPending.exchange (false))
    {
        if (rhythm.getTransport() == RhythmEngine::Transport::Playing)
            rhythm.stop();
        else
            rhythm.start();
    }
    rhythm.setFillHeld (fillHeldUI.load());

    const int numSamples = buffer.getNumSamples();

    // Merge on-screen keyboard notes into the incoming MIDI stream.
    keyboardCollector.removeNextBlockOfMessages (midi, numSamples);
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    int sample = 0;
    for (const auto meta : midi)
    {
        const int ts = juce::jlimit (0, numSamples, meta.samplePosition);
        while (sample < ts)
        {
            const float s = renderNextSample();
            left[sample] = s;
            if (right) right[sample] = s;
            ++sample;
        }
        handleMidiMessage (meta.getMessage());
    }

    while (sample < numSamples)
    {
        const float s = renderNextSample();
        left[sample] = s;
        if (right) right[sample] = s;
        ++sample;
    }
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
void CasioMT40AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CasioMT40AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void CasioMT40AudioProcessor::injectNoteOn (int note, float velocity)
{
    auto m = juce::MidiMessage::noteOn (1, note, velocity);
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    keyboardCollector.addMessageToQueue (m);
}

void CasioMT40AudioProcessor::injectNoteOff (int note)
{
    auto m = juce::MidiMessage::noteOff (1, note);
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    keyboardCollector.addMessageToQueue (m);
}

juce::AudioProcessorEditor* CasioMT40AudioProcessor::createEditor()
{
    return new CasioMT40AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CasioMT40AudioProcessor();
}
