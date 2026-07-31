#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include <algorithm>
#include <cmath>

namespace
{
    // Logarithmic (frequency-style) range with a chosen midpoint.
    juce::NormalisableRange<float> logRange (float min, float max, float centre, float interval = 0.0f)
    {
        juce::NormalisableRange<float> range (min, max, interval);
        range.setSkewForCentre (centre);
        return range;
    }

    juce::String msValueToText (float ms, int)
    {
        if (ms >= 1000.0f)
            return juce::String (ms / 1000.0f, 2) + " s";
        return juce::String (ms, 0) + " ms";
    }

    juce::String hzValueToText (float hz, int)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, 2) + " kHz";
        return juce::String (hz, 0) + " Hz";
    }
}

MoogSynthAudioProcessor::MoogSynthAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    synthParams.connect (apvts);
    for (auto& v : voices)
        v.setParameters (&synthParams);
    for (auto& n : voiceNote)
        n = -1;

    for (auto& s : scope.buffer)
        s.store (0.0f, std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MoogSynthAudioProcessor::createParameterLayout()
{
    using APF   = juce::AudioParameterFloat;
    using APC   = juce::AudioParameterChoice;
    using APB   = juce::AudioParameterBool;
    using APInt = juce::AudioParameterInt;
    using Attr  = juce::AudioParameterFloatAttributes;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ---- Controllers --------------------------------------------------------
    params.push_back (std::make_unique<APF> (ParamID::masterVolume, "Master Volume",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f));
    params.push_back (std::make_unique<APF> (ParamID::masterTune, "Master Tune",
                                             juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                             Attr().withLabel ("st")));
    params.push_back (std::make_unique<APB> (ParamID::glideOn, "Glide On", false));
    params.push_back (std::make_unique<APF> (ParamID::glideTime, "Glide Time",
                                             logRange (0.0f, 5.0f, 0.5f), 0.05f,
                                             Attr().withLabel ("s")));
    params.push_back (std::make_unique<APInt> (ParamID::pitchBendRange, "Pitch Bend Range", 0, 24, 2));
    params.push_back (std::make_unique<APF> (ParamID::modWheel, "Mod Wheel",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<APF> (ParamID::modMix, "Modulation Mix",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<APB> (ParamID::modOscOn, "Oscillator Modulation", false));
    params.push_back (std::make_unique<APB> (ParamID::modFilterOn, "Filter Modulation", false));
    params.push_back (std::make_unique<APB> (ParamID::polyOn, "Polyphonic", false));

    // ---- Oscillator Bank ----------------------------------------------------
    params.push_back (std::make_unique<APC> (ParamID::osc1Wave, "Osc 1 Waveform", ParamChoices::waveforms(), 2));
    params.push_back (std::make_unique<APC> (ParamID::osc1Range, "Osc 1 Range", ParamChoices::ranges(), 3));

    params.push_back (std::make_unique<APC> (ParamID::osc2Wave, "Osc 2 Waveform", ParamChoices::waveforms(), 2));
    params.push_back (std::make_unique<APC> (ParamID::osc2Range, "Osc 2 Range", ParamChoices::ranges(), 3));
    params.push_back (std::make_unique<APF> (ParamID::osc2Detune, "Osc 2 Tune",
                                             juce::NormalisableRange<float> (-7.0f, 7.0f, 0.01f), 0.0f,
                                             Attr().withLabel ("st")));

    params.push_back (std::make_unique<APC> (ParamID::osc3Wave, "Osc 3 Waveform", ParamChoices::waveforms(), 2));
    params.push_back (std::make_unique<APC> (ParamID::osc3Range, "Osc 3 Range", ParamChoices::ranges(), 3));
    params.push_back (std::make_unique<APF> (ParamID::osc3Detune, "Osc 3 Tune",
                                             juce::NormalisableRange<float> (-7.0f, 7.0f, 0.01f), 0.0f,
                                             Attr().withLabel ("st")));
    params.push_back (std::make_unique<APB> (ParamID::osc3KbControl, "Osc 3 Keyboard Control", true));

    // ---- Mixer --------------------------------------------------------------
    params.push_back (std::make_unique<APF> (ParamID::mixOsc1Vol, "Osc 1 Volume",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f));
    params.push_back (std::make_unique<APF> (ParamID::mixOsc2Vol, "Osc 2 Volume",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<APF> (ParamID::mixOsc3Vol, "Osc 3 Volume",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<APF> (ParamID::mixNoiseVol, "Noise Volume",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<APF> (ParamID::mixExtVol, "External / Feedback",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<APB> (ParamID::mixOsc1On,  "Osc 1 On",   true));
    params.push_back (std::make_unique<APB> (ParamID::mixOsc2On,  "Osc 2 On",   false));
    params.push_back (std::make_unique<APB> (ParamID::mixOsc3On,  "Osc 3 On",   false));
    params.push_back (std::make_unique<APB> (ParamID::mixNoiseOn, "Noise On",   false));
    params.push_back (std::make_unique<APB> (ParamID::mixExtOn,   "External On", false));
    params.push_back (std::make_unique<APC> (ParamID::noiseType, "Noise Type", ParamChoices::noiseTypes(), 0));

    // ---- Modifiers: Filter --------------------------------------------------
    params.push_back (std::make_unique<APF> (ParamID::filterCutoff, "Cutoff Frequency",
                                             logRange (20.0f, 20000.0f, 1000.0f), 12000.0f,
                                             Attr().withStringFromValueFunction (hzValueToText)));
    params.push_back (std::make_unique<APF> (ParamID::filterReso, "Emphasis",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.1f));
    params.push_back (std::make_unique<APF> (ParamID::filterEnv, "Amount of Contour",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    params.push_back (std::make_unique<APC> (ParamID::filterKeyTrack, "Keyboard Control",
                                             ParamChoices::keyTracking(), 2));

    params.push_back (std::make_unique<APF> (ParamID::filterAttack, "Filter Attack",
                                             logRange (1.0f, 10000.0f, 200.0f), 20.0f,
                                             Attr().withStringFromValueFunction (msValueToText)));
    params.push_back (std::make_unique<APF> (ParamID::filterDecay, "Filter Decay",
                                             logRange (4.0f, 35000.0f, 600.0f), 400.0f,
                                             Attr().withStringFromValueFunction (msValueToText)));
    params.push_back (std::make_unique<APF> (ParamID::filterSustain, "Filter Sustain",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f));
    params.push_back (std::make_unique<APF> (ParamID::filterRelease, "Filter Release",
                                             logRange (1.0f, 10000.0f, 200.0f), 200.0f,
                                             Attr().withStringFromValueFunction (msValueToText)));

    // ---- Output: Loudness envelope ------------------------------------------
    params.push_back (std::make_unique<APF> (ParamID::ampAttack, "Amp Attack",
                                             logRange (1.0f, 10000.0f, 200.0f), 10.0f,
                                             Attr().withStringFromValueFunction (msValueToText)));
    params.push_back (std::make_unique<APF> (ParamID::ampDecay, "Amp Decay",
                                             logRange (4.0f, 35000.0f, 600.0f), 400.0f,
                                             Attr().withStringFromValueFunction (msValueToText)));
    params.push_back (std::make_unique<APF> (ParamID::ampSustain, "Amp Sustain",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f));
    params.push_back (std::make_unique<APF> (ParamID::ampRelease, "Amp Release",
                                             logRange (1.0f, 10000.0f, 200.0f), 150.0f,
                                             Attr().withStringFromValueFunction (msValueToText)));

    // ---- Analog character ---------------------------------------------------
    params.push_back (std::make_unique<APF> (ParamID::driftAmount, "Analog Drift",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f));
    params.push_back (std::make_unique<APF> (ParamID::filterDrive, "Filter Drive",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<APF> (ParamID::bassThin, "Bass Thinning",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f));

    // ---- Perform: chord + arpeggiator ---------------------------------------
    params.push_back (std::make_unique<APC> (ParamID::chordType, "Chord", ParamChoices::chordTypes(), 0));
    params.push_back (std::make_unique<APB> (ParamID::arpOn, "Arp On", false));
    params.push_back (std::make_unique<APC> (ParamID::arpRate, "Arp Rate", ParamChoices::arpRates(), 1));
    params.push_back (std::make_unique<APC> (ParamID::arpMode, "Arp Mode", ParamChoices::arpModes(), 0));
    params.push_back (std::make_unique<APC> (ParamID::arpOct, "Arp Octaves", ParamChoices::arpOctaves(), 0));
    params.push_back (std::make_unique<APF> (ParamID::arpGate, "Arp Gate",
                                             juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f));
    params.push_back (std::make_unique<APF> (ParamID::arpBpm, "Arp Tempo",
                                             juce::NormalisableRange<float> (40.0f, 240.0f, 1.0f), 120.0f,
                                             Attr().withLabel ("bpm")));

    return { params.begin(), params.end() };
}

void MoogSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    for (auto& v : voices)
    {
        v.prepare (sampleRate, samplesPerBlock);
        v.reset();
    }
    for (auto& b : voiceMidi)
        b.ensureSize (256);
    for (auto& n : voiceNote)
        n = -1;
    ageCounter = 0;
    masterGain.reset (sampleRate, 0.02);

    currentSampleRate = sampleRate;
    perfHeld.clear();
    perfToNextStep = perfToGateOff = 0.0;
    perfArpNote = -1; perfArpPos = 0; perfWasArp = false;

    const auto numOut = (size_t) juce::jmax (1, getTotalNumOutputChannels());
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        numOut, oversamplingFactor,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples (juce::roundToInt (oversampler->getLatencyInSamples()));

    for (auto& d : dcX1) d = 0.0f;
    for (auto& d : dcY1) d = 0.0f;
    for (auto& d : lpWarm) d = 0.0f;
    for (auto& d : lpAir)  d = 0.0f;

    // Gentle analog tone shelves (warmth ~ 220 Hz, air ~ 6.5 kHz).
    coefWarm = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 220.0  / sampleRate));
    coefAir  = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 6500.0 / sampleRate));

    for (auto& s : scope.buffer)
        s.store (0.0f, std::memory_order_relaxed);
    scope.writePos.store (0, std::memory_order_relaxed);
}

void MoogSynthAudioProcessor::pushScope (float sample) noexcept
{
    const int p = scope.writePos.load (std::memory_order_relaxed);
    scope.buffer[(size_t) p].store (sample, std::memory_order_relaxed);
    scope.writePos.store ((p + 1) & Scope::mask, std::memory_order_release);
}

int MoogSynthAudioProcessor::readScope (float* dest, int numSamples) const noexcept
{
    numSamples = juce::jmin (numSamples, Scope::size);
    const int p = scope.writePos.load (std::memory_order_acquire);
    for (int i = 0; i < numSamples; ++i)
        dest[i] = scope.buffer[(size_t) ((p - numSamples + i) & Scope::mask)].load (std::memory_order_relaxed);
    return numSamples;
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MoogSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}
#endif

void MoogSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    buffer.clear();

    // Merge notes played on the on-screen keyboard with incoming host MIDI.
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    const float extBend = uiPitchBend.load()
                          * apvts.getRawParameterValue (ParamID::pitchBendRange)->load();
    for (auto& v : voices)
        v.setExternalBend (extBend);

    const bool poly = apvts.getRawParameterValue (ParamID::polyOn)->load() > 0.5f;
    const int  numSamples = buffer.getNumSamples();

    // Perform layer: expand chords / run the arpeggiator on the MIDI stream.
    applyPerform (midiMessages, numSamples);

    // Reset the bank when switching mode so no note gets stuck across the change.
    if (poly != wasPoly)
    {
        for (auto& v : voices) v.reset();
        for (auto& n : voiceNote) n = -1;
        wasPoly = poly;
    }

    if (! poly)
    {
        voices[0].renderNextBlock (buffer, midiMessages, 0, numSamples);
    }
    else
    {
        for (auto& b : voiceMidi) b.clear();

        for (const auto meta : midiMessages)
        {
            const auto msg = meta.getMessage();
            const int  sp  = meta.samplePosition;

            if (msg.isNoteOn())
            {
                const int v = allocateVoice (msg.getNoteNumber());
                voiceMidi[(size_t) v].addEvent (msg, sp);
            }
            else if (msg.isNoteOff())
            {
                const int v = findVoiceForNote (msg.getNoteNumber());
                if (v >= 0)
                {
                    voiceMidi[(size_t) v].addEvent (msg, sp);
                    voiceNote[v] = -1;  // freed (envelope releases)
                }
            }
            else
            {
                // Pitch bend, mod wheel, all-notes-off, etc. go to every voice.
                for (auto& b : voiceMidi) b.addEvent (msg, sp);
                if (msg.isAllNotesOff() || msg.isAllSoundOff())
                    for (auto& n : voiceNote) n = -1;
            }
        }

        for (int i = 0; i < maxVoices; ++i)
            voices[(size_t) i].renderNextBlock (buffer, voiceMidi[(size_t) i], 0, numSamples);

        buffer.applyGain (0.6f); // headroom for stacked voices
    }

    // ---- Master volume -----------------------------------------------------
    masterGain.setTargetValue (apvts.getRawParameterValue (ParamID::masterVolume)->load());

    const int numCh = buffer.getNumChannels();

    // ---- Master gain + analog tone shaping (warmth + air shelves) ----------
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float g = masterGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            const int c = juce::jmin (ch, 1);
            float x = buffer.getSample (ch, i) * g;

            // One-pole low-shelf (adds body) and high-shelf (adds sheen/air).
            lpWarm[c] += coefWarm * (x - lpWarm[c]);
            lpAir[c]  += coefAir  * (x - lpAir[c]);
            x += 0.14f * lpWarm[c];          // warmth
            x += 0.42f * (x - lpAir[c]);     // air (boost content above ~6.5 kHz)

            buffer.setSample (ch, i, x);
        }
    }

    // ---- Oversampled tube stage (asymmetric drive -> even + odd harmonics) --
    if (oversampler != nullptr)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto osBlock = oversampler->processSamplesUp (block);

        for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
        {
            auto* d = osBlock.getChannelPointer (ch);
            for (size_t i = 0; i < osBlock.getNumSamples(); ++i)
            {
                const float x = d[i] * 1.15f;
                // Subtle asymmetry gives the 2nd-harmonic "tube" colour; the
                // tanh provides the soft, bandlimited limiting.
                d[i] = std::tanh (x + 0.045f * x * x);
            }
        }

        oversampler->processSamplesDown (block);
    }

    // ---- DC blocker + metering + scope feed --------------------------------
    constexpr float R = 0.9975f;
    float peak[2] { 0.0f, 0.0f };

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = buffer.getSample (ch, i);
            const int c = juce::jmin (ch, 1);
            const float y = x - dcX1[c] + R * dcY1[c];
            dcX1[c] = x;
            dcY1[c] = y;
            buffer.setSample (ch, i, y);
            mono += y;
            peak[c] = juce::jmax (peak[c], std::abs (y));
        }
        pushScope (numCh > 0 ? mono / (float) numCh : 0.0f);
    }

    meter[0].store (peak[0], std::memory_order_relaxed);
    meter[1].store (numCh > 1 ? peak[1] : peak[0], std::memory_order_relaxed);
}

// Chord expansion + arpeggiator. Rewrites `midi` in place. When Chord=Off and
// Arp=Off this is a transparent pass-through (no behaviour change).
void MoogSynthAudioProcessor::applyPerform (juce::MidiBuffer& midi, int numSamples)
{
    const int  chordIx = (int) apvts.getRawParameterValue (ParamID::chordType)->load();
    const bool arp     = apvts.getRawParameterValue (ParamID::arpOn)->load() > 0.5f;
    const auto& iv     = ParamChoices::chordIntervals()[(size_t) juce::jlimit (0, 9, chordIx)];

    auto held_add = [this] (int n) { if (std::find (perfHeld.begin(), perfHeld.end(), n) == perfHeld.end()) perfHeld.push_back (n); };
    auto held_rm  = [this] (int n) { perfHeld.erase (std::remove (perfHeld.begin(), perfHeld.end(), n), perfHeld.end()); };

    juce::MidiBuffer out;

    if (! arp)
    {
        auto emitChord = [&] (bool on, int note, juce::uint8 vel, int sp)
        {
            for (int step : iv)
            {
                const int n = note + step;
                if (n < 0 || n > 127) continue;
                out.addEvent (on ? juce::MidiMessage::noteOn (1, n, vel)
                                 : juce::MidiMessage::noteOff (1, n), sp);
            }
        };
        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            const int sp = meta.samplePosition;
            if (m.isNoteOn())       { held_add (m.getNoteNumber()); emitChord (true,  m.getNoteNumber(), (juce::uint8) m.getVelocity(), sp); }
            else if (m.isNoteOff()) { held_rm  (m.getNoteNumber()); emitChord (false, m.getNoteNumber(), 0, sp); }
            else                    { out.addEvent (m, sp); }
        }
        if (perfWasArp && perfArpNote >= 0) { out.addEvent (juce::MidiMessage::noteOff (1, perfArpNote), 0); perfArpNote = -1; }
        perfWasArp = false;
        midi.swapWith (out);
        return;
    }

    // ---- Arpeggiator -------------------------------------------------------
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())        held_add (m.getNoteNumber());
        else if (m.isNoteOff())  held_rm  (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) perfHeld.clear();
        else                     out.addEvent (m, meta.samplePosition); // bend / CC pass through
    }

    const int  mode = (int) apvts.getRawParameterValue (ParamID::arpMode)->load();
    const int  oct  = (int) apvts.getRawParameterValue (ParamID::arpOct)->load() + 1;
    const int  rate = (int) apvts.getRawParameterValue (ParamID::arpRate)->load();
    const float gate = juce::jlimit (0.05f, 0.98f, apvts.getRawParameterValue (ParamID::arpGate)->load());

    double bpm = (double) apvts.getRawParameterValue (ParamID::arpBpm)->load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm(); b.hasValue() && *b > 1.0)
                bpm = *b;
    const double stepS = juce::jmax (32.0, currentSampleRate * (60.0 / bpm) / ParamChoices::arpRateSteps (rate));

    auto buildSeq = [&]()
    {
        std::vector<int> base;
        for (int n : perfHeld) for (int s : iv) base.push_back (n + s);
        if (mode != 4) { std::sort (base.begin(), base.end()); base.erase (std::unique (base.begin(), base.end()), base.end()); }
        std::vector<int> seq;
        for (int o = 0; o < oct; ++o) for (int n : base) seq.push_back (n + o * 12);
        if (mode == 1) std::reverse (seq.begin(), seq.end());
        else if (mode == 2 && seq.size() > 2) { auto up = seq; for (int i = (int) seq.size() - 2; i > 0; --i) up.push_back (seq[(size_t) i]); seq.swap (up); }
        return seq;
    };

    if (! perfWasArp) { perfToNextStep = 0.0; perfToGateOff = 0.0; perfArpPos = 0; perfArpNote = -1; }
    perfWasArp = true;

    if (perfHeld.empty())
    {
        if (perfArpNote >= 0) { out.addEvent (juce::MidiMessage::noteOff (1, perfArpNote), 0); perfArpNote = -1; }
        perfToNextStep = 0.0;
        midi.swapWith (out);
        return;
    }

    int pos = 0;
    while (pos < numSamples)
    {
        const double dGate = (perfArpNote >= 0) ? perfToGateOff : 1.0e18;
        double adv = juce::jmin ((double) (numSamples - pos), juce::jmin (perfToNextStep, dGate));
        if (adv < 0.0) adv = 0.0;
        const int iadv = (int) adv;
        pos += iadv;
        perfToNextStep -= iadv;
        if (perfArpNote >= 0) perfToGateOff -= iadv;
        const int at = juce::jlimit (0, juce::jmax (0, numSamples - 1), pos);

        bool did = false;
        if (perfArpNote >= 0 && perfToGateOff <= 0.0)
        {
            out.addEvent (juce::MidiMessage::noteOff (1, perfArpNote), at); perfArpNote = -1; did = true;
        }
        if (perfToNextStep <= 0.0)
        {
            if (perfArpNote >= 0) { out.addEvent (juce::MidiMessage::noteOff (1, perfArpNote), at); perfArpNote = -1; }
            const auto seq = buildSeq();
            if (! seq.empty())
            {
                int idx;
                if (mode == 3) { perfRandSeed = perfRandSeed * 1664525u + 1013904223u; idx = (int) (perfRandSeed % (uint32_t) seq.size()); }
                else           { idx = perfArpPos % (int) seq.size(); }
                perfArpPos = idx + 1;
                const int note = juce::jlimit (0, 127, seq[(size_t) idx]);
                out.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), at);
                perfArpNote = note; perfToGateOff = stepS * gate;
            }
            perfToNextStep += stepS;
            did = true;
        }
        if (iadv == 0 && ! did) break; // no progress guard
    }

    midi.swapWith (out);
}

int MoogSynthAudioProcessor::allocateVoice (int note) noexcept
{
    // 1) A fully idle voice (free and silent).
    for (int i = 0; i < maxVoices; ++i)
        if (voiceNote[i] < 0 && ! voices[(size_t) i].isActive())
        {
            voiceNote[i] = note; voiceAge[i] = ageCounter++;
            return i;
        }

    // 2) The oldest released voice (freed but still ringing out).
    int best = -1; uint64_t oldest = UINT64_MAX;
    for (int i = 0; i < maxVoices; ++i)
        if (voiceNote[i] < 0 && voiceAge[i] < oldest) { oldest = voiceAge[i]; best = i; }

    // 3) Otherwise steal the oldest still-held voice (reset to avoid a stuck note).
    if (best < 0)
    {
        oldest = UINT64_MAX;
        for (int i = 0; i < maxVoices; ++i)
            if (voiceAge[i] < oldest) { oldest = voiceAge[i]; best = i; }
        voices[(size_t) best].reset();
    }

    voiceNote[best] = note; voiceAge[best] = ageCounter++;
    return best;
}

int MoogSynthAudioProcessor::findVoiceForNote (int note) const noexcept
{
    // Most recently allocated voice holding this note.
    int best = -1; uint64_t newest = 0;
    for (int i = 0; i < maxVoices; ++i)
        if (voiceNote[i] == note && voiceAge[i] >= newest) { newest = voiceAge[i]; best = i; }
    return best;
}

juce::AudioProcessorEditor* MoogSynthAudioProcessor::createEditor()
{
    return new MoogSynthAudioProcessorEditor (*this);
}

void MoogSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        auto xml = state.createXml();
        copyXmlToBinary (*xml, destData);
    }
}

void MoogSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MoogSynthAudioProcessor();
}
