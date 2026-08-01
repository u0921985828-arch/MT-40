//==============================================================================
//  CannabisLeaf.tsx
//
//  Two crossed seven-point fan leaves lying on the substrate — the unmistakable
//  cannabis silhouette, one per chromosome (A chlorophyll, B magenta). Each leaf
//  is procedurally built from 7 serrated leaflets radiating from a common
//  petiole. They breathe with the capillary phase, reinforcing the "phenotype"
//  identity beneath the genome.
//==============================================================================

import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { PALETTE } from "./theme";
import { telemetry } from "../store/usePhenotypeStore";

function leafletOutline(angle: number, length: number, width: number): THREE.Vector2[] {
  const dir = new THREE.Vector2(Math.cos(angle), Math.sin(angle));
  const nor = new THREE.Vector2(-dir.y, dir.x);
  const pts: THREE.Vector2[] = [];
  const N = 16;
  const edge = (sign: number, i: number) => {
    const t = i / N;
    const along = t * length;
    // Blade profile: widest ~40% up, tapering to a fine tip.
    const base = Math.sin(Math.PI * Math.min(t, 1)) ** 0.7;
    // Serrations: alternating notch on the margin.
    const serr = i > 1 && i < N ? (i % 2 === 0 ? 0 : -0.16) : 0;
    const w = width * base * (1 + serr) * (1 - 0.1 * t);
    return dir
      .clone()
      .multiplyScalar(along)
      .add(nor.clone().multiplyScalar(sign * w));
  };
  for (let i = 0; i <= N; i++) pts.push(edge(1, i));
  for (let i = N; i >= 0; i--) pts.push(edge(-1, i));
  return pts;
}

function buildLeafGeometry(): THREE.ShapeGeometry {
  const angles = [-70, -44, -21, 0, 21, 44, 70].map((d) => (d * Math.PI) / 180 + Math.PI / 2);
  const lengths = [0.5, 0.72, 0.92, 1.0, 0.92, 0.72, 0.5];
  const shapes = angles.map((a, i) => {
    const s = new THREE.Shape(leafletOutline(a, lengths[i]! * 3.4, 0.42));
    return s;
  });
  return new THREE.ShapeGeometry(shapes);
}

function Leaf({
  color,
  rotation,
  offset,
}: {
  color: string;
  rotation: number;
  offset: [number, number];
}) {
  const ref = useRef<THREE.Group>(null);
  const geo = useMemo(() => buildLeafGeometry(), []);
  const edges = useMemo(() => new THREE.EdgesGeometry(geo), [geo]);
  const fillMat = useMemo(
    () =>
      new THREE.MeshBasicMaterial({
        color: new THREE.Color(color),
        transparent: true,
        opacity: 0.08,
        side: THREE.DoubleSide,
        depthWrite: false,
        toneMapped: false,
      }),
    [color],
  );
  const lineMat = useMemo(
    () =>
      new THREE.LineBasicMaterial({
        color: new THREE.Color(color),
        transparent: true,
        opacity: 0.4,
        toneMapped: false,
      }),
    [color],
  );

  useFrame(() => {
    const breath = 0.7 + 0.3 * telemetry.capillary;
    fillMat.opacity = 0.05 + 0.08 * telemetry.capillary;
    lineMat.opacity = 0.25 + 0.35 * breath * (0.5 + Math.min(1, telemetry.activeGrains / 48) * 0.5);
  });

  return (
    <group
      ref={ref}
      position={[offset[0], -0.58, offset[1]]}
      rotation={[-Math.PI / 2, 0, rotation]}
    >
      <mesh geometry={geo} material={fillMat} />
      <primitive object={new THREE.LineSegments(edges, lineMat)} />
    </group>
  );
}

export function CannabisLeaves() {
  return (
    <group>
      <Leaf color={PALETTE.leafA} rotation={0.35} offset={[-1.6, -1.2]} />
      <Leaf color={PALETTE.leafB} rotation={-0.35} offset={[1.6, -1.2]} />
    </group>
  );
}
