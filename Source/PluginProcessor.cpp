#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

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
    engine.setParameters (&synthParams);

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

    return { params.begin(), params.end() };
}

void MoogSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    masterGain.reset (sampleRate, 0.02);

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

    engine.setExternalBend (uiPitchBend.load()
                            * apvts.getRawParameterValue (ParamID::pitchBendRange)->load());
    engine.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());

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
