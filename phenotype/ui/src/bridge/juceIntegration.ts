//==============================================================================
//  juceIntegration.ts
//
//  The single seam between the WebGL frontend and the JUCE/C++ backend.
//  Publishes `window.juceIntegration` (per Phenotype spec §4). Outbound
//  parameter edits go through the registered native function `phenotypeSend`;
//  inbound telemetry (FFT, capillary phase, grain count) arrives on the
//  `phenotypeTelemetry` event. When not hosted inside JUCE (plain browser dev)
//  a self-animating mock keeps the visualiser alive.
//==============================================================================

import { getNativeFunction } from "../juce/index.js";

export interface TelemetryFrame {
  type: "telemetry";
  fft: number[];
  capillary: number;
  activeGrains: number;
}

export interface ParamsFrame {
  type: "params";
  params: Record<string, number>;
}

export type TelemetryListener = (frame: TelemetryFrame) => void;
export type ParamsListener = (frame: ParamsFrame) => void;

export interface ProgramInfo {
  index: number;
  name: string;
  count: number;
}
export type ProgramAction = "next" | "prev" | "set" | "get";

export interface LibraryInfo {
  count: number;
  imported: number;
}
export type LibraryAction = "count" | "rescan" | "import";

export interface JuceIntegration {
  readonly hosted: boolean;
  /** Push a single parameter edit to the backend (async, non-blocking). */
  setParam(id: string, value: number): void;
  /** Push several parameter edits in one IPC frame. */
  setParams(params: Record<string, number>): void;
  /** Subscribe to telemetry frames. Returns an unsubscribe function. */
  onTelemetry(listener: TelemetryListener): () => void;
  /** Subscribe to backend-originated parameter snapshots (host automation). */
  onParams(listener: ParamsListener): () => void;
  /** Change / query the current factory program (preset). */
  program(action: ProgramAction, index?: number): Promise<ProgramInfo>;
  /** Import a DLC bank / rescan / count the preset library. */
  library(action: LibraryAction): Promise<LibraryInfo>;
}

const NATIVE_PROGRAM = "phenotypeProgram";
const NATIVE_LIBRARY = "phenotypeLibrary";

function normaliseProgram(r: unknown): ProgramInfo {
  const o = (typeof r === "object" && r !== null ? r : {}) as Partial<ProgramInfo>;
  return {
    index: typeof o.index === "number" ? o.index : 0,
    name: typeof o.name === "string" ? o.name : "—",
    count: typeof o.count === "number" ? o.count : 0,
  };
}

//  --- JUCE global surface (injected by the WebView backend) ------------------
interface JuceBackend {
  addEventListener(event: string, fn: (payload: unknown) => void): unknown;
  removeEventListener?(handle: unknown): void;
}
interface JuceInitData {
  __juce__functions?: string[];
}
interface JuceGlobal {
  backend: JuceBackend;
  initialisationData?: JuceInitData;
}

declare global {
  interface Window {
    __JUCE__?: JuceGlobal;
    juceIntegration: JuceIntegration;
  }
}

const TELEMETRY_EVENT = "phenotypeTelemetry";
const PARAMS_EVENT = "phenotypeParams";
const NATIVE_SEND = "phenotypeSend";

function createHostedIntegration(juce: JuceGlobal): JuceIntegration {
  const send = getNativeFunction(NATIVE_SEND) as (payload: string) => Promise<unknown>;
  const programFn = getNativeFunction(NATIVE_PROGRAM) as (payload: string) => Promise<unknown>;
  const libraryFn = getNativeFunction(NATIVE_LIBRARY) as (payload: string) => Promise<unknown>;
  const telemetryListeners = new Set<TelemetryListener>();
  const paramsListeners = new Set<ParamsListener>();

  juce.backend.addEventListener(TELEMETRY_EVENT, (payload: unknown) => {
    const frame = normaliseFrame(payload);
    if (frame) telemetryListeners.forEach((l) => l(frame));
  });

  juce.backend.addEventListener(PARAMS_EVENT, (payload: unknown) => {
    const frame = normaliseParams(payload);
    if (frame) paramsListeners.forEach((l) => l(frame));
  });

  return {
    hosted: true,
    setParam(id, value) {
      void send(JSON.stringify({ type: "param", id, value }));
    },
    setParams(params) {
      void send(JSON.stringify({ type: "batch", params }));
    },
    onTelemetry(listener) {
      telemetryListeners.add(listener);
      return () => telemetryListeners.delete(listener);
    },
    onParams(listener) {
      paramsListeners.add(listener);
      return () => paramsListeners.delete(listener);
    },
    program(action, index) {
      return programFn(JSON.stringify({ action, index })).then(normaliseProgram);
    },
    library(action) {
      return libraryFn(JSON.stringify({ action })).then((r) => {
        const o = (typeof r === "object" && r !== null ? r : {}) as Partial<LibraryInfo>;
        return {
          count: typeof o.count === "number" ? o.count : 0,
          imported: typeof o.imported === "number" ? o.imported : 0,
        };
      });
    },
  };
}

//  Deterministic mock so the UI is fully explorable in a browser tab.
function createMockIntegration(): JuceIntegration {
  const listeners = new Set<TelemetryListener>();
  const bins = 256;
  let t = 0;

  const tick = () => {
    t += 0.03;
    const fft = new Array<number>(bins);
    for (let i = 0; i < bins; i++) {
      const f = i / bins;
      const env = Math.exp(-f * 4);
      fft[i] = Math.max(
        0,
        env * (0.5 + 0.5 * Math.sin(t * 2 + f * 40)) * (0.6 + 0.4 * Math.sin(t * 0.7)),
      );
    }
    const capillary = 0.5 + 0.5 * Math.sin(t * 0.9);
    const frame: TelemetryFrame = {
      type: "telemetry",
      fft,
      capillary,
      activeGrains: Math.round(24 + 20 * Math.abs(Math.sin(t))),
    };
    listeners.forEach((l) => l(frame));
    requestAnimationFrame(tick);
  };
  requestAnimationFrame(tick);

  return {
    hosted: false,
    setParam(id, value) {
      // eslint-disable-next-line no-console
      console.debug(`[mock] ${id} = ${value.toFixed(3)}`);
    },
    setParams(params) {
      // eslint-disable-next-line no-console
      console.debug("[mock] batch", params);
    },
    onTelemetry(listener) {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    onParams() {
      // Mock has no host automation source; nothing to subscribe to.
      return () => {};
    },
    program(action, index) {
      const count = 876;
      if (action === "next") mockProgram = (mockProgram + 1) % count;
      else if (action === "prev") mockProgram = (mockProgram - 1 + count) % count;
      else if (action === "set" && index !== undefined)
        mockProgram = Math.max(0, Math.min(count - 1, index));
      return Promise.resolve({ index: mockProgram, name: `Preset ${mockProgram + 1}`, count });
    },
    library(action) {
      // eslint-disable-next-line no-console
      console.debug(`[mock] library ${action}`);
      return Promise.resolve({ count: 876, imported: 0 });
    },
  };
}

let mockProgram = 0;

function normaliseParams(payload: unknown): ParamsFrame | null {
  if (typeof payload === "object" && payload !== null) {
    const p = payload as { params?: unknown };
    if (typeof p.params === "object" && p.params !== null) {
      const out: Record<string, number> = {};
      for (const [k, v] of Object.entries(p.params as Record<string, unknown>)) {
        if (typeof v === "number") out[k] = v;
      }
      return { type: "params", params: out };
    }
  }
  return null;
}

function normaliseFrame(payload: unknown): TelemetryFrame | null {
  if (typeof payload === "object" && payload !== null) {
    const p = payload as Partial<TelemetryFrame>;
    if (Array.isArray(p.fft)) {
      return {
        type: "telemetry",
        fft: p.fft as number[],
        capillary: typeof p.capillary === "number" ? p.capillary : 0,
        activeGrains: typeof p.activeGrains === "number" ? p.activeGrains : 0,
      };
    }
  }
  return null;
}

//  The vendored check_native_interop.js installs a placeholder window.__JUCE__
//  even in a plain browser, so presence alone is not proof of a live backend.
//  Real hosting is confirmed only when the backend has registered our native
//  function; otherwise we run the self-animating mock.
function isLiveBackend(juce: JuceGlobal | undefined): juce is JuceGlobal {
  return (
    !!juce &&
    Array.isArray(juce.initialisationData?.__juce__functions) &&
    juce.initialisationData!.__juce__functions!.includes(NATIVE_SEND)
  );
}

export const juceIntegration: JuceIntegration =
  typeof window !== "undefined" && isLiveBackend(window.__JUCE__)
    ? createHostedIntegration(window.__JUCE__)
    : createMockIntegration();

if (typeof window !== "undefined") {
  window.juceIntegration = juceIntegration;
}
