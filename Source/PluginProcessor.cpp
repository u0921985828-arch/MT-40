#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr double kVibratoHz = 5.5;          // §6: vibrato on = 5.5 Hz
    constexpr float  kVibratoDepthSemis = 0.20f; // subtle pitch modulation
    constexpr double kSustainReleaseMult = 3.0;  // §3.3: 3x when sustain ON
    // Octave the auto-chord accompaniment plays in (above the split root).
    constexpr int kChordVoicingOctave = 0;
}

ST40AudioProcessor::ST40AudioProcessor()
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

void ST40AudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    melodic.prepare (sr);
    chordVoices.prepare (sr);
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

bool ST40AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void ST40AudioProcessor::updateFromParameters()
{
    const int patch = static_cast<int> (*pPatch);
    if (patch != currentPatch)
    {
        currentPatch = patch;
        const auto& presets = getMelodicPresets();
        const int idx = std::clamp (patch, 0, static_cast<int> (presets.size()) - 1);
        melodic.setPreset (presets[idx]);
        chordVoices.setPreset (presets[idx]);
    }

    const double relMult = *pSustain > 0.5f ? kSustainReleaseMult : 1.0;
    melodic.setReleaseMultiplier (relMult);
    chordVoices.setReleaseMultiplier (relMult);

    rhythm.setTempo (*pTempo);
    rhythm.setRhythm (static_cast<int> (*pRhythmIdx));
    rhythm.setRhythmGain (*pRhythm);
    rhythm.setBassGain (*pBass);
}

// ---------------------------------------------------------------------------
// MIDI handling
// ---------------------------------------------------------------------------
void ST40AudioProcessor::handleMidiMessage (const juce::MidiMessage& m)
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
        chordVoices.allNotesOff();
        heldChordZoneNotes.clear();
        activeChordTones.clear();
    }
}

void ST40AudioProcessor::handleNoteOn (int note, float velocity)
{
    const int mode = static_cast<int> (*pMode); // 0 Off, 1 Play, 2 Chord

    if (mode == 2 && isChordZone (note))
    {
        // Chord/bass trigger zone. Register the key and update the chord root
        // BEFORE starting the rhythm, so the very first bass hit of step 0 uses
        // this chord rather than the stale/default root.
        // Dedup: a repeated Note-On (retrigger, no intervening Note-Off) must
        // not leave an orphaned entry that keeps the chord stuck.
        if (std::find (heldChordZoneNotes.begin(), heldChordZoneNotes.end(), note)
                == heldChordZoneNotes.end())
            heldChordZoneNotes.push_back (note);
        recomputeChord();
        rhythm.noteOnInSplitZone();
        return;
    }

    if (mode == 0)
        return; // keyboard off: no melodic sound

    // Play mode, or the melodic zone in Chord mode.
    melodic.noteOn (note, velocity);
}

void ST40AudioProcessor::handleNoteOff (int note)
{
    // Release the note from whichever path actually owns it, independent of the
    // current mode: the mode (or CC85 / automation) may have changed since the
    // Note-On, so branching on the *current* mode here would leak a stuck note.
    auto it = std::find (heldChordZoneNotes.begin(), heldChordZoneNotes.end(), note);
    if (it != heldChordZoneNotes.end())
    {
        heldChordZoneNotes.erase (it);
        if (heldChordZoneNotes.empty())
            rhythm.allSplitNotesReleased();
        recomputeChord();
    }

    // Harmless if the note was never a melodic voice.
    melodic.noteOff (note);
}

void ST40AudioProcessor::recomputeChord()
{
    const auto chord = AutoChord::detect (heldChordZoneNotes);

    if (! chord.valid)
    {
        for (int n : activeChordTones)
            chordVoices.noteOff (n);
        activeChordTones.clear();
        rhythm.setChordRoot (-1);
        return;
    }

    // Bass follows the chord root (dedicated monophonic voice, §2).
    rhythm.setChordRoot (chord.bassNote);

    // Build the new accompaniment voicing (one octave above the split root).
    std::vector<int> newTones;
    newTones.reserve (4);
    for (int i = 0; i < chord.numTones; ++i)
        newTones.push_back (chord.tones[i] + 12 * (1 + kChordVoicingOctave));

    // Only re-voice when the chord actually changed; otherwise adding/removing
    // a redundant key would needlessly re-attack the held chord (clicks).
    if (newTones == activeChordTones)
        return;

    for (int n : activeChordTones)
        chordVoices.noteOff (n);

    // The accompaniment plays in its OWN voice pool, so releasing it never
    // steals a note the player is holding on the melody manual.
    for (int n : newTones)
        chordVoices.noteOn (n, 0.6f);

    activeChordTones = std::move (newTones);
}

void ST40AudioProcessor::handleControlChange (int cc, int value)
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
            // §6: discrete values 0 = Off, 1 = Play, 2 = Chord. The CC value is
            // the choice index directly (higher values clamp to the last mode).
            const int idx = std::clamp (value, 0, 2);
            if (auto* p = apvts.getParameter (ParamIDs::kbdMode))
                p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (idx)));
            break;
        }

        case MidiCC::rhythmSelect:
        {
            // §6: integer 0..5 sent directly as the CC value (clamped).
            const int idx = std::clamp (value, 0, RhythmEngine::kNumRhythms - 1);
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
float ST40AudioProcessor::renderNextSample()
{
    // Global vibrato LFO (§6, CC1: on = 5.5 Hz).
    float lfo = 0.0f;
    if (*pVibrato > 0.5f)
        lfo = static_cast<float> (std::sin (2.0 * M_PI * lfoPhase));
    lfoPhase += kVibratoHz / sampleRate;
    if (lfoPhase >= 1.0) lfoPhase -= 1.0;

    const float vibratoDepth = (*pVibrato > 0.5f) ? kVibratoDepthSemis : 0.0f;

    const float mel = melodic.render (lfo, vibratoDepth)
                    + chordVoices.render (lfo, vibratoDepth);
    const float rhy = rhythm.process(); // already includes rhythm+bass gains

    const float master = *pMaster;
    // Sum the busses, trim headroom, then soft-clip the master output. The
    // rhythm bus can transiently sum several simultaneous hits above unity;
    // the tanh stage models the ST-40's analog output saturation and keeps
    // the signal bounded to [-1, 1] instead of clipping harshly.
    const float mix = (mel * 0.45f + rhy * 0.6f) * master;
    return std::tanh (mix);
}

void ST40AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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
void ST40AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ST40AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void ST40AudioProcessor::injectNoteOn (int note, float velocity)
{
    auto m = juce::MidiMessage::noteOn (1, note, velocity);
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    keyboardCollector.addMessageToQueue (m);
}

void ST40AudioProcessor::injectNoteOff (int note)
{
    auto m = juce::MidiMessage::noteOff (1, note);
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    keyboardCollector.addMessageToQueue (m);
}

juce::AudioProcessorEditor* ST40AudioProcessor::createEditor()
{
    return new ST40AudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ST40AudioProcessor();
}
