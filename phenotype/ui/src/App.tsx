//==============================================================================
//  App.tsx — Phenotype shell: isometric viewport + genotype control rack.
//==============================================================================

import { IsometricGrid } from "./three/IsometricGrid";
import { usePhenotypeStore, type ParamId } from "./store/usePhenotypeStore";
import { PALETTE } from "./three/theme";

interface ControlDef {
  id: ParamId;
  label: string;
  chromosome?: 0 | 1;
}

const RACK: { title: string; controls: ControlDef[] }[] = [
  {
    title: "Capillary Modulator",
    controls: [
      { id: "caudal", label: "Caudal" },
      { id: "soilDensity", label: "Densidad del Suelo" },
      { id: "saturation", label: "Saturación" },
      { id: "modDepth", label: "Profundidad Mod" },
    ],
  },
  {
    title: "Granular Cloud",
    controls: [
      { id: "grainDensity", label: "Densidad" },
      { id: "grainSize", label: "Tamaño" },
      { id: "position", label: "Posición" },
      { id: "spray", label: "Dispersión" },
    ],
  },
  {
    title: "Diploid Genome",
    controls: [
      { id: "pitchA", label: "Cromosoma A", chromosome: 0 },
      { id: "pitchB", label: "Cromosoma B", chromosome: 1 },
      { id: "crossBlend", label: "Cross-Synthesis" },
      { id: "outputGain", label: "Salida" },
    ],
  },
  {
    title: "Arpeggiator",
    controls: [
      { id: "arpOn", label: "Arpegiador" },
      { id: "arpRate", label: "Velocidad / División" },
      { id: "arpMode", label: "Patrón" },
      { id: "arpSync", label: "Sync Tempo" },
    ],
  },
  {
    title: "Escala",
    controls: [{ id: "scaleType", label: "Cuantizador" }],
  },
];

function Slider({ def }: { def: ControlDef }) {
  const value = usePhenotypeStore((s) => s.params[def.id]);
  const setParam = usePhenotypeStore((s) => s.setParam);
  const setDragging = usePhenotypeStore((s) => s.setDragging);
  const accent =
    def.chromosome === 0
      ? PALETTE.chlorophyll
      : def.chromosome === 1
        ? PALETTE.ledMagenta
        : PALETTE.ink;

  return (
    <label className="ph-slider">
      <span className="ph-slider__label">{def.label}</span>
      <input
        type="range"
        min={0}
        max={1}
        step={0.001}
        value={value}
        style={{ accentColor: accent }}
        onPointerDown={() => setDragging(def.id)}
        onPointerUp={() => setDragging(null)}
        onPointerCancel={() => setDragging(null)}
        onChange={(e) => setParam(def.id, Number(e.currentTarget.value))}
      />
      <span className="ph-slider__value">{Math.round(value * 100)}</span>
    </label>
  );
}

export default function App() {
  const hosted = usePhenotypeStore((s) => s.hosted);
  const activeGrains = usePhenotypeStore((s) => s.activeGrains);

  return (
    <div className="ph-root">
      <header className="ph-header">
        <h1>
          PHENO<span style={{ color: PALETTE.chlorophyll }}>TYPE</span>
        </h1>
        <div className="ph-status">
          <span className={hosted ? "ph-dot ph-dot--live" : "ph-dot"} />
          {hosted ? "JUCE BACKEND" : "BROWSER MOCK"} · grains {activeGrains}
        </div>
      </header>

      <main className="ph-main">
        <section className="ph-viewport">
          <IsometricGrid />
        </section>

        <aside className="ph-rack">
          {RACK.map((group) => (
            <fieldset key={group.title} className="ph-group">
              <legend>{group.title}</legend>
              {group.controls.map((c) => (
                <Slider key={c.id} def={c} />
              ))}
            </fieldset>
          ))}
        </aside>
      </main>
    </div>
  );
}
