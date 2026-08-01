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
import { PALETTE, HELIX } from "./theme";
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

  // Assign per-instance base colours once.
  const nucInit = useRef(false);

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

      // Two nucleotide instances per pair: 2i = A, 2i+1 = B.
      const s = 0.09 + e * 0.16;
      place(nuc, 2 * i, pairs[i]!.a, s, colA, !nucInit.current);
      place(nuc, 2 * i + 1, pairs[i]!.b, s, colB, !nucInit.current);

      // Rung colour: green->magenta gradient, brightened by band energy.
      tmpColor.copy(colA).lerp(colB, pairs[i]!.band).multiplyScalar(0.5 + e * 1.5);
      rungCol.setXYZ(2 * i, tmpColor.r, tmpColor.g, tmpColor.b);
      rungCol.setXYZ(2 * i + 1, tmpColor.r, tmpColor.g, tmpColor.b);
    }
    nuc.instanceMatrix.needsUpdate = true;
    if (nuc.instanceColor) nuc.instanceColor.needsUpdate = true;
    rungCol.needsUpdate = true;
    rungMat.opacity = 0.35 + telemetry.capillary * 0.4;
    nucInit.current = true;
  });

  return (
    <group ref={groupRef}>
      <instancedMesh ref={nucRef} args={[undefined, undefined, pairs.length * 2]}>
        <sphereGeometry args={[1, 12, 12]} />
        <meshStandardMaterial roughness={0.3} metalness={0.15} toneMapped={false} />
      </instancedMesh>
      <primitive object={new THREE.LineSegments(rungGeo, rungMat)} />
      <primitive object={backboneA} />
      <primitive object={backboneB} />
    </group>
  );
}

function place(
  mesh: THREE.InstancedMesh,
  index: number,
  p: THREE.Vector3,
  scale: number,
  color: THREE.Color,
  setColor: boolean,
) {
  dummy.position.copy(p);
  dummy.scale.setScalar(scale);
  dummy.updateMatrix();
  mesh.setMatrixAt(index, dummy.matrix);
  if (setColor) mesh.setColorAt(index, color);
}

function makeStrand(points: THREE.Vector3[], color: string): THREE.Line {
  const curve = new THREE.CatmullRomCurve3(points);
  const geo = new THREE.BufferGeometry().setFromPoints(curve.getPoints(points.length * 4));
  const mat = new THREE.LineBasicMaterial({
    color: new THREE.Color(color),
    transparent: true,
    opacity: 0.85,
    toneMapped: false,
  });
  return new THREE.Line(geo, mat);
}
