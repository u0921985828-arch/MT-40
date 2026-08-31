#!/usr/bin/env python3
"""Synthesise a curated palette of HQ genome sources for the factory presets.
Each is a short mono WAV (looped by the engine into the granular genome). The
palette is embedded in the plugin (CMake juce_add_binary_data) and assigned to
presets by type, so the 876 factory presets play real HQ timbres, not the plain
built-in wavetable. No third-party deps.

    python3 tools/make_genome_palette.py            # -> genomes/*.wav

Roles (for preset assignment): pad, key, lead, pluck, bass, arp."""

import math, os, struct, wave, sys

SR = 22050
DUR = 1.6
TAU = 2 * math.pi

def wv(name, fn, out_dir):
    n = int(SR * DUR)
    s = [fn(i / SR) for i in range(n)]
    peak = max(1e-6, max(abs(x) for x in s))
    g = 0.89 / peak
    with wave.open(os.path.join(out_dir, name + ".wav"), "w") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, x * g)) * 32767)) for x in s))

def saw(f, t, h=20):  return sum((1.0/k)*math.sin(TAU*f*k*t) for k in range(1, h))
def squ(f, t, h=20):  return sum((1.0/k)*math.sin(TAU*f*k*t) for k in range(1, h, 2))
def tri(f, t, h=15):  return sum(((-1)**((k-1)//2))/(k*k)*math.sin(TAU*f*k*t) for k in range(1, h, 2))

# ---- role-tagged generators -------------------------------------------------
def gen(role_map):
    F = 110.0
    def A(t, k=1.0): return math.exp(-k * (t % 1.0))
    palette = {
        # pads / keys — rich, slow
        "warm_saw_ens": ("pad",  lambda t: saw(F,t)+0.7*saw(F*1.007,t)+0.7*saw(F*0.993,t)),
        "choir_aah":    ("pad",  lambda t: sum(math.sin(TAU*F*h*t)*g for h,g in [(1,1),(2,.5),(3,.35),(4,.2),(5,.15)]) * (0.7+0.3*math.sin(TAU*5*t))),
        "glass_pad":    ("pad",  lambda t: math.sin(TAU*F*t)+0.5*math.sin(TAU*F*3.01*t)+0.3*math.sin(TAU*F*5.02*t)),
        "airy_pwm":     ("pad",  lambda t: squ(F,t) * (0.6+0.4*math.sin(TAU*0.3*t)) + 0.2*squ(F*2.003,t)),
        "organ_add":    ("pad",  lambda t: sum(math.sin(TAU*F*h*t)*g for h,g in [(1,1),(2,.8),(4,.6),(6,.4),(8,.25)])),
        # keys
        "glass_fm":     ("key",  lambda t: A(t,2)*math.sin(TAU*F*2*t + 3*A(t,2)*math.sin(TAU*F*2.01*2*t))),
        "e_piano":      ("key",  lambda t: A(t,3)*(math.sin(TAU*F*t) + 0.5*A(t,6)*math.sin(TAU*F*2*t)) + 0.2*A(t,9)*math.sin(TAU*F*14*t)),
        "bell_soft":    ("key",  lambda t: A(t,2.5)*sum(math.sin(TAU*F*r*t)*g for r,g in [(1,1),(2.76,.5),(5.4,.25)])),
        # leads
        "super_saw":    ("lead", lambda t: sum(saw(F*d,t) for d in (0.988,0.995,1.0,1.005,1.012))/5),
        "square_lead":  ("lead", lambda t: squ(F,t)*(0.8+0.2*math.sin(TAU*6*t))),
        "fm_horn":      ("lead", lambda t: math.sin(TAU*F*t + 2.2*math.sin(TAU*F*t))),
        "bright_saw":   ("lead", lambda t: saw(F,t,32)),
        # plucks
        "mallet":       ("pluck",lambda t: A(t,6)*(math.sin(TAU*F*t)+0.6*math.sin(TAU*F*4.1*t)+0.3*math.sin(TAU*F*9.2*t))),
        "karplus":      ("pluck",lambda t: A(t,4)*sum(math.sin(TAU*F*k*t)*(1.0/k) for k in range(1,25))),
        "fm_pluck":     ("pluck",lambda t: A(t,5)*math.sin(TAU*F*t + 4*A(t,10)*math.sin(TAU*F*3*t))),
        "kalimba":      ("pluck",lambda t: A(t,7)*(math.sin(TAU*F*t)+0.4*math.sin(TAU*F*6.3*t))),
        # bass
        "sub_sine":     ("bass", lambda t: math.sin(TAU*F*0.5*t) + 0.15*math.sin(TAU*F*t)),
        "saw_sub":      ("bass", lambda t: 0.7*saw(F*0.5,t,12) + 0.5*math.sin(TAU*F*0.5*t)),
        "reese":        ("bass", lambda t: saw(F*0.5,t,16)+saw(F*0.5*1.01,t,16)+saw(F*0.5*0.99,t,16)),
        "fm_bass":      ("bass", lambda t: math.sin(TAU*F*0.5*t + 1.5*math.sin(TAU*F*0.5*t)) ),
        # arps / blips
        "pluck_short":  ("arp",  lambda t: A(t,10)*saw(F,t,20)),
        "blip":         ("arp",  lambda t: A(t,14)*squ(F*2,t,12)),
        "digi_pluck":   ("arp",  lambda t: A(t,9)*(math.sin(TAU*F*t)+0.5*math.sin(TAU*F*2*t)+0.25*math.sin(TAU*F*4*t))),
        "mallet_hi":    ("arp",  lambda t: A(t,8)*(math.sin(TAU*F*2*t)+0.5*math.sin(TAU*F*5*t))),
    }
    for name, (role, fn) in palette.items():
        role_map.setdefault(role, []).append(name)
    return palette

def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "genomes")
    out_dir = os.path.abspath(out_dir); os.makedirs(out_dir, exist_ok=True)
    roles = {}
    palette = gen(roles)
    for name, (_, fn) in palette.items():
        wv(name, fn, out_dir)
    # emit the role map so the assignment script stays in sync
    import json
    with open(os.path.join(out_dir, "_roles.json"), "w") as f:
        json.dump(roles, f, indent=1)
    print("wrote", len(palette), "genomes to", out_dir)
    for r, names in roles.items():
        print(f"  {r:<6} {len(names)}: {', '.join(names)}")

if __name__ == "__main__":
    main()
