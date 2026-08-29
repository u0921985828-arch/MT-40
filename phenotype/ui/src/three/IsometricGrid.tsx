//==============================================================================
//  IsometricGrid.tsx
//
//  Orthographic grow-lab stage rendered dark with real bloom: the diploid DNA
//  helix and trichome haze glow like bioluminescent LEDs against a near-black
//  substrate, a symmetric fan of cannabis leaves fades in behind, and a quiet
//  orbital halo rings the genome. A soft radial pool of light on the floor
//  anchors the whole composition. All motion is driven by live backend
//  telemetry, with a slow idle camera breath so the scene never reads as static.
//==============================================================================

import { useMemo, useRef } from "react";
import { Canvas, useFrame, useThree } from "@react-three/fiber";
import {
  EffectComposer,
  Bloom,
  Vignette,
  SMAA,
  Noise,
  ChromaticAberration,
} from "@react-three/postprocessing";
import { BlendFunction } from "postprocessing";
import * as THREE from "three";
import { NodeGraph } from "./NodeGraph";
import { DNAHelix } from "./DNAHelix";
import { Trichomes } from "./Trichomes";
import { CannabisLeaves } from "./CannabisLeaf";
import { PALETTE, GRID_HALF } from "./theme";
import { telemetry } from "../store/usePhenotypeStore";

function IsoFloor() {
  const gridRef = useRef<THREE.GridHelper>(null);
  const grid = useRef(
    new THREE.GridHelper(GRID_HALF * 2, GRID_HALF * 3, PALETTE.chlorophyll, PALETTE.grid),
  ).current;

  useFrame(() => {
    const mat = grid.material as THREE.Material & { opacity: number; transparent: boolean };
    mat.transparent = true;
    mat.opacity = 0.06 + telemetry.capillary * 0.16;
    if (gridRef.current) gridRef.current.position.y = -0.98 + telemetry.capillary * 0.06;
  });

  return <primitive ref={gridRef} object={grid} />;
}

// Soft pool of light on the grow floor beneath the genome — grounds the scene.
function FloorGlow() {
  const matRef = useRef<THREE.MeshBasicMaterial>(null);

  const texture = useMemo(() => {
    const s = 256;
    const c = document.createElement("canvas");
    c.width = c.height = s;
    const ctx = c.getContext("2d")!;
    const g = ctx.createRadialGradient(s / 2, s / 2, 0, s / 2, s / 2, s / 2);
    g.addColorStop(0, "rgba(120,255,190,0.9)");
    g.addColorStop(0.4, "rgba(70,255,150,0.28)");
    g.addColorStop(1, "rgba(0,0,0,0)");
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, s, s);
    const tex = new THREE.CanvasTexture(c);
    tex.needsUpdate = true;
    return tex;
  }, []);

  useFrame(() => {
    if (matRef.current) matRef.current.opacity = 0.4 + telemetry.capillary * 0.4;
  });

  return (
    <mesh position={[0, -0.97, 0]} rotation={[-Math.PI / 2, 0, 0]}>
      <planeGeometry args={[16, 16]} />
      <meshBasicMaterial
        ref={matRef}
        map={texture}
        transparent
        depthWrite={false}
        blending={THREE.AdditiveBlending}
        toneMapped={false}
        opacity={0.5}
      />
    </mesh>
  );
}

function CapillaryLight() {
  const a = useRef<THREE.PointLight>(null);
  const b = useRef<THREE.PointLight>(null);
  useFrame(() => {
    const c = telemetry.capillary;
    if (a.current) a.current.intensity = 6 + c * 30;
    if (b.current) b.current.intensity = 6 + (1 - c) * 30;
  });
  return (
    <>
      <pointLight ref={a} position={[-3, 5, 2]} color={PALETTE.chlorophyll} intensity={12} distance={22} />
      <pointLight ref={b} position={[3, -2, -2]} color={PALETTE.ledMagenta} intensity={12} distance={22} />
    </>
  );
}

// Slow, small camera breath around the framing point so the stage feels alive.
function CameraRig() {
  const { camera } = useThree();
  const base = useMemo(() => camera.position.clone(), [camera]);
  const target = useMemo(() => new THREE.Vector3(0, 0.4, 0), []);
  useFrame((state) => {
    const t = state.clock.elapsedTime;
    camera.position.x = base.x + Math.sin(t * 0.12) * 0.5;
    camera.position.y = base.y + Math.sin(t * 0.09 + 1.3) * 0.28;
    camera.position.z = base.z + Math.cos(t * 0.12) * 0.5;
    camera.lookAt(target);
  });
  return null;
}

export function IsometricGrid() {
  return (
    <Canvas
      orthographic
      camera={{ position: [8, 6.6, 8], zoom: 46, near: 0.1, far: 100 }}
      gl={{ antialias: true, alpha: false }}
      onCreated={({ gl, scene, camera }) => {
        gl.setClearColor(new THREE.Color(PALETTE.background), 1);
        scene.fog = new THREE.Fog(new THREE.Color(PALETTE.fog).getHex(), 15, 32);
        camera.lookAt(0, 0.4, 0);
      }}
    >
      <ambientLight intensity={0.18} color={"#20302a"} />
      <CapillaryLight />
      <FloorGlow />
      <IsoFloor />
      <CannabisLeaves />
      <DNAHelix />
      <Trichomes />
      <NodeGraph />
      <CameraRig />

      <EffectComposer multisampling={0}>
        {/* Only true HDR highlights bloom — darks stay crisp. */}
        <Bloom
          intensity={0.95}
          luminanceThreshold={0.5}
          luminanceSmoothing={0.72}
          mipmapBlur
          radius={0.78}
        />
        {/* Faint lab-scope character. */}
        <ChromaticAberration
          blendFunction={BlendFunction.NORMAL}
          offset={new THREE.Vector2(0.0006, 0.0009)}
          radialModulation={false}
          modulationOffset={0}
        />
        <Vignette eskil={false} offset={0.28} darkness={0.86} />
        <Noise premultiply blendFunction={BlendFunction.OVERLAY} opacity={0.03} />
        {/* Edge anti-alias — the composer bypasses canvas MSAA. */}
        <SMAA />
      </EffectComposer>
    </Canvas>
  );
}
