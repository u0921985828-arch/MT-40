//==============================================================================
//  CannabisLeaf.tsx
//
//  Procedural seven-point fan leaves — the cannabis silhouette, one per
//  chromosome (A chlorophyll, B magenta), plus a larger dim leaf set behind for
//  depth. Each leaf now carries glowing veins (midrib + branch veins), a fuller
//  gradient body, and a gentle cup (z-displacement) so it catches light and
//  bloom instead of reading as a flat cut-out. All breathe with the capillary
//  phase and brighten with grain activity.
//==============================================================================

import { useMemo } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { PALETTE, GLOW } from "./theme";
import { telemetry } from "../store/usePhenotypeStore";

const LEAFLETS = [-72, -46, -22, 0, 22, 46, 72].map((d) => (d * Math.PI) / 180 + Math.PI / 2);
const LENGTHS = [0.46, 0.7, 0.9, 1.0, 0.9, 0.7, 0.46];
const BLADE = 3.6;
const WIDTH = 0.4;

// Gentle downward cup so the blade isn't perfectly flat.
const cup = (x: number, y: number) => -0.09 * (x * x + y * y);

function leafletOutline(angle: number, length: number, width: number): THREE.Vector2[] {
  const dir = new THREE.Vector2(Math.cos(angle), Math.sin(angle));
  const nor = new THREE.Vector2(-dir.y, dir.x);
  const pts: THREE.Vector2[] = [];
  const N = 26;
  const edge = (sign: number, i: number) => {
    const t = i / N;
    const along = t * length;
    const base = Math.sin(Math.PI * Math.min(t, 1)) ** 0.62; // widest ~40%, fine tip
    const serr = i > 1 && i < N ? (i % 2 === 0 ? 0 : -0.12) : 0; // finer teeth
    const w = width * base * (1 + serr) * (1 - 0.35 * t * t); // taper harder near apex
    return dir.clone().multiplyScalar(along).add(nor.clone().multiplyScalar(sign * w));
  };
  for (let i = 0; i <= N; i++) pts.push(edge(1, i));
  for (let i = N; i >= 0; i--) pts.push(edge(-1, i));
  return pts;
}

function buildLeaf() {
  const shapes = LEAFLETS.map((a, i) => new THREE.Shape(leafletOutline(a, LENGTHS[i]! * BLADE, WIDTH)));
  const geo = new THREE.ShapeGeometry(shapes, 10);

  // Cup the blade in z + tint from base (dark) to tip (bright) via vertex colour.
  const pos = geo.getAttribute("position") as THREE.BufferAttribute;
  const col = new Float32Array(pos.count * 3);
  let maxR = 0.001;
  for (let i = 0; i < pos.count; i++) maxR = Math.max(maxR, Math.hypot(pos.getX(i), pos.getY(i)));
  for (let i = 0; i < pos.count; i++) {
    const x = pos.getX(i), y = pos.getY(i);
    pos.setZ(i, cup(x, y));
    const r = Math.hypot(x, y) / maxR;
    const c = 0.25 + 0.75 * r; // brighter toward the tips
    col[i * 3] = c; col[i * 3 + 1] = c; col[i * 3 + 2] = c;
  }
  pos.needsUpdate = true;
  geo.setAttribute("color", new THREE.BufferAttribute(col, 3));
  geo.computeVertexNormals();

  // Veins: midrib per leaflet + a few branch veins, cupped to match the blade.
  const vp: number[] = [];
  const P = (x: number, y: number) => vp.push(x, y, cup(x, y));
  LEAFLETS.forEach((a, i) => {
    const len = LENGTHS[i]! * BLADE;
    const dir = new THREE.Vector2(Math.cos(a), Math.sin(a));
    const nor = new THREE.Vector2(-dir.y, dir.x);
    const tip = dir.clone().multiplyScalar(len);
    P(0, 0); P(tip.x, tip.y); // midrib
    for (const t of [0.32, 0.55, 0.76]) {
      const base = Math.sin(Math.PI * t) ** 0.62;
      const along = dir.clone().multiplyScalar(t * len);
      const w = WIDTH * base * (1 - 0.35 * t * t) * 0.82;
      for (const s of [1, -1]) {
        const end = along.clone().add(nor.clone().multiplyScalar(s * w));
        P(along.x, along.y); P(end.x, end.y);
      }
    }
  });
  const veins = new THREE.BufferGeometry();
  veins.setAttribute("position", new THREE.BufferAttribute(new Float32Array(vp), 3));

  const edges = new THREE.EdgesGeometry(geo, 25);
  return { geo, veins, edges };
}

function Leaf({
  color,
  rotation,
  offset,
  scale = 1,
  dim = 1,
}: {
  color: string;
  rotation: number;
  offset: [number, number];
  scale?: number;
  dim?: number;
}) {
  const { geo, veins, edges } = useMemo(buildLeaf, []);
  const glow = useMemo(() => new THREE.Color(color).multiplyScalar(GLOW.leaf), [color]);

  const fillMat = useMemo(
    () => new THREE.MeshBasicMaterial({
      color: new THREE.Color(color),
      vertexColors: true,
      transparent: true,
      opacity: 0.16 * dim,
      side: THREE.DoubleSide,
      depthWrite: false,
      toneMapped: false,
    }),
    [color, dim],
  );
  const veinMat = useMemo(
    () => new THREE.LineBasicMaterial({ color: glow, transparent: true, opacity: 0.4 * dim, toneMapped: false }),
    [glow, dim],
  );
  const lineMat = useMemo(
    () => new THREE.LineBasicMaterial({ color: glow, transparent: true, opacity: 0.6 * dim, toneMapped: false }),
    [glow, dim],
  );

  useFrame(() => {
    const grains = Math.min(1, telemetry.activeGrains / 48);
    const breath = 0.7 + 0.3 * telemetry.capillary;
    fillMat.opacity = (0.1 + 0.16 * telemetry.capillary) * dim;
    veinMat.opacity = (0.25 + 0.45 * breath * (0.5 + grains * 0.5)) * dim;
    lineMat.opacity = (0.4 + 0.4 * breath * (0.5 + grains * 0.5)) * dim;
  });

  const veinObj = useMemo(() => new THREE.LineSegments(veins, veinMat), [veins, veinMat]);
  const edgeObj = useMemo(() => new THREE.LineSegments(edges, lineMat), [edges, lineMat]);

  return (
    <group position={[offset[0], -0.6, offset[1]]} rotation={[-Math.PI / 2, 0, rotation]} scale={scale}>
      <mesh geometry={geo} material={fillMat} />
      <primitive object={veinObj} />
      <primitive object={edgeObj} />
    </group>
  );
}

export function CannabisLeaves() {
  return (
    <group>
      {/* Dim oversized backdrop leaf for depth. */}
      <Leaf color={PALETTE.leafA} rotation={0.1} offset={[0.2, -0.6]} scale={1.7} dim={0.35} />
      {/* Foreground diploid pair, anchored symmetrically at the helix base. */}
      <Leaf color={PALETTE.leafA} rotation={0.42} offset={[-1.7, -1.0]} />
      <Leaf color={PALETTE.leafB} rotation={-0.42} offset={[1.7, -1.0]} />
    </group>
  );
}
