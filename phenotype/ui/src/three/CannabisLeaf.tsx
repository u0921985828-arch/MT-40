//==============================================================================
//  CannabisLeaf.tsx
//
//  Procedural seven-point fan leaves — the cannabis silhouette, laid flat on the
//  grow floor as a soft botanical backdrop behind the genome. Each leaf reads as
//  a glowing filled blade with a luminous midrib skeleton and a fine rim, tinted
//  base->tip. No hard wireframe edges (those made the old leaves read as spiky
//  bursts) — just soft fill + veins that breathe with the capillary phase and
//  brighten with grain activity. Placed symmetrically so they frame, never
//  clutter, the helix.
//==============================================================================

import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { PALETTE, GLOW } from "./theme";
import { telemetry } from "../store/usePhenotypeStore";

const LEAFLETS = [-72, -46, -22, 0, 22, 46, 72].map((d) => (d * Math.PI) / 180 + Math.PI / 2);
const LENGTHS = [0.44, 0.68, 0.9, 1.0, 0.9, 0.68, 0.44];
const BLADE = 3.6;
const WIDTH = 0.42;

// Gentle downward cup so the blade isn't perfectly flat and catches the bloom.
const cup = (x: number, y: number) => -0.07 * (x * x + y * y);

// Smooth leaflet outline — fat around 40%, drawn to a fine tip, NO serration.
function leafletOutline(angle: number, length: number, width: number): THREE.Vector2[] {
  const dir = new THREE.Vector2(Math.cos(angle), Math.sin(angle));
  const nor = new THREE.Vector2(-dir.y, dir.x);
  const pts: THREE.Vector2[] = [];
  const N = 30;
  const edge = (sign: number, i: number) => {
    const t = i / N;
    const along = t * length;
    const base = Math.sin(Math.PI * Math.min(t, 1)) ** 0.6; // widest ~40%, fine tip
    const w = width * base * (1 - 0.34 * t * t); // taper harder near the apex
    return dir.clone().multiplyScalar(along).add(nor.clone().multiplyScalar(sign * w));
  };
  for (let i = 0; i <= N; i++) pts.push(edge(1, i));
  for (let i = N; i >= 0; i--) pts.push(edge(-1, i));
  return pts;
}

function buildLeaf() {
  const shapes = LEAFLETS.map((a, i) => new THREE.Shape(leafletOutline(a, LENGTHS[i]! * BLADE, WIDTH)));
  const geo = new THREE.ShapeGeometry(shapes, 12);

  // Cup the blade in z + tint base (dark) -> tip (bright) via vertex colour.
  const pos = geo.getAttribute("position") as THREE.BufferAttribute;
  const col = new Float32Array(pos.count * 3);
  let maxR = 0.001;
  for (let i = 0; i < pos.count; i++) maxR = Math.max(maxR, Math.hypot(pos.getX(i), pos.getY(i)));
  for (let i = 0; i < pos.count; i++) {
    const x = pos.getX(i), y = pos.getY(i);
    pos.setZ(i, cup(x, y));
    const r = Math.hypot(x, y) / maxR;
    const c = 0.18 + 0.82 * r * r; // dark base, luminous tips
    col[i * 3] = c; col[i * 3 + 1] = c; col[i * 3 + 2] = c;
  }
  pos.needsUpdate = true;
  geo.setAttribute("color", new THREE.BufferAttribute(col, 3));
  geo.computeVertexNormals();

  // Vein skeleton: a single clean midrib per leaflet — no branch veins, so the
  // blades read as soft glowing fans rather than a technical mesh.
  const vp: number[] = [];
  const P = (x: number, y: number) => vp.push(x, y, cup(x, y) + 0.002);
  LEAFLETS.forEach((a, i) => {
    const len = LENGTHS[i]! * BLADE;
    const dir = new THREE.Vector2(Math.cos(a), Math.sin(a));
    const tip = dir.clone().multiplyScalar(len);
    P(0, 0); P(tip.x, tip.y); // midrib
  });
  const veins = new THREE.BufferGeometry();
  veins.setAttribute("position", new THREE.BufferAttribute(new Float32Array(vp), 3));

  return { geo, veins };
}

function Leaf({
  color,
  rotation,
  offset,
  y = -0.62,
  scale = 1,
  dim = 1,
}: {
  color: string;
  rotation: number;
  offset: [number, number];
  y?: number;
  scale?: number;
  dim?: number;
}) {
  const groupRef = useRef<THREE.Group>(null);
  const { geo, veins } = useMemo(buildLeaf, []);
  const glow = useMemo(() => new THREE.Color(color).multiplyScalar(GLOW.leaf), [color]);
  const phase = useMemo(() => offset[0] * 1.3 + offset[1] * 0.7, [offset]);

  const fillMat = useMemo(
    () => new THREE.MeshBasicMaterial({
      color: new THREE.Color(color),
      vertexColors: true,
      transparent: true,
      opacity: 0.14 * dim,
      side: THREE.DoubleSide,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      toneMapped: false,
    }),
    [color, dim],
  );
  const veinMat = useMemo(
    () => new THREE.LineBasicMaterial({ color: glow, transparent: true, opacity: 0.35 * dim, toneMapped: false, blending: THREE.AdditiveBlending }),
    [glow, dim],
  );

  useFrame((state) => {
    const grains = Math.min(1, telemetry.activeGrains / 48);
    const breath = 0.7 + 0.3 * telemetry.capillary;
    fillMat.opacity = (0.08 + 0.14 * telemetry.capillary) * dim;
    veinMat.opacity = (0.15 + 0.28 * breath * (0.5 + grains * 0.5)) * dim;

    // Living plant: a slow wind sway + gentle breath, desynced per leaf.
    const g = groupRef.current;
    if (g) {
      const t = state.clock.elapsedTime;
      g.rotation.z = rotation + Math.sin(t * 0.5 + phase) * 0.05;
      const s = scale * (1 + Math.sin(t * 0.6 + phase) * 0.02 + 0.03 * telemetry.capillary);
      g.scale.setScalar(s);
    }
  });

  const veinObj = useMemo(() => new THREE.LineSegments(veins, veinMat), [veins, veinMat]);

  return (
    <group ref={groupRef} position={[offset[0], y, offset[1]]} rotation={[-Math.PI / 2, 0, rotation]} scale={scale}>
      <mesh geometry={geo} material={fillMat} />
      <primitive object={veinObj} />
    </group>
  );
}

export function CannabisLeaves() {
  return (
    <group>
      {/* Big dim leaf fanning straight back from the genome base — depth. */}
      <Leaf color={PALETTE.leafA} rotation={Math.PI} offset={[0, 0.9]} y={-0.68} scale={2.1} dim={0.22} />
      {/* Symmetric diploid pair fanning down-and-out from the helix foot. */}
      <Leaf color={PALETTE.leafA} rotation={2.55} offset={[-2.9, 1.55]} scale={1.35} dim={0.6} />
      <Leaf color={PALETTE.leafB} rotation={-2.55 + Math.PI * 2} offset={[2.9, 1.55]} scale={1.35} dim={0.6} />
    </group>
  );
}
