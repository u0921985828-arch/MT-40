#include "SnareDrum.h"
#include <algorithm>

void SnareDrum::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    bodyBpf.setBandPass (fs, 220.0, 6.0);
    noiseHpf.setHighPass (fs, 2500.0, 0.707);
    bodyEnv.prepare (fs);
    noiseEnv.prepare (fs);
    bodyEnv.setDecay (0.09);   // short tonal body
    noiseEnv.setDecay (0.15);  // ~150 ms wire tail
    reset();
}

void SnareDrum::reset() noexcept
{
    bodyBpf.reset();
    noiseHpf.reset();
    bodyEnv.hardReset();
    noiseEnv.hardReset();
    impulseSamplesLeft = 0;
}

void SnareDrum::trigger() noexcept
{
    bodyBpf.reset();
    impulseSamplesLeft = std::max (1, static_cast<int> (0.001 * fs));
    bodyEnv.trigger();
    noiseEnv.trigger();
}

bool SnareDrum::isActive() const noexcept
{
    return bodyEnv.isActive() || noiseEnv.isActive();
}

float SnareDrum::process() noexcept
{
    if (! isActive() && impulseSamplesLeft == 0)
        return 0.0f;

    float x = 0.0f;
    if (impulseSamplesLeft > 0) { x = 1.0f; --impulseSamplesLeft; }

    // Tonal body.
    const float body = bodyBpf.process (x) * bodyEnv.getNextSample() * kTonalAmp;

    // Snare wires.
    const float n = noiseHpf.process (noise.nextSample());
    const float wires = n * noiseEnv.getNextSample() * kNoiseAmp;

    return body + wires;
}
