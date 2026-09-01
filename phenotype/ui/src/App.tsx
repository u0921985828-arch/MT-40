//==============================================================================
//  App.tsx — Phenotype shell: living genome viewport + instrument control rack.
//==============================================================================

import { useCallback, useEffect, useState } from "react";
import { IsometricGrid } from "./three/IsometricGrid";
import { usePhenotypeStore } from "./store/usePhenotypeStore";
import { Knob, type KnobDef, Tag, IconButton, Screen, PresetBrowser } from "./components";
import { PALETTE } from "./three/theme";
import { juceIntegration, type ProgramInfo } from "./bridge/juceIntegration";

const RACK: { title: string; tag: string; controls: KnobDef[]; collapsible?: boolean }[] = [
  {
    title: "Capillary Modulator",
    tag: "MOD",
    controls: [
      { id: "caudal", label: "Caudal", def: 0.5 },
      { id: "soilDensity", label: "Densidad Suelo", def: 0.5 },
      { id: "saturation", label: "Saturación", def: 0.9 },
      { id: "modDepth", label: "Profundidad", def: 0.5 },
    ],
  },
  {
    title: "Granular Cloud",
    tag: "GRAIN",
    controls: [
      { id: "grainDensity", label: "Densidad", def: 0.4 },
      { id: "grainSize", label: "Tamaño", def: 0.3 },
      { id: "position", label: "Posición", def: 0.5 },
      { id: "spray", label: "Dispersión", def: 0.2 },
    ],
  },
  {
    title: "Diploid Genome",
    tag: "A×B",
    controls: [
      { id: "pitchA", label: "Cromosoma A", chromosome: 0, def: 0.5 },
      { id: "pitchB", label: "Cromosoma B", chromosome: 1, def: 0.5 },
      { id: "crossBlend", label: "Cross-Synth", def: 0.5 },
      { id: "outputGain", label: "Salida", def: 0.8 },
    ],
  },
  {
    title: "Filtro",
    tag: "SVF",
    controls: [
      { id: "filterType", label: "Tipo", def: 0.0 },
      { id: "filterCutoff", label: "Cutoff", def: 1.0 },
      { id: "filterReso", label: "Resonancia", def: 0.12 },
      { id: "filterMod", label: "Capilar→Cut", def: 0.0 },
    ],
  },
  {
    title: "Textura & Espacio",
    tag: "TONE",
    controls: [
      { id: "drive", label: "Drive", def: 0.1 },
      { id: "unison", label: "Unison", def: 0.0 },
      { id: "unisonDetune", label: "Detune", def: 0.25 },
      { id: "stereoWidth", label: "Anchura", def: 0.5 },
    ],
  },
  {
    title: "Arpegiador & Escala",
    tag: "ARP",
    controls: [
      { id: "arpOn", label: "Activo", def: 0.0 },
      { id: "arpRate", label: "Velocidad", def: 0.4 },
      { id: "arpMode", label: "Patrón", def: 0.0 },
      { id: "arpSync", label: "Sync", def: 0.0 },
      { id: "scaleType", label: "Escala", def: 0.0 },
    ],
  },
  {
    title: "FX · Espacio",
    tag: "FX",
    collapsible: true,
    controls: [
      { id: "delayMix", label: "Delay", def: 0.0 },
      { id: "delayTime", label: "Time", def: 0.35 },
      { id: "delayFb", label: "Feedback", def: 0.35 },
      { id: "reverbMix", label: "Reverb", def: 0.0 },
      { id: "reverbSize", label: "Size", def: 0.5 },
      { id: "reverbDamp", label: "Damp", def: 0.4 },
    ],
  },
];

function RackSection({ group }: { group: (typeof RACK)[number] }) {
  const [open, setOpen] = useState(!group.collapsible);
  return (
    <section
      className={"ph-sec" + (group.collapsible ? " ph-sec--coll" : "")}
      style={{ ["--n" as string]: group.controls.length }}
    >
      <div className="ph-sec__hd">
        <Tag>{group.tag}</Tag>
        <span className="ph-sec__name">{group.title}</span>
        {group.collapsible && (
          <button
            className="ph-sec__tog"
            aria-expanded={open}
            aria-label={open ? "Colapsar FX" : "Expandir FX"}
            onClick={() => setOpen((o) => !o)}
          >
            <span className="ph-sec__arw">{open ? "▼" : "▲"}</span>
          </button>
        )}
      </div>
      {open && (
        <div className="ph-sec__knobs">
          {group.controls.map((c) => (
            <Knob key={c.id} def={c} />
          ))}
        </div>
      )}
    </section>
  );
}

function PresetBar() {
  const [info, setInfo] = useState<ProgramInfo>({ index: 0, name: "—", count: 0 });
  useEffect(() => {
    let live = true;
    juceIntegration.program("get").then((p) => live && setInfo(p));
    return () => {
      live = false;
    };
  }, []);
  const step = useCallback((dir: "prev" | "next") => {
    juceIntegration.program(dir).then(setInfo);
  }, []);
  const [browse, setBrowse] = useState(false);
  const pick = useCallback((index: number) => {
    juceIntegration.program("set", index).then(setInfo);
    setBrowse(false);
  }, []);

  const [lib, rest] = info.name.includes(" > ")
    ? [info.name.split(" > ")[0]!, info.name.split(" > ").slice(1).join(" > ")]
    : ["PRESET", info.name];

  return (
    <>
      <div className="ph-preset">
        <IconButton aria-label="Preset anterior" onClick={() => step("prev")}>
          ◀
        </IconButton>
        <button
          className="ph-preset__open"
          aria-label="Abrir navegador de presets"
          title="Explorar librería"
          onClick={() => setBrowse(true)}
        >
          <Screen label={lib} title={rest} meta={info.count > 0 ? `${info.index + 1} / ${info.count}` : undefined} />
        </button>
        <IconButton aria-label="Preset siguiente" onClick={() => step("next")}>
          ▶
        </IconButton>
        <IconButton
          tone="magenta"
          aria-label="Explorar / importar librería"
          title="Explorar / importar librería"
          onClick={() => setBrowse(true)}
        >
          ☰
        </IconButton>
      </div>
      <PresetBrowser open={browse} currentIndex={info.index} onSelect={pick} onClose={() => setBrowse(false)} />
    </>
  );
}

export default function App() {
  const hosted = usePhenotypeStore((s) => s.hosted);
  const activeGrains = usePhenotypeStore((s) => s.activeGrains);

  return (
    <div className="ph-root">
      {/* Full-bleed living genome behind everything. */}
      <div className="ph-stage">
        <IsometricGrid />
        <div className="ph-vignette" aria-hidden="true" />
      </div>

      {/* Floating HUD. The layer itself is click-through; panels re-enable it,
          so empty space still passes clicks to the 3D scene (node scrubbing). */}
      <div className="ph-hud">
        <header className="ph-top">
          <div className="ph-brand">
            <div className="ph-mark" aria-hidden="true">
              <span className="ph-mark__a" />
              <span className="ph-mark__b" />
            </div>
            <div className="ph-title">
              <h1>
                PHENO<span style={{ color: PALETTE.chlorophyll }}>TYPE</span>
              </h1>
              <p className="ph-tag">granular diploide</p>
            </div>
          </div>

          <PresetBar />

          <div className="ph-status">
            <span className={hosted ? "ph-dot ph-dot--live" : "ph-dot"} />
            <span className="ph-conn">{hosted ? "LIVE" : "MOCK"}</span>
          </div>
        </header>

        <aside className="ph-console">
          {RACK.map((group) => (
            <RackSection key={group.title} group={group} />
          ))}
        </aside>

        <footer className="ph-bottom">
          <div className="ph-bottom__geno">
            <span className="ph-chip ph-chip--a">◆ A</span>
            <span className="ph-bottom__x">×</span>
            <span className="ph-chip ph-chip--b">◆ B</span>
          </div>
          <div className="ph-bottom__mid">granular cross-synthesis · capillary modulation</div>
          <div className="ph-bottom__stat">
            <span className="ph-bottom__grains">
              <b>{activeGrains}</b> granos
            </span>
          </div>
        </footer>
      </div>
    </div>
  );
}
