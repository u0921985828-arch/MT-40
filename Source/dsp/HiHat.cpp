#include "HiHat.h"

void HiHat::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    // Two Butterworth-ish sections cascade to a 4th-order high-pass @ 8 kHz.
    hp1.setHighPass (fs, 8000.0, 0.5412);
    hp2.setHighPass (fs, 8000.0, 1.3066);
    closedEnv.prepare (fs);
    openEnv.prepare (fs);
    closedEnv.setDecay (0.04); // 40 ms
    openEnv.setDecay (0.30);   // 300 ms
    reset();
}

void HiHat::reset() noexcept
{
    hp1.reset();
    hp2.reset();
    closedEnv.hardReset();
    openEnv.hardReset();
}

void HiHat::triggerClosed() noexcept
{
    // Choke: a closed hat silences any ringing open hat immediately.
    if (openEnv.isActive())
        openEnv.hardReset();
    closedEnv.trigger();
}

void HiHat::triggerOpen() noexcept
{
    openEnv.trigger();
}

bool HiHat::isActive() const noexcept
{
    return closedEnv.isActive() || openEnv.isActive();
}

float HiHat::process() noexcept
{
    if (! isActive()) return 0.0f;

    float n = noise.nextSample();
    n = hp1.process (n);
    n = hp2.process (n);

    const float env = closedEnv.getNextSample() + openEnv.getNextSample();
    return n * env;
}
