//==============================================================================
//  usePhenotypeStore.ts
//
//  Zustand global state. Two tiers, on purpose:
//
//   * Reactive tier (params, connection, coarse HUD stats) — drives React.
//   * Transient tier (`telemetry`) — a mutable buffer updated ~30–60 Hz from
//     the JUCE telemetry stream and read inside r3f useFrame loops. Kept OUT of
//     React state so per-frame FFT frames never trigger a component re-render.
//==============================================================================

import { create } from "zustand";
import { juceIntegration } from "../bridge/juceIntegration";

export interface ParamState {
  caudal: number;
  soilDensity: number;
  saturation: number;
  grainDensity: number;
  grainSize: number;
  position: number;
  spray: number;
  pitchA: number;
  pitchB: number;
  crossBlend: number;
  modDepth: number;
  outputGain: number;
}

export type ParamId = keyof ParamState;

const DEFAULT_PARAMS: ParamState = {
  caudal: 0.5,
  soilDensity: 0.5,
  saturation: 0.9,
  grainDensity: 0.4,
  grainSize: 0.3,
  position: 0.5,
  spray: 0.2,
  pitchA: 0.5,
  pitchB: 0.5,
  crossBlend: 0.5,
  modDepth: 0.5,
  outputGain: 0.8,
};

// --- Transient telemetry buffer (mutable, non-reactive) ---------------------
export interface TelemetryBuffer {
  fft: Float32Array;
  capillary: number;
  activeGrains: number;
  /** monotonically increasing frame id, lets consumers detect fresh data */
  frame: number;
}

export const telemetry: TelemetryBuffer = {
  fft: new Float32Array(1024),
  capillary: 0,
  activeGrains: 0,
  frame: 0,
};

interface StoreState {
  params: ParamState;
  hosted: boolean;
  activeGrains: number; // coarse HUD mirror, throttled
  setParam: (id: ParamId, value: number) => void;
  reset: () => void;
}

export const usePhenotypeStore = create<StoreState>((set, get) => ({
  params: { ...DEFAULT_PARAMS },
  hosted: juceIntegration.hosted,
  activeGrains: 0,

  setParam: (id, value) => {
    const clamped = value < 0 ? 0 : value > 1 ? 1 : value;
    set((s) => ({ params: { ...s.params, [id]: clamped } }));
    juceIntegration.setParam(id, clamped);
  },

  reset: () => {
    set({ params: { ...DEFAULT_PARAMS } });
    juceIntegration.setParams({ ...DEFAULT_PARAMS });
    void get();
  },
}));

// --- Wire the telemetry stream into the transient buffer --------------------
let hudThrottle = 0;
juceIntegration.onTelemetry((f) => {
  if (f.fft.length !== telemetry.fft.length) {
    telemetry.fft = new Float32Array(f.fft.length);
  }
  telemetry.fft.set(f.fft);
  telemetry.capillary = f.capillary;
  telemetry.activeGrains = f.activeGrains;
  telemetry.frame++;

  // Mirror grain count to the reactive HUD at ~4 Hz to avoid render churn.
  if (++hudThrottle >= 8) {
    hudThrottle = 0;
    usePhenotypeStore.setState({ activeGrains: f.activeGrains });
  }
});
