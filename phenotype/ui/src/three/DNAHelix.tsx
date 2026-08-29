//==============================================================================
//  DNAHelix.tsx
//
//  Diploid double helix — the genetic centrepiece. Two sugar-phosphate strands
//  (chromosome A = chlorophyll, chromosome B = magenta) wind around a common
//  axis, joined by base-pair rungs. The whole genome spins slowly; each base
//  pair swells with its FFT band, the rungs brighten with band energy, and the
//  helix breathes vertically with the capillary phase. Rendered with instanced
//  spheres (nucleotides) + vertex-coloured line segments (rungs/backbones) so
//  the whole structure is a handful of draw calls.
//==============================================================================

import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { PALETTE, HELIX, GLOW } from "./theme";
import { telemetry, usePhenotypeStore } from "../store/usePhenotypeStore";

interface PairGeom {
  a: THREE.Vector3;
  b: THREE.Vector3;
  band: number;
}

const dummy = new THREE.Object3D();
const colA = new THREE.Color(PALETTE.chlorophyll);
const colB = new THREE.Color(PALETTE.ledMagenta);
const tmpColor = new THREE.Color();

export function DNAHelix() {
  const groupRef = useRef<THREE.Group>(null);
  const nucRef = useRef<THREE.InstancedMesh>(null);

  const pairs = useMemo<PairGeom[]>(() => {
    const out: PairGeom[] = [];
    const half = ((HELIX.pairs - 1) * HELIX.step) / 2;
    for (let i = 0; i < HELIX.pairs; i++) {
      const y = i * HELIX.step - half;
      const t = i * HELIX.twist;
      out.push({
        a: new THREE.Vector3(Math.cos(t) * HELIX.radius, y, Math.sin(t) * HELIX.radius),
        b: new THREE.Vector3(
          Math.cos(t + Math.PI) * HELIX.radius,
          y,
          Math.sin(t + Math.PI) * HELIX.radius,
        ),
        band: i / HELIX.pairs,
      });
    }
    return out;
  }, []);

  // Fade envelope: 0 at the two tips, 1 through the body, so the strands
  // dissolve at their extremes instead of ending abruptly.
  const endFade = useMemo(() => {
    const n = pairs.length;
    const edge = Math.max(2, Math.round(n * 0.18)); // ~18% of the length fades
    const out = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const d = Math.min(i, n - 1 - i) / edge; // 0 at a tip, >=1 inside
      const t = Math.min(1, d);
      out[i] = t * t * (3 - 2 * t); // smoothstep
    }
    return out;
  }, [pairs]);

  // Rungs (A<->B per pair) as vertex-coloured line segments.
  const rungGeo = useMemo(() => {
    const g = new THREE.BufferGeometry();
    const pos = new Float32Array(pairs.length * 2 * 3);
    const col = new Float32Array(pairs.length * 2 * 3);
    pairs.forEach((p, i) => {
      pos.set([p.a.x, p.a.y, p.a.z, p.b.x, p.b.y, p.b.z], i * 6);
    });
    g.setAttribute("position", new THREE.BufferAttribute(pos, 3));
    g.setAttribute("color", new THREE.BufferAttribute(col, 3));
    return g;
  }, [pairs]);

  const rungMat = useMemo(
    () => new THREE.LineBasicMaterial({ vertexColors: true, transparent: true, opacity: 0.6 }),
    [],
  );

  // Backbone strands.
  const backboneA = useMemo(
    () => makeStrand(pairs.map((p) => p.a), PALETTE.chlorophyll),
    [pairs],
  );
  const backboneB = useMemo(
    () => makeStrand(pairs.map((p) => p.b), PALETTE.ledMagenta),
    [pairs],
  );

  // Backbone base glow colours (the frame loop rescales these by cross-synth).
  const baseColA = useMemo(() => new THREE.Color(PALETTE.chlorophyll).multiplyScalar(GLOW.rung * 0.8), []);
  const baseColB = useMemo(() => new THREE.Color(PALETTE.ledMagenta).multiplyScalar(GLOW.rung * 0.8), []);

  const rungSegments = useMemo(() => new THREE.LineSegments(rungGeo, rungMat), [rungGeo, rungMat]);

  useFrame((_, delta) => {
    const g = groupRef.current;
    const nuc = nucRef.current;
    if (!g || !nuc) return;

    g.rotation.y += delta * HELIX.spin;
    g.position.y = telemetry.capillary * 0.5 - 0.25;

    // Preset signature (read live, no re-render): cross-synthesis weights the
    // two strands green<->magenta, output level sets the genome's overall glow.
    const P = usePhenotypeStore.getState().params;
    const gGain = 0.55 + P.outputGain * 0.95;
    const aMul = (0.4 + 1.0 * (1 - P.crossBlend)) * gGain; // chromosome A weight
    const bMul = (0.4 + 1.0 * P.crossBlend) * gGain;       // chromosome B weight

    const fft = telemetry.fft;
    const rungCol = rungGeo.getAttribute("color") as THREE.BufferAttribute;

    for (let i = 0; i < pairs.length; i++) {
      const bin = Math.min(fft.length - 1, Math.floor(pairs[i]!.band * fft.length));
      const e = fft[bin] ?? 0;
      const fade = endFade[i]!; // 0 at the tips -> 1 in the body

      // Two nucleotide instances per pair (2i = A, 2i+1 = B). HDR colour so the
      // beads bloom; energy drives size/brightness, the end-fade dissolves the
      // tips so the strands emerge from nothing instead of stopping dead.
      const s = (0.11 + e * 0.2) * (0.35 + 0.65 * fade);
      place(nuc, 2 * i, pairs[i]!.a, s, colA, GLOW.nucleusA * (0.55 + e * 1.2) * fade * aMul);
      place(nuc, 2 * i + 1, pairs[i]!.b, s, colB, GLOW.nucleusB * (0.55 + e * 1.2) * fade * bMul);

      // Rung colour: green->magenta gradient, brightened (HDR) by band energy,
      // faded to black at the tips, biased by the cross-synth balance.
      tmpColor
        .copy(colA)
        .lerp(colB, THREE.MathUtils.clamp(pairs[i]!.band * 0.5 + P.crossBlend * 0.5, 0, 1))
        .multiplyScalar(GLOW.rung * (0.4 + e * 1.6) * fade * gGain);
      rungCol.setXYZ(2 * i, tmpColor.r, tmpColor.g, tmpColor.b);
      rungCol.setXYZ(2 * i + 1, tmpColor.r, tmpColor.g, tmpColor.b);
    }
    nuc.instanceMatrix.needsUpdate = true;
    if (nuc.instanceColor) nuc.instanceColor.needsUpdate = true;
    rungCol.needsUpdate = true;
    rungMat.opacity = 0.5 + telemetry.capillary * 0.45;

    // Backbones track the same cross-synth weighting so a strand can recede.
    (backboneA.material as THREE.MeshBasicMaterial).color.copy(baseColA).multiplyScalar(0.55 + 0.9 * (1 - P.crossBlend) * gGain);
    (backboneB.material as THREE.MeshBasicMaterial).color.copy(baseColB).multiplyScalar(0.55 + 0.9 * P.crossBlend * gGain);
  });

  return (
    <group ref={groupRef}>
      <instancedMesh ref={nucRef} args={[undefined, undefined, pairs.length * 2]}>
        <sphereGeometry args={[1, 14, 14]} />
        <meshBasicMaterial toneMapped={false} />
      </instancedMesh>
      <primitive object={rungSegments} />
      <primitive object={backboneA} />
      <primitive object={backboneB} />
    </group>
  );
}

const placeColor = new THREE.Color();

function place(
  mesh: THREE.InstancedMesh,
  index: number,
  p: THREE.Vector3,
  scale: number,
  baseColor: THREE.Color,
  brightness: number,
) {
  dummy.position.copy(p);
  dummy.scale.setScalar(scale);
  dummy.updateMatrix();
  mesh.setMatrixAt(index, dummy.matrix);
  placeColor.copy(baseColor).multiplyScalar(brightness);
  mesh.setColorAt(index, placeColor);
}

function makeStrand(points: THREE.Vector3[], color: string): THREE.Mesh {
  // Volumetric sugar-phosphate strand: a smooth tube instead of a 1px line, so
  // the backbone reads as a solid glowing ribbon and the bloom wraps it. Vertex
  // colours carry a fade envelope so the tube dies to black at both tips.
  const radial = 8;
  const tubular = points.length * 6;
  const curve = new THREE.CatmullRomCurve3(points);
  const geo = new THREE.TubeGeometry(curve, tubular, 0.05, radial, false);

  const edge = 0.18; // fraction of the length that fades at each end
  const env = (u: number) => {
    const d = Math.min(u, 1 - u) / edge;
    const t = Math.min(1, d);
    return t * t * (3 - 2 * t); // smoothstep
  };
  const count = geo.getAttribute("position").count;
  const col = new Float32Array(count * 3);
  for (let v = 0; v < count; v++) {
    const ring = Math.floor(v / (radial + 1)); // 0..tubular
    const f = env(ring / tubular);
    col[v * 3] = f; col[v * 3 + 1] = f; col[v * 3 + 2] = f;
  }
  geo.setAttribute("color", new THREE.BufferAttribute(col, 3));

  const mat = new THREE.MeshBasicMaterial({
    color: new THREE.Color(color).multiplyScalar(GLOW.rung * 0.8),
    vertexColors: true,
    toneMapped: false,
  });
  return new THREE.Mesh(geo, mat);
}
