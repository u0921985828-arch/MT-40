#include "MonoSynthEngine.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace
{
    // Sample/wavetable bank: 8 single-cycle timbres generated additively once,
    // shared read-only by all voices (mirrors the web app's tables).
    static constexpr int kWaveLen = 2048;
    using WaveTable = std::array<float, kWaveLen>;

    const std::array<WaveTable, 8>& waveBank()
    {
        static const std::array<WaveTable, 8> bank = []
        {
            std::array<WaveTable, 8> b {};
            auto build = [] (WaveTable& t, auto partial)
            {
                float peak = 0.0f;
                for (int i = 0; i < kWaveLen; ++i)
                {
                    const double ph = juce::MathConstants<double>::twoPi * i / kWaveLen;
                    double s = 0.0;
                    for (int k = 1; k <= 64; ++k)
                    {
                        const double a = partial (k);
                        if (a != 0.0) s += a * std::sin (k * ph);
                    }
                    t[(size_t) i] = (float) s;
                    peak = juce::jmax (peak, std::abs ((float) s));
                }
                if (peak > 0.0f) for (auto& v : t) v /= peak;
            };
            auto organ = [] (int k){ static const double d[]={0,1,0,0.7,0,0.5,0,0,0,0.35}; return k<10? d[k]:0.0; };
            auto bell  = [] (int k){ static const double d[]={0,1,0,0,0.6,0,0,0.4,0,0,0.3}; return k<11? d[k]:0.0; };
            build (b[0], [] (int k){ return (k%2?1.0:0.6)/k; });                                   // Digital
            build (b[1], organ);                                                                    // Organ
            build (b[2], [] (int k){ const double hz=k*110.0, F[]={700,1150,2600};                  // Voice
                     double a=std::exp(-std::pow((hz-260)/120,2));
                     for (double c : F) a+=0.8*std::exp(-std::pow((hz-c)/420,2));
                     return a/std::sqrt((double)k); });
            build (b[3], [] (int k){ return (k%2? 1.0/((double)k*k):0.0); });                        // Reed
            build (b[4], bell);                                                                      // Bell
            build (b[5], [] (int k){ return std::exp(-k*0.12); });                                   // Pluck
            build (b[6], [] (int k){ return k>3? std::exp(-(k-3)*0.2):0.0; });                       // Glass
            build (b[7], [] (int k){ return 1.0/k; });                                               // Saw2
            return b;
        }();
        return bank;
    }
}

void MonoSynthEngine::prepare (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    osc1.prepare (sampleRate);
    osc2.prepare (sampleRate);
    osc3.prepare (sampleRate);
    filter.prepare (sampleRate);
    filterEnv.prepare (sampleRate);
    ampEnv.prepare (sampleRate);
    noise.reset();
    heldNotes.reserve (16);
    juce::ignoreUnused (samplesPerBlock);

    // Give every voice its own stable "unit tolerance" so a poly stack sounds
    // like several slightly-different physical instruments, not N clones.
    rng = (uint32_t) (reinterpret_cast<std::uintptr_t> (this) >> 4) | 1u;
    voiceSeed = rndf();
    envTol    = 1.0f + (rndf() - 0.5f) * 0.06f;   // +-3% ADSR time tolerance
    for (int i = 0; i < 3; ++i)
        driftTarget[i] = (double) rndf() * 2.0 - 1.0;

    reset();
}

void MonoSynthEngine::reset() noexcept
{
    heldNotes.clear();
    filterEnv.reset();
    ampEnv.reset();
    filter.reset();
    feedbackSample = lastOsc3 = lastNoise = 0.0f;
    samplePhase = 0.0;
}

float MonoSynthEngine::midiNoteToHz (float noteNumber) const noexcept
{
    return 440.0f * std::pow (2.0f, (noteNumber - 69.0f) / 12.0f);
}

void MonoSynthEngine::pushNote (int note, float velocity)
{
    const bool wasSilent = heldNotes.empty();
    heldNotes.push_back (note);

    targetNoteNumber = (float) note;
    velocityGain = 0.2f + 0.8f * velocity;

    if (wasSilent)
    {
        if (! c.glideOn)
            glidingNoteNumber = targetNoteNumber;
        // Single trigger: only (re)trigger the envelopes from silence.
        filterEnv.noteOn();
        ampEnv.noteOn();
    }
    // else: legato — glide to the new note without retriggering.
}

void MonoSynthEngine::releaseNote (int note)
{
    for (int i = (int) heldNotes.size() - 1; i >= 0; --i)
        if (heldNotes[(size_t) i] == note)
            heldNotes.erase (heldNotes.begin() + i);

    if (heldNotes.empty())
    {
        filterEnv.noteOff();
        ampEnv.noteOff();
    }
    else
    {
        targetNoteNumber = (float) heldNotes.back(); // most recent still-held
    }
}

void MonoSynthEngine::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        pushNote (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        releaseNote (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        reset();
    else if (m.isPitchWheel())
        pitchBendSemitones = ((m.getPitchWheelValue() - 8192) / 8192.0f) * c.bendRange;
    else if (m.isController() && m.getControllerNumber() == 1)
        modWheelMidi = m.getControllerValue() / 127.0f;
}

void MonoSynthEngine::updateBlockParameters()
{
    if (params == nullptr)
        return;

    c.masterTune = params->masterTune->load();
    c.glideOn    = params->glideOn->load() > 0.5f;
    c.bendRange  = params->pitchBendRange->load();

    const float glideTime = params->glideTime->load();
    c.glideCoef = (! c.glideOn || glideTime <= 0.0001f)
                    ? 1.0f
                    : (float) (1.0 - std::exp (-5.0 / juce::jmax (1.0, glideTime * sampleRate)));

    c.modWheel    = params->modWheel->load();
    c.modMix      = params->modMix->load();
    c.modOscOn    = params->modOscOn->load() > 0.5f;
    c.modFilterOn = params->modFilterOn->load() > 0.5f;

    c.osc1Wave = (int) params->osc1Wave->load();  c.osc1Range = (int) params->osc1Range->load();
    c.osc2Wave = (int) params->osc2Wave->load();  c.osc2Range = (int) params->osc2Range->load();
    c.osc3Wave = (int) params->osc3Wave->load();  c.osc3Range = (int) params->osc3Range->load();
    c.osc2Detune = params->osc2Detune->load();
    c.osc3Detune = params->osc3Detune->load();
    c.osc3Kb     = params->osc3KbControl->load() > 0.5f;

    c.osc1Vol = params->mixOsc1Vol->load();  c.osc1On = params->mixOsc1On->load() > 0.5f;
    c.osc2Vol = params->mixOsc2Vol->load();  c.osc2On = params->mixOsc2On->load() > 0.5f;
    c.osc3Vol = params->mixOsc3Vol->load();  c.osc3On = params->mixOsc3On->load() > 0.5f;
    c.noiseVol = params->mixNoiseVol->load(); c.noiseOn = params->mixNoiseOn->load() > 0.5f;
    c.extVol = params->mixExtVol->load();     c.extOn = params->mixExtOn->load() > 0.5f;
    c.sampleOn  = params->sampleOn->load() > 0.5f;
    c.sampleVol = params->sampleVol->load();
    c.sampleSel = juce::jlimit (0, 7, (int) params->sampleSel->load());

    c.cutoff  = params->filterCutoff->load();
    c.reso    = params->filterReso->load();
    c.contour = params->filterEnv->load();
    c.keyTrack = ParamChoices::keyTrackAmount ((int) params->filterKeyTrack->load());
    c.drift   = params->driftAmount->load();

    // Per-voice ADSR time tolerance, faded in with the Drift control (sustain
    // levels are untouched — only the RC-like timings vary between units).
    const float et = 1.0f + (envTol - 1.0f) * juce::jmin (1.0f, c.drift * 2.0f);

    filterEnv.setAttackMs  (params->filterAttack->load()  * et);
    filterEnv.setDecayMs   (params->filterDecay->load()   * et);
    filterEnv.setSustain   (params->filterSustain->load());
    filterEnv.setReleaseMs (params->filterRelease->load() * et);

    ampEnv.setAttackMs  (params->ampAttack->load()  * et);
    ampEnv.setDecayMs   (params->ampDecay->load()   * et);
    ampEnv.setSustain   (params->ampSustain->load());
    ampEnv.setReleaseMs (params->ampRelease->load() * et);

    osc1.setWaveform (static_cast<MoogOscillator::Waveform> (c.osc1Wave));
    osc2.setWaveform (static_cast<MoogOscillator::Waveform> (c.osc2Wave));
    osc3.setWaveform (static_cast<MoogOscillator::Waveform> (c.osc3Wave));

    noise.setType (params->noiseType->load() > 0.5f ? NoiseGenerator::Type::pink
                                                     : NoiseGenerator::Type::white);

    filter.setResonance (c.reso);
    filter.setInputDrive (params->filterDrive->load());
    filter.setBassThin (params->bassThin->load());
    filter.setAnalog (c.drift, voiceSeed);   // true-circuit tolerance + asymmetric sat
}

inline float MonoSynthEngine::renderSample()
{
    // ---- Glide -------------------------------------------------------------
    glidingNoteNumber += c.glideCoef * (targetNoteNumber - glidingNoteNumber);

    // ---- Analog drift (slow LFO + stochastic voltage random-walk) ----------
    float drift[3] = { 0.0f, 0.0f, 0.0f };
    if (c.drift > 0.0001f)
    {
        // Refresh the random-walk targets a few dozen times a second so the
        // wander is organic (not a pure, repeating LFO).
        if (--driftCtr <= 0)
        {
            driftCtr = (int) (sampleRate * 0.05);           // ~20 Hz target update
            for (int i = 0; i < 3; ++i)
                driftTarget[i] = (double) rndf() * 2.0 - 1.0;
        }

        const double rates[3] = { 0.11, 0.17, 0.23 };
        const double rwCoef = 8.0 / sampleRate;             // slew toward the target
        for (int i = 0; i < 3; ++i)
        {
            driftPhase[i] += rates[i] / sampleRate;
            if (driftPhase[i] >= 1.0) driftPhase[i] -= 1.0;
            driftRW[i] += (driftTarget[i] - driftRW[i]) * rwCoef;

            const float sine = std::sin (juce::MathConstants<float>::twoPi * (float) driftPhase[i]);
            drift[i] = c.drift * (0.05f * sine + 0.06f * (float) driftRW[i]);
        }
    }

    // ---- Modulation bus (one-sample-delayed to break the algebraic loop) ---
    const float modSrc = (1.0f - c.modMix) * lastOsc3 + c.modMix * lastNoise;
    const float effMod = juce::jmax (c.modWheel, modWheelMidi);
    const float pitchModSemis  = c.modOscOn    ? modSrc * effMod * 7.0f : 0.0f;
    const float cutoffModOctave = c.modFilterOn ? modSrc * effMod * 4.0f : 0.0f;

    const float baseNote = glidingNoteNumber + c.masterTune + pitchBendSemitones
                           + externalBendSemitones + pitchModSemis;

    const float m1 = ParamChoices::rangeMultiplier (c.osc1Range);
    const float m2 = ParamChoices::rangeMultiplier (c.osc2Range);
    const float m3 = ParamChoices::rangeMultiplier (c.osc3Range);

    osc1.setFrequency (midiNoteToHz (baseNote + drift[0]) * m1);
    osc2.setFrequency (midiNoteToHz (baseNote + c.osc2Detune + drift[1]) * m2);

    // Osc 3 follows the keyboard, or free-runs (LFO / drone) when KB control off.
    const float osc3Note = c.osc3Kb ? (baseNote + c.osc3Detune + drift[2])
                                    : (36.0f + c.osc3Detune); // low, note-independent
    osc3.setFrequency (midiNoteToHz (osc3Note) * m3);

    const float s1 = osc1.getNextSample();
    const float s2 = osc2.getNextSample();
    const float s3 = osc3.getNextSample();
    const float n  = noise.getNextSample();
    lastOsc3 = s3;
    lastNoise = n;

    // ---- Mixer (can overdrive the filter) ----------------------------------
    float mix = 0.0f;
    if (c.osc1On)  mix += s1 * c.osc1Vol;
    if (c.osc2On)  mix += s2 * c.osc2Vol;
    if (c.osc3On)  mix += s3 * c.osc3Vol;
    if (c.noiseOn) mix += n * c.noiseVol;
    if (c.extOn)   mix += feedbackSample * c.extVol * 0.7f;

    // ---- Sample / wavetable layer (reads at Osc 1 pitch) -------------------
    if (c.sampleOn)
    {
        const auto& tbl = waveBank()[(size_t) c.sampleSel];
        const double x = samplePhase * kWaveLen;
        const int    i0 = (int) x;
        const float  frac = (float) (x - i0);
        const int    i1 = (i0 + 1) & (kWaveLen - 1);
        mix += (tbl[(size_t) i0] + (tbl[(size_t) i1] - tbl[(size_t) i0]) * frac) * c.sampleVol;

        const double f = (double) midiNoteToHz (baseNote + drift[0]) * m1;
        samplePhase += f / sampleRate;
        while (samplePhase >= 1.0) samplePhase -= 1.0;
    }

    // ---- Filter ------------------------------------------------------------
    const float fEnv = filterEnv.getNextSample();
    const float keyOctaves = c.keyTrack * (glidingNoteNumber - 60.0f) / 12.0f;
    const float envOctaves = c.contour * fEnv * 5.0f;
    float cutoffHz = c.cutoff * std::pow (2.0f, keyOctaves + envOctaves + cutoffModOctave);
    cutoffHz = juce::jlimit (20.0f, (float) (sampleRate * 0.49), cutoffHz);
    filter.setCutoff (cutoffHz);

    const float filtered = filter.processSample (mix);
    feedbackSample = filtered;

    const float aEnv = ampEnv.getNextSample();
    return filtered * aEnv * velocityGain;
}

void MonoSynthEngine::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                       const juce::MidiBuffer& midiMessages,
                                       int startSample, int numSamples)
{
    updateBlockParameters();

    auto midiIt = midiMessages.cbegin();
    const auto midiEnd = midiMessages.cend();
    const int numCh = outputBuffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        while (midiIt != midiEnd && (*midiIt).samplePosition <= i)
        {
            handleMidi ((*midiIt).getMessage());
            ++midiIt;
        }

        const float s = renderSample();
        for (int ch = 0; ch < numCh; ++ch)
            outputBuffer.addSample (ch, startSample + i, s);
    }
}
