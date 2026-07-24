#pragma once

#include "Biquad.h"
#include "ExpDecay.h"
#include "LFSRNoise.h"

// ---------------------------------------------------------------------------
// HiHat — closed/open hats with choke-group state machine (§4.3).
//
//   * Source : shared global LFSR white noise.
//   * Filter : 4th-order high-pass (two cascaded biquads, fc = 8 kHz).
//   * VCAs   : closed = exp decay 40 ms, open = exp decay 300 ms.
//   * Choke  : if the open-hat envelope is still ringing and a closed-hat
//              trigger arrives, the open envelope is hard-reset to 0.
// ---------------------------------------------------------------------------
class HiHat
{
public:
    explicit HiHat (LFSRNoise& sharedNoise) : noise (sharedNoise) {}

    void prepare (double sampleRate) noexcept;
    void triggerClosed() noexcept;
    void triggerOpen() noexcept;
    void reset() noexcept;
    bool isActive() const noexcept;
    float process() noexcept;

private:
    LFSRNoise& noise;
    Biquad hp1, hp2;          // 4th-order HPF = two cascaded 2nd-order sections
    ExpDecay closedEnv;
    ExpDecay openEnv;
    double fs = 44100.0;
};
