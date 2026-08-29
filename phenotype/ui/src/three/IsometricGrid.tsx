//==============================================================================
//  IsometricGrid.tsx
//
//  Orthographic grow-lab stage rendered dark with real bloom: the diploid DNA
//  helix and trichome haze glow like bioluminescent LEDs against a near-black
//  substrate, crossed cannabis fan leaves backlit in neon. All motion is driven
//  by live backend telemetry.
//==============================================================================

import { useRef } from "react";
import { Canvas, useFrame } from "@react-three/fiber";
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
    mat.opacity = 0.12 + telemetry.capillary * 0.28;
    if (gridRef.current) gridRef.current.position.y = -0.9 + telemetry.capillary * 0.08;
  });

  return <primitive ref={gridRef} object={grid} />;
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

export function IsometricGrid() {
  return (
    <Canvas
      orthographic
      camera={{ position: [8, 6.5, 8], zoom: 47, near: 0.1, far: 100 }}
      gl={{ antialias: true, alpha: false }}
      onCreated={({ gl, scene, camera }) => {
        gl.setClearColor(new THREE.Color(PALETTE.background), 1);
        scene.fog = new THREE.Fog(new THREE.Color(PALETTE.fog).getHex(), 14, 30);
        camera.lookAt(0, 0.5, 0);
      }}
    >
      <ambientLight intensity={0.18} color={"#20302a"} />
      <CapillaryLight />
      <IsoFloor />
      <CannabisLeaves />
      <DNAHelix />
      <Trichomes />
      <NodeGraph />

      <EffectComposer multisampling={0}>
        {/* Only true HDR highlights bloom now — darks stay crisp. */}
        <Bloom
          intensity={0.85}
          luminanceThreshold={0.55}
          luminanceSmoothing={0.7}
          mipmapBlur
          radius={0.72}
        />
        {/* Faint lab-scope character. */}
        <ChromaticAberration
          blendFunction={BlendFunction.NORMAL}
          offset={new THREE.Vector2(0.0006, 0.0009)}
          radialModulation={false}
          modulationOffset={0}
        />
        <Vignette eskil={false} offset={0.32} darkness={0.82} />
        <Noise premultiply blendFunction={BlendFunction.OVERLAY} opacity={0.035} />
        {/* Edge anti-alias — the composer bypasses canvas MSAA. */}
        <SMAA />
      </EffectComposer>
    </Canvas>
  );
}
