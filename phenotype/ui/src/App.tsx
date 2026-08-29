//==============================================================================
//  App.tsx — Phenotype shell: living genome viewport + instrument control rack.
//==============================================================================

import { IsometricGrid } from "./three/IsometricGrid";
import { usePhenotypeStore } from "./store/usePhenotypeStore";
import { Knob, type KnobDef } from "./Knob";
import { PALETTE } from "./three/theme";

const RACK: { title: string; tag: string; controls: KnobDef[] }[] = [
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
    title: "Arpegiador",
    tag: "ARP",
    controls: [
      { id: "arpOn", label: "Activo", def: 0.0 },
      { id: "arpRate", label: "Velocidad", def: 0.4 },
      { id: "arpMode", label: "Patrón", def: 0.0 },
      { id: "arpSync", label: "Sync", def: 0.0 },
    ],
  },
  {
    title: "Escala",
    tag: "KEY",
    controls: [{ id: "scaleType", label: "Cuantizador", def: 0.0 }],
  },
];

export default function App() {
  const hosted = usePhenotypeStore((s) => s.hosted);
  const activeGrains = usePhenotypeStore((s) => s.activeGrains);

  return (
    <div className="ph-root">
      <header className="ph-header">
        <div className="ph-brand">
          <div className="ph-mark" aria-hidden="true">
            <span className="ph-mark__a" />
            <span className="ph-mark__b" />
          </div>
          <div className="ph-title">
            <h1>
              PHENO<span style={{ color: PALETTE.chlorophyll }}>TYPE</span>
            </h1>
            <p className="ph-tag">granular diploide · cross-synthesis</p>
          </div>
        </div>
        <div className="ph-status">
          <span className={hosted ? "ph-dot ph-dot--live" : "ph-dot"} />
          <span className="ph-conn">{hosted ? "JUCE BACKEND" : "BROWSER MOCK"}</span>
        </div>
      </header>

      <main className="ph-main">
        <section className="ph-viewport">
          <IsometricGrid />
          <div className="ph-vignette" aria-hidden="true" />
        </section>

        <aside className="ph-rack">
          {RACK.map((group) => (
            <fieldset key={group.title} className="ph-group">
              <legend>
                <span className="ph-group__tag">{group.tag}</span>
                {group.title}
              </legend>
              <div className={`ph-knobs ph-knobs--${group.controls.length}`}>
                {group.controls.map((c) => (
                  <Knob key={c.id} def={c} />
                ))}
              </div>
            </fieldset>
          ))}
          <p className="ph-foot">BOTANICA DSP · v1.0</p>
        </aside>
      </main>

      <footer className="ph-bar">
        <div className="ph-bar__geno">
          <span className="ph-bar__label">Genotipo</span>
          <span className="ph-chip ph-chip--a">◆ A</span>
          <span className="ph-bar__x">×</span>
          <span className="ph-chip ph-chip--b">◆ B</span>
        </div>
        <div className="ph-bar__mid">granular cross-synthesis · capillary modulation</div>
        <div className="ph-bar__stat">
          <span className="ph-bar__grains">
            <b>{activeGrains}</b> granos activos
          </span>
          <span className="ph-bar__sep" />
          <span className={hosted ? "ph-dot ph-dot--live" : "ph-dot"} />
        </div>
      </footer>
    </div>
  );
}
