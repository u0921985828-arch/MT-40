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
import { Html } from "@react-three/drei";
import { EffectComposer, Bloom, Vignette } from "@react-three/postprocessing";
import * as THREE from "three";
import { NodeGraph } from "./NodeGraph";
import { DNAHelix } from "./DNAHelix";
import { Trichomes } from "./Trichomes";
import { CannabisLeaves } from "./CannabisLeaf";
import { PALETTE, GRID_HALF } from "./theme";
import { telemetry, usePhenotypeStore } from "../store/usePhenotypeStore";

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

function GenotypeTag() {
  const grains = usePhenotypeStore((s) => s.activeGrains);
  return (
    <Html position={[0, 5.4, 0]} center distanceFactor={11} zIndexRange={[10, 0]}>
      <div className="ph-geno">
        <span className="ph-geno__id">GENOTYPE · A×B</span>
        <span className="ph-geno__meta">
          <span style={{ color: PALETTE.chlorophyll }}>◆A</span>
          <span style={{ color: PALETTE.ledMagenta }}>◆B</span>
          expr {grains}
        </span>
      </div>
    </Html>
  );
}

export function IsometricGrid() {
  return (
    <Canvas
      orthographic
      camera={{ position: [8, 6.5, 8], zoom: 44, near: 0.1, far: 100 }}
      gl={{ antialias: true, alpha: false }}
      onCreated={({ gl, scene, camera }) => {
        gl.setClearColor(new THREE.Color(PALETTE.background), 1);
        scene.fog = new THREE.Fog(new THREE.Color(PALETTE.fog).getHex(), 14, 30);
        camera.lookAt(0, 0.8, 0);
      }}
    >
      <ambientLight intensity={0.18} color={"#20302a"} />
      <CapillaryLight />
      <IsoFloor />
      <CannabisLeaves />
      <DNAHelix />
      <Trichomes />
      <NodeGraph />
      <GenotypeTag />

      <EffectComposer>
        <Bloom
          intensity={1.15}
          luminanceThreshold={0.18}
          luminanceSmoothing={0.9}
          mipmapBlur
          radius={0.85}
        />
        <Vignette eskil={false} offset={0.25} darkness={0.85} />
      </EffectComposer>
    </Canvas>
  );
}
