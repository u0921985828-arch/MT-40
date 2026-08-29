//==============================================================================
//  format.ts — normalised (0..1) parameter value -> human, unit-bearing text.
//
//  Mirrors the DSP mappings in GranularEngine so the UI reads like the
//  instrument sounds: Hz, dB, cents, voices, note divisions — never raw 0..1.
//==============================================================================

import type { ParamId } from "./store/usePhenotypeStore";

const ARP_MODES = ["Up", "Down", "Up-Down", "Random"];
const SCALES = ["Cromática", "Mayor", "Menor", "Pentatónica", "Dórica"];
const FILTERS = ["LP", "BP", "HP"];

const pct = (v: number) => `${Math.round(v * 100)}%`;
const step = (v: number, list: string[]) =>
  list[Math.min(list.length - 1, Math.floor(v * (list.length - 0.0001)))]!;

function hz(v: number): string {
  return v >= 1000 ? `${(v / 1000).toFixed(2)} kHz` : `${Math.round(v)} Hz`;
}

/** Returns { value, unit } for a parameter id at normalised value v. */
export function formatParam(id: ParamId, v: number): { value: string; unit: string } {
  switch (id) {
    case "grainDensity":
      return { value: `${Math.round(2 + v * 198)}`, unit: "gr/s" };
    case "grainSize":
      return { value: `${Math.round(8 + v * 392)}`, unit: "ms" };
    case "pitchA":
    case "pitchB": {
      const s = (v - 0.5) * 24;
      return { value: `${s >= 0 ? "+" : ""}${s.toFixed(1)}`, unit: "st" };
    }
    case "outputGain": {
      const db = v <= 0.0001 ? -60 : 20 * Math.log10(v);
      return { value: `${db > -0.05 ? "0.0" : db.toFixed(1)}`, unit: "dB" };
    }
    case "filterCutoff":
      return { value: hz(20 * Math.exp(v * 6.9077)).split(" ")[0]!, unit: 20 * Math.exp(v * 6.9077) >= 1000 ? "kHz" : "Hz" };
    case "filterReso":
      return { value: (0.5 + v * 9.5).toFixed(1), unit: "Q" };
    case "filterType":
      return { value: step(v, FILTERS), unit: "" };
    case "unison":
      return { value: `${1 + Math.round(v * 6)}`, unit: "voces" };
    case "unisonDetune":
      return { value: `±${Math.round(v * 50)}`, unit: "cent" };
    case "stereoWidth":
      return { value: `${Math.round(v * 200)}`, unit: "%" };
    case "arpOn":
    case "arpSync":
      return { value: v > 0.5 ? "On" : "Off", unit: "" };
    case "arpMode":
      return { value: step(v, ARP_MODES), unit: "" };
    case "arpRate":
      return { value: (0.5 + v * 19.5).toFixed(1), unit: "Hz" };
    case "scaleType":
      return { value: step(v, SCALES), unit: "" };
    default:
      return { value: pct(v), unit: "" };
  }
}
