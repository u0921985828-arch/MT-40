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
import { telemetry } from "../store/usePhenotypeStore";

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

  useFrame((_, delta) => {
    const g = groupRef.current;
    const nuc = nucRef.current;
    if (!g || !nuc) return;

    g.rotation.y += delta * HELIX.spin;
    g.position.y = telemetry.capillary * 0.5 - 0.25;

    const fft = telemetry.fft;
    const rungCol = rungGeo.getAttribute("color") as THREE.BufferAttribute;

    for (let i = 0; i < pairs.length; i++) {
      const bin = Math.min(fft.length - 1, Math.floor(pairs[i]!.band * fft.length));
      const e = fft[bin] ?? 0;

      // Two nucleotide instances per pair (2i = A, 2i+1 = B). HDR colour so the
      // beads bloom; energy drives both size and brightness.
      const s = 0.11 + e * 0.2;
      place(nuc, 2 * i, pairs[i]!.a, s, colA, GLOW.nucleusA * (0.55 + e * 1.2));
      place(nuc, 2 * i + 1, pairs[i]!.b, s, colB, GLOW.nucleusB * (0.55 + e * 1.2));

      // Rung colour: green->magenta gradient, brightened (HDR) by band energy.
      tmpColor
        .copy(colA)
        .lerp(colB, pairs[i]!.band)
        .multiplyScalar(GLOW.rung * (0.4 + e * 1.6));
      rungCol.setXYZ(2 * i, tmpColor.r, tmpColor.g, tmpColor.b);
      rungCol.setXYZ(2 * i + 1, tmpColor.r, tmpColor.g, tmpColor.b);
    }
    nuc.instanceMatrix.needsUpdate = true;
    if (nuc.instanceColor) nuc.instanceColor.needsUpdate = true;
    rungCol.needsUpdate = true;
    rungMat.opacity = 0.5 + telemetry.capillary * 0.45;
  });

  return (
    <group ref={groupRef}>
      <instancedMesh ref={nucRef} args={[undefined, undefined, pairs.length * 2]}>
        <sphereGeometry args={[1, 14, 14]} />
        <meshBasicMaterial toneMapped={false} />
      </instancedMesh>
      <primitive object={new THREE.LineSegments(rungGeo, rungMat)} />
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

function makeStrand(points: THREE.Vector3[], color: string): THREE.Line {
  const curve = new THREE.CatmullRomCurve3(points);
  const geo = new THREE.BufferGeometry().setFromPoints(curve.getPoints(points.length * 4));
  const mat = new THREE.LineBasicMaterial({
    color: new THREE.Color(color).multiplyScalar(GLOW.rung),
    transparent: true,
    opacity: 0.95,
    toneMapped: false,
  });
  return new THREE.Line(geo, mat);
}
