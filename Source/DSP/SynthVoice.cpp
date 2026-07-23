#include "SynthVoice.h"

void SynthVoice::prepareToPlay (double newSampleRate, int samplesPerBlock, int numChannels)
{
    sampleRate = newSampleRate;

    osc1.prepare (sampleRate);
    osc2.prepare (sampleRate);
    osc3.prepare (sampleRate);
    filter.prepare (sampleRate);
    filterEnv.prepare (sampleRate);
    ampEnv.prepare (sampleRate);
    noise.reset();

    voiceBuffer.setSize (1, samplesPerBlock, false, false, true);
    juce::ignoreUnused (numChannels);
}

float SynthVoice::midiNoteToHz (float noteNumber) const noexcept
{
    return 440.0f * std::pow (2.0f, (noteNumber - 69.0f) / 12.0f);
}

void SynthVoice::startNote (int midiNoteNumber, float velocity,
                            juce::SynthesiserSound*, int currentPitchWheelPosition)
{
    targetNoteNumber = (float) midiNoteNumber;

    // If no note is currently sounding (envelope idle), jump straight to pitch;
    // otherwise let glide slew from the previous note for legato portamento.
    if (! ampEnv.isActive())
        glidingNoteNumber = targetNoteNumber;

    velocityGain = 0.2f + 0.8f * velocity;  // keep some floor so soft notes are audible

    pitchWheelMoved (currentPitchWheelPosition);

    filterEnv.noteOn();
    ampEnv.noteOn();
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        filterEnv.noteOff();
        ampEnv.noteOff();
    }
    else
    {
        filterEnv.reset();
        ampEnv.reset();
        clearCurrentNote();
    }
}

void SynthVoice::pitchWheelMoved (int newPitchWheelValue)
{
    pitchWheelPosition = newPitchWheelValue;
    const float norm = (newPitchWheelValue - 8192) / 8192.0f; // -1..+1
    pitchBendSemitones = norm * c.bendRange;
}

void SynthVoice::updateBlockParameters()
{
    if (params == nullptr)
        return;

    c.masterTune = params->masterTune->load();
    c.glideOn    = params->glideOn->load() > 0.5f;
    c.bendRange  = params->pitchBendRange->load();

    // Glide coefficient: one-pole slew reaching the target note over glideTime.
    const float glideTime = params->glideTime->load();
    if (! c.glideOn || glideTime <= 0.0001f)
        c.glideCoef = 1.0f;
    else
        c.glideCoef = (float) (1.0 - std::exp (-5.0 / juce::jmax (1.0, glideTime * sampleRate)));

    c.osc1Wave  = (int) params->osc1Wave->load();
    c.osc1Range = (int) params->osc1Range->load();
    c.osc2Wave  = (int) params->osc2Wave->load();
    c.osc2Range = (int) params->osc2Range->load();
    c.osc3Wave  = (int) params->osc3Wave->load();
    c.osc3Range = (int) params->osc3Range->load();

    c.osc2Detune = params->osc2Detune->load();
    c.osc2Coarse = params->osc2Coarse->load();
    c.osc3Detune = params->osc3Detune->load();
    c.osc3Coarse = params->osc3Coarse->load();
    c.osc3Lfo    = params->osc3LfoMode->load() > 0.5f;
    c.osc3ModDepth = params->osc3ModDepth->load();

    c.osc1Vol = params->mixOsc1Vol->load();
    c.osc2Vol = params->mixOsc2Vol->load();
    c.osc3Vol = params->mixOsc3Vol->load();
    c.noiseVol = params->mixNoiseVol->load();
    c.feedbackDrive = params->mixFeedback->load();

    c.osc1On = params->mixOsc1On->load() > 0.5f;
    c.osc2On = params->mixOsc2On->load() > 0.5f;
    c.osc3On = params->mixOsc3On->load() > 0.5f;
    c.noiseOn = params->mixNoiseOn->load() > 0.5f;
    c.feedbackOn = params->mixFeedbackOn->load() > 0.5f;

    c.cutoff  = params->filterCutoff->load();
    c.reso    = params->filterReso->load();
    c.contour = params->filterEnv->load();

    const int keyTrackIndex = (int) params->filterKeyTrack->load();
    c.keyTrack = (keyTrackIndex == 0) ? 0.0f : (keyTrackIndex == 1 ? 0.5f : 1.0f);

    // Push envelope / filter times & shapes down to the DSP objects.
    filterEnv.setAttackMs  (params->filterAttack->load());
    filterEnv.setDecayMs   (params->filterDecay->load());
    filterEnv.setSustain   (params->filterSustain->load());
    filterEnv.setReleaseMs (params->filterRelease->load());

    ampEnv.setAttackMs  (params->ampAttack->load());
    ampEnv.setDecayMs   (params->ampDecay->load());
    ampEnv.setSustain   (params->ampSustain->load());
    ampEnv.setReleaseMs (params->ampRelease->load());

    osc1.setWaveform (static_cast<MoogOscillator::Waveform> (c.osc1Wave));
    osc2.setWaveform (static_cast<MoogOscillator::Waveform> (c.osc2Wave));
    osc3.setWaveform (static_cast<MoogOscillator::Waveform> (c.osc3Wave));

    noise.setType (params->noiseType->load() > 0.5f ? NoiseGenerator::Type::pink
                                                     : NoiseGenerator::Type::white);

    filter.setResonance (c.reso);
    filter.setDrive (c.feedbackOn ? c.feedbackDrive : 0.0f);
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                  int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    updateBlockParameters();

    const float osc1Mult = ParamChoices::rangeMultiplier (c.osc1Range);
    const float osc2Mult = ParamChoices::rangeMultiplier (c.osc2Range);
    const float osc3Mult = ParamChoices::rangeMultiplier (c.osc3Range);

    for (int i = 0; i < numSamples; ++i)
    {
        // ---- Portamento / glide --------------------------------------------
        glidingNoteNumber += c.glideCoef * (targetNoteNumber - glidingNoteNumber);

        // ---- Osc 3 as modulation LFO ---------------------------------------
        float pitchModSemis  = 0.0f;
        float cutoffModOctave = 0.0f;
        if (c.osc3Lfo)
        {
            const float lfoNote = glidingNoteNumber + c.masterTune + c.osc3Coarse + c.osc3Detune;
            osc3.setFrequency (midiNoteToHz (lfoNote) * osc3Mult);
            const float lfo = osc3.getNextLfoSample();
            pitchModSemis   = lfo * c.osc3ModDepth * 12.0f;   // up to +/- 1 octave of vibrato
            cutoffModOctave = lfo * c.osc3ModDepth * 2.0f;    // filter sweep
        }

        const float baseNote = glidingNoteNumber + c.masterTune + pitchBendSemitones + pitchModSemis;

        // ---- Oscillator bank -----------------------------------------------
        float mix = 0.0f;

        if (c.osc1On)
        {
            osc1.setFrequency (midiNoteToHz (baseNote) * osc1Mult);
            mix += osc1.getNextSample() * c.osc1Vol;
        }

        if (c.osc2On)
        {
            osc2.setFrequency (midiNoteToHz (baseNote + c.osc2Coarse + c.osc2Detune) * osc2Mult);
            mix += osc2.getNextSample() * c.osc2Vol;
        }

        if (c.osc3On && ! c.osc3Lfo)
        {
            osc3.setFrequency (midiNoteToHz (baseNote + c.osc3Coarse + c.osc3Detune) * osc3Mult);
            mix += osc3.getNextSample() * c.osc3Vol;
        }

        if (c.noiseOn)
            mix += noise.getNextSample() * c.noiseVol;

        // ---- Feedback / external-input path --------------------------------
        if (c.feedbackOn)
            mix += feedbackSample * c.feedbackDrive * 0.7f;

        // ---- Filter with envelope contour + key tracking -------------------
        const float fEnv = filterEnv.getNextSample();

        // Keyboard tracking: shift cutoff in octaves relative to middle C.
        const float keyOctaves = c.keyTrack * (glidingNoteNumber - 60.0f) / 12.0f;

        // Contour: the filter envelope opens the cutoff by up to ~5 octaves.
        const float envOctaves = c.contour * fEnv * 5.0f;

        float cutoffHz = c.cutoff * std::pow (2.0f, keyOctaves + envOctaves + cutoffModOctave);
        cutoffHz = juce::jlimit (20.0f, (float) (sampleRate * 0.49), cutoffHz);
        filter.setCutoff (cutoffHz);

        float filtered = filter.processSample (mix);
        feedbackSample = filtered;

        // ---- Loudness envelope + velocity ----------------------------------
        const float aEnv = ampEnv.getNextSample();
        const float sample = filtered * aEnv * velocityGain;

        // Sum into all output channels.
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.addSample (ch, startSample + i, sample);

        // A finished amp envelope means the voice is free again.
        if (! ampEnv.isActive())
        {
            clearCurrentNote();
            feedbackSample = 0.0f;
            break;
        }
    }
}
