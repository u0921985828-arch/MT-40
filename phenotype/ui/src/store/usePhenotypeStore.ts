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
  arpOn: number;
  arpRate: number;
  arpMode: number;
  arpSync: number;
  scaleType: number;
  filterCutoff: number;
  filterReso: number;
  filterType: number;
  filterMod: number;
  drive: number;
  unison: number;
  unisonDetune: number;
  stereoWidth: number;
  delayMix: number;
  delayTime: number;
  delayFb: number;
  reverbMix: number;
  reverbSize: number;
  reverbDamp: number;
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
  arpOn: 0.0,
  arpRate: 0.4,
  arpMode: 0.0,
  arpSync: 0.0,
  scaleType: 0.0,
  filterCutoff: 1.0,
  filterReso: 0.12,
  filterType: 0.0,
  filterMod: 0.0,
  drive: 0.1,
  unison: 0.0,
  unisonDetune: 0.25,
  stereoWidth: 0.5,
  delayMix: 0.0,
  delayTime: 0.35,
  delayFb: 0.35,
  reverbMix: 0.0,
  reverbSize: 0.5,
  reverbDamp: 0.4,
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
  /** Param the user is currently dragging; remote updates skip it. */
  dragging: ParamId | null;
  setParam: (id: ParamId, value: number) => void;
  setDragging: (id: ParamId | null) => void;
  /** Apply a backend-originated snapshot (host automation) without echoing. */
  applyRemoteParams: (incoming: Partial<ParamState>) => void;
  reset: () => void;
}

const clamp01 = (v: number) => (v < 0 ? 0 : v > 1 ? 1 : v);

export const usePhenotypeStore = create<StoreState>((set, get) => ({
  params: { ...DEFAULT_PARAMS },
  hosted: juceIntegration.hosted,
  activeGrains: 0,
  dragging: null,

  setParam: (id, value) => {
    const clamped = clamp01(value);
    set((s) => ({ params: { ...s.params, [id]: clamped } }));
    juceIntegration.setParam(id, clamped);
  },

  setDragging: (id) => set({ dragging: id }),

  applyRemoteParams: (incoming) => {
    const { dragging, params } = get();
    const next = { ...params };
    let changed = false;
    (Object.keys(incoming) as ParamId[]).forEach((id) => {
      const v = incoming[id];
      if (v === undefined || id === dragging) return;
      const c = clamp01(v);
      if (next[id] !== c) {
        next[id] = c;
        changed = true;
      }
    });
    if (changed) set({ params: next });
  },

  reset: () => {
    set({ params: { ...DEFAULT_PARAMS } });
    juceIntegration.setParams({ ...DEFAULT_PARAMS });
  },
}));

// Mirror host/preset parameter changes into the reactive store.
juceIntegration.onParams((frame) => {
  usePhenotypeStore.getState().applyRemoteParams(frame.params as Partial<ParamState>);
});

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
