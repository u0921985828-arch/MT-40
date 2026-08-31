#!/usr/bin/env python3
"""Synthesise a large palette of HQ genome sources for the 10k DLC pack.

~256 distinct mono WAVs at 44.1 kHz / 2 s, spanning many timbral families
(analogue, additive, FM, wavetable, formant, metallic, physical, noise/air,
sub/reese/growl bass, evolving textures). Each family produces several seeded
variants so no two genomes are alike. Every buffer is loop-crossfaded so the
granular engine can loop it with no click ("no bucle malo"), peak-normalised,
and written 16-bit.

    python3 tools/make_dlc_genomes.py            # -> dlc/samples/*.wav + manifest

Requires numpy (dev-time tool only; nothing here ships in the plugin)."""

import json, math, os, struct, sys, wave
import numpy as np

SR = 44100
DUR = 2.0
XF = 1536                      # loop crossfade length (~35 ms)
N = int(SR * DUR)
TAU = 2 * math.pi

# ---------------------------------------------------------------------------
#  Primitives (vectorised)
# ---------------------------------------------------------------------------
def _t(extra=XF):
    return np.arange(N + extra) / SR

def nyq_harmonics(f, kind="saw"):
    kmax = int((SR * 0.45) / max(1.0, f))
    kmax = max(1, min(kmax, 64))
    if kind == "saw":
        return [(k, 1.0 / k) for k in range(1, kmax + 1)]
    if kind == "square":
        return [(k, 1.0 / k) for k in range(1, kmax + 1, 2)]
    if kind == "tri":
        return [(k, ((-1) ** ((k - 1) // 2)) / (k * k)) for k in range(1, kmax + 1, 2)]
    return [(1, 1.0)]

def additive(f, t, kind="saw", detune=0.0):
    out = np.zeros_like(t)
    fr = f * (1.0 + detune)
    for k, a in nyq_harmonics(fr, kind):
        out += a * np.sin(TAU * fr * k * t)
    return out

def ensemble(f, t, kind="saw", voices=3, spread=0.006):
    out = np.zeros_like(t)
    for i in range(voices):
        d = (i - (voices - 1) / 2) / max(1, (voices - 1) / 2) * spread if voices > 1 else 0.0
        out += additive(f, t, kind, d)
    return out / voices

def fm(fc, ratio, index, t, ienv=None):
    m = np.sin(TAU * fc * ratio * t)
    if ienv is not None:
        m = m * ienv
    return np.sin(TAU * fc * t + index * m)

def lfo(t, rate, depth=0.4, base=0.6):
    # A pure sine LFO is continuous across the loop boundary (no click). Genome
    # buffers are LOOPED by the engine, so no baked attack/decay envelope may be
    # used — the engine's grain window + capillary modulator shape the dynamics.
    # The genome supplies the *timbre* (spectrum); amplitude stays sustained.
    return base + depth * np.sin(TAU * rate * t)

def colored_noise(t, lp=0.15, seed=0):
    rng = np.random.default_rng(seed)
    x = rng.standard_normal(len(t))
    ksize = max(3, int(lp * 400))
    k = np.hanning(ksize)
    k /= k.sum()
    return np.convolve(x, k, mode="same")

def formant_bank(f, t, formants, bw=0.06):
    # additive vowel: weight harmonics by proximity to formant centres.
    out = np.zeros_like(t)
    for k, a in nyq_harmonics(f, "saw"):
        hz = f * k
        w = 0.0
        for fc, g in formants:
            w += g * math.exp(-((hz - fc) ** 2) / (2 * (bw * fc + 40) ** 2))
        out += a * w * np.sin(TAU * f * k * t)
    return out

def inharmonic(f, t, ratios, env):
    out = np.zeros_like(t)
    for r, g in ratios:
        out += g * np.sin(TAU * f * r * t)
    return out * env

# ---------------------------------------------------------------------------
#  Loop-seamless crossfade + write
# ---------------------------------------------------------------------------
def seamless(sig):
    # sig has length N+XF. Place the tail at the head and cross-fade so the
    # loop point (n-1 -> 0) is continuous and the head content resumes cleanly.
    body = sig[:N].copy()
    w = 0.5 * (1 - np.cos(np.linspace(0, math.pi, XF)))   # raised cosine 0..1
    body[:XF] = sig[N:N + XF] * (1 - w) + sig[:XF] * w
    return body

def write_wav(path, sig):
    sig = seamless(np.asarray(sig, dtype=np.float64))
    peak = max(1e-9, float(np.max(np.abs(sig))))
    sig = (sig * (0.9 / peak)).clip(-1, 1)
    pcm = (sig * 32767.0).astype("<i2").tobytes()
    with wave.open(path, "w") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(pcm)

# ---------------------------------------------------------------------------
#  Families — each returns a signal of length N+XF for a given seed.
# ---------------------------------------------------------------------------
def build_families():
    fams = {}

    def reg(name, role):
        def deco(fn):
            fams[name] = (role, fn)
            return fn
        return deco

    @reg("saw_ens", "pad")
    def f(rng):
        t = _t(); F = rng.uniform(90, 130)
        v = rng.integers(3, 8); sp = rng.uniform(0.004, 0.014)
        s = ensemble(F, t, "saw", int(v), sp)
        return s * lfo(t, rng.uniform(0.2, 0.6), 0.25, 0.75)

    @reg("pwm_pad", "pad")
    def f(rng):
        t = _t(); F = rng.uniform(90, 130)
        pw = lfo(t, rng.uniform(0.15, 0.5), rng.uniform(0.2, 0.45), 0.5)
        s = additive(F, t, "square") * pw + 0.3 * additive(F * 2.001, t, "square")
        return s

    @reg("glass_pad", "pad")
    def f(rng):
        t = _t(); F = rng.uniform(100, 150)
        r = [(1, 1.0), (rng.uniform(2.9, 3.1), 0.5), (rng.uniform(4.8, 5.2), 0.3), (rng.uniform(6.8, 7.3), 0.18)]
        return inharmonic(F, t, r, lfo(t, rng.uniform(0.2, 0.5), 0.3, 0.7))

    @reg("organ_pad", "pad")
    def f(rng):
        t = _t(); F = rng.uniform(90, 130)
        draws = [(1, 1.0), (2, rng.uniform(.5, .9)), (3, rng.uniform(.3, .7)),
                 (4, rng.uniform(.2, .6)), (6, rng.uniform(.1, .4)), (8, rng.uniform(.1, .3))]
        return sum(g * np.sin(TAU * F * h * t) for h, g in draws)

    @reg("choir_vox", "pad")
    def f(rng):
        t = _t(); F = rng.uniform(100, 140)
        vowel = rng.choice(["a", "e", "o"])
        F1 = {"a": 730, "e": 530, "o": 570}[vowel]; F2 = {"a": 1090, "e": 1840, "o": 840}[vowel]
        s = formant_bank(F, t, [(F1, 1.0), (F2, 0.7), (2600, 0.25)])
        return s * lfo(t, rng.uniform(4.5, 6.0), 0.08, 0.92)

    @reg("string_ens", "pad")
    def f(rng):
        t = _t(); F = rng.uniform(90, 130)
        s = ensemble(F, t, "saw", 7, rng.uniform(0.006, 0.012))
        return s * lfo(t, rng.uniform(0.15, 0.4), 0.15, 0.85)

    @reg("brass_swell", "pad")
    def f(rng):
        t = _t(); F = rng.uniform(90, 130)
        bright = lfo(t, rng.uniform(0.3, 0.7), 0.5, 0.5)
        s = additive(F, t, "saw") * (0.4 + 0.6 * bright)
        return s

    @reg("ep_electric", "key")
    def f(rng):
        t = _t(); F = rng.uniform(180, 240)
        s = np.sin(TAU * F * t) + 0.5 * np.sin(TAU * F * 2 * t) + 0.22 * np.sin(TAU * F * 14 * t)
        return s * lfo(t, rng.uniform(0.2, 0.5), 0.15, 0.85)

    @reg("fm_key", "key")
    def f(rng):
        t = _t(); F = rng.uniform(110, 180)
        ratio = rng.choice([1.0, 2.0, 3.0, 1.5]); idx = rng.uniform(1.5, 4.0)
        return fm(F, ratio, idx, t) * lfo(t, rng.uniform(0.2, 0.5), 0.1, 0.9)

    @reg("bell_soft", "key")
    def f(rng):
        t = _t(); F = rng.uniform(180, 260)
        r = [(1, 1.0), (rng.uniform(2.7, 2.85), 0.5), (rng.uniform(5.2, 5.5), 0.25), (rng.uniform(8.1, 8.6), 0.12)]
        return inharmonic(F, t, r, lfo(t, rng.uniform(0.2, 0.5), 0.15, 0.85))

    @reg("clav_wt", "key")
    def f(rng):
        t = _t(); F = rng.uniform(150, 220)
        return additive(F, t, "square") * 0.9 + 0.2 * additive(F * 2, t, "saw")

    @reg("super_saw", "lead")
    def f(rng):
        t = _t(); F = rng.uniform(160, 230)
        return ensemble(F, t, "saw", 7, rng.uniform(0.008, 0.02))

    @reg("square_lead", "lead")
    def f(rng):
        t = _t(); F = rng.uniform(160, 240)
        return additive(F, t, "square") * lfo(t, rng.uniform(5, 7), 0.15, 0.85)

    @reg("fm_horn", "lead")
    def f(rng):
        t = _t(); F = rng.uniform(140, 210)
        return fm(F, rng.choice([1.0, 2.0]), rng.uniform(1.8, 3.2), t)

    @reg("sync_lead", "lead")
    def f(rng):
        t = _t(); F = rng.uniform(150, 220)
        sync = 1.0 + lfo(t, rng.uniform(0.5, 1.5), 0.5, 1.0)
        return np.sin(TAU * F * sync * t) * additive(F, t, "saw") * 0.5

    @reg("bright_saw", "lead")
    def f(rng):
        t = _t(); F = rng.uniform(150, 230)
        return additive(F, t, "saw")

    @reg("mallet", "pluck")
    def f(rng):
        t = _t(); F = rng.uniform(200, 320)
        return np.sin(TAU * F * t) + 0.6 * np.sin(TAU * F * 4.1 * t) + 0.3 * np.sin(TAU * F * 9.2 * t)

    @reg("karplus", "pluck")
    def f(rng):
        t = _t(); F = rng.uniform(150, 260)
        return additive(F, t, "saw") * lfo(t, rng.uniform(0.3, 0.7), 0.1, 0.9)

    @reg("fm_pluck", "pluck")
    def f(rng):
        t = _t(); F = rng.uniform(180, 300)
        return fm(F, rng.choice([2.0, 3.0]), rng.uniform(3, 6), t)

    @reg("kalimba", "pluck")
    def f(rng):
        t = _t(); F = rng.uniform(240, 360)
        return np.sin(TAU * F * t) + 0.4 * np.sin(TAU * F * 6.3 * t)

    @reg("digi_pluck", "pluck")
    def f(rng):
        t = _t(); F = rng.uniform(200, 320)
        return np.sin(TAU*F*t) + 0.5*np.sin(TAU*F*2*t) + 0.25*np.sin(TAU*F*4*t)

    @reg("sub_sine", "bass")
    def f(rng):
        t = _t(); F = rng.uniform(55, 75)
        return np.sin(TAU * F * t) + 0.15 * np.sin(TAU * F * 2 * t)

    @reg("saw_sub", "bass")
    def f(rng):
        t = _t(); F = rng.uniform(55, 75)
        return 0.7 * additive(F, t, "saw") + 0.5 * np.sin(TAU * F * t)

    @reg("reese", "bass")
    def f(rng):
        t = _t(); F = rng.uniform(55, 78)
        return ensemble(F, t, "saw", 3, rng.uniform(0.01, 0.02))

    @reg("fm_bass", "bass")
    def f(rng):
        t = _t(); F = rng.uniform(55, 80)
        return fm(F, 1.0, rng.uniform(1.2, 2.4), t)

    @reg("growl_bass", "bass")
    def f(rng):
        t = _t(); F = rng.uniform(58, 82)
        g = lfo(t, rng.uniform(6, 12), 0.9, 1.0)
        return additive(F, t, "saw") * g

    @reg("pluck_short", "arp")
    def f(rng):
        t = _t(); F = rng.uniform(220, 340)
        return additive(F, t, "saw")

    @reg("blip", "arp")
    def f(rng):
        t = _t(); F = rng.uniform(260, 400)
        return additive(F * 2, t, "square")

    @reg("digi_arp", "arp")
    def f(rng):
        t = _t(); F = rng.uniform(240, 380)
        return np.sin(TAU*F*t) + 0.5*np.sin(TAU*F*2*t) + 0.25*np.sin(TAU*F*3*t)

    @reg("mallet_hi", "arp")
    def f(rng):
        t = _t(); F = rng.uniform(300, 460)
        return np.sin(TAU*F*2*t) + 0.5*np.sin(TAU*F*5*t)

    @reg("noise_bed", "texture")
    def f(rng):
        t = _t(); base = additive(rng.uniform(90, 130), t, "saw") * 0.5
        return base + 0.5 * colored_noise(t, rng.uniform(0.1, 0.3), int(rng.integers(0, 1e6)))

    @reg("granular_evolve", "texture")
    def f(rng):
        t = _t(); F = rng.uniform(90, 140)
        m = lfo(t, rng.uniform(0.1, 0.3), 0.5, 0.5)
        return additive(F, t, "saw") * m + additive(F * 1.5, t, "tri") * (1 - m)

    @reg("drone_inharm", "texture")
    def f(rng):
        t = _t(); F = rng.uniform(70, 110)
        r = [(1, 1.0), (rng.uniform(1.4, 1.6), 0.6), (rng.uniform(2.3, 2.7), 0.4), (rng.uniform(3.5, 4.1), 0.25)]
        return inharmonic(F, t, r, lfo(t, rng.uniform(0.1, 0.3), 0.3, 0.7))

    @reg("air_shimmer", "texture")
    def f(rng):
        t = _t(); F = rng.uniform(140, 200)
        return (np.sin(TAU*F*t) + 0.5*np.sin(TAU*F*4*t) + 0.3*np.sin(TAU*F*8*t)) * lfo(t, rng.uniform(3, 6), 0.2, 0.6) \
            + 0.2 * colored_noise(t, 0.05, int(rng.integers(0, 1e6)))

    @reg("noise_sweep", "fx")
    def f(rng):
        t = _t(); swell = lfo(t, rng.uniform(0.2, 0.5), 0.4, 0.6)
        return colored_noise(t, rng.uniform(0.05, 0.12), int(rng.integers(0, 1e6))) * swell

    @reg("metallic_hit", "fx")
    def f(rng):
        t = _t(); F = rng.uniform(160, 260)
        r = [(rng.uniform(1, 1.2), 1.0), (rng.uniform(2.4, 3.1), 0.7), (rng.uniform(4.2, 5.6), 0.5), (rng.uniform(7, 9), 0.3)]
        return inharmonic(F, t, r, lfo(t, rng.uniform(0.2, 0.5), 0.2, 0.8))

    @reg("formant_sweep", "fx")
    def f(rng):
        t = _t(); F = rng.uniform(110, 160)
        sweep = 400 + 1600 * (0.5 + 0.5 * np.sin(TAU * rng.uniform(0.2, 0.5) * t))
        out = np.zeros_like(t)
        for k, a in nyq_harmonics(F, "saw"):
            hz = F * k
            w = np.exp(-((hz - sweep) ** 2) / (2 * (0.15 * hz + 60) ** 2))
            out += a * w * np.sin(TAU * F * k * t)
        return out

    return fams


# variants per family so the total lands near the target count.
VARIANTS = {
    "saw_ens": 16, "pwm_pad": 12, "glass_pad": 12, "organ_pad": 10, "choir_vox": 10,
    "string_ens": 10, "brass_swell": 8, "ep_electric": 12, "fm_key": 14, "bell_soft": 12,
    "clav_wt": 8, "super_saw": 12, "square_lead": 10, "fm_horn": 10, "sync_lead": 8,
    "bright_saw": 8, "mallet": 10, "karplus": 10, "fm_pluck": 12, "kalimba": 8,
    "digi_pluck": 8, "sub_sine": 8, "saw_sub": 8, "reese": 8, "fm_bass": 10,
    "growl_bass": 8, "pluck_short": 8, "blip": 6, "digi_arp": 8, "mallet_hi": 6,
    "noise_bed": 8, "granular_evolve": 8, "drone_inharm": 8, "air_shimmer": 8,
    "noise_sweep": 4, "metallic_hit": 6, "formant_sweep": 6,
}


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "dlc")
    out_dir = os.path.abspath(out_dir)
    samp_dir = os.path.join(out_dir, "samples")
    os.makedirs(samp_dir, exist_ok=True)

    fams = build_families()
    manifest = {}
    total = 0
    for fam, (role, fn) in fams.items():
        nv = VARIANTS.get(fam, 6)
        for v in range(nv):
            rng = np.random.default_rng(hash((fam, v)) & 0xFFFFFFFF)
            name = f"{fam}_{v:02d}"
            write_wav(os.path.join(samp_dir, name + ".wav"), fn(rng))
            manifest[name] = {"family": fam, "role": role}
            total += 1
    with open(os.path.join(out_dir, "_genomes.json"), "w") as f:
        json.dump(manifest, f, indent=0)
    print(f"wrote {total} genomes -> {samp_dir}")
    # roles summary
    roles = {}
    for m in manifest.values():
        roles[m["role"]] = roles.get(m["role"], 0) + 1
    print("by role:", roles)


if __name__ == "__main__":
    main()
