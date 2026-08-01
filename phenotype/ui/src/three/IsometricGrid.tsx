//==============================================================================
//  IsometricGrid.tsx
//
//  Orthographic (isometric) genome stage: a breathing substrate grid, crossed
//  cannabis fan leaves, the diploid DNA double helix, drifting trichomes, and a
//  ring of expression loci — all driven by live backend telemetry. A DOM
//  genotype read-out (drei Html) overlays a strain-style label.
//==============================================================================

import { useRef } from "react";
import { Canvas, useFrame } from "@react-three/fiber";
import { Html } from "@react-three/drei";
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
    new THREE.GridHelper(GRID_HALF * 2, GRID_HALF * 4, PALETTE.chlorophyll, PALETTE.grid),
  ).current;

  useFrame(() => {
    const mat = grid.material as THREE.Material & { opacity: number; transparent: boolean };
    mat.transparent = true;
    mat.opacity = 0.22 + telemetry.capillary * 0.35;
    if (gridRef.current) gridRef.current.position.y = -0.62 + telemetry.capillary * 0.08;
  });

  return <primitive ref={gridRef} object={grid} />;
}

function CapillaryLight() {
  const lightRef = useRef<THREE.PointLight>(null);
  useFrame(() => {
    if (lightRef.current) lightRef.current.intensity = 8 + telemetry.capillary * 24;
  });
  return <pointLight ref={lightRef} position={[0, 6, 0]} color={PALETTE.ledMagenta} intensity={12} />;
}

function GenotypeTag() {
  const grains = usePhenotypeStore((s) => s.activeGrains);
  return (
    <Html position={[0, 4.4, 0]} center distanceFactor={10} zIndexRange={[10, 0]}>
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
      camera={{ position: [8, 7, 8], zoom: 46, near: 0.1, far: 100 }}
      gl={{ antialias: true, alpha: false }}
      onCreated={({ gl, camera }) => {
        gl.setClearColor(new THREE.Color(PALETTE.background), 1);
        camera.lookAt(0, 0.6, 0);
      }}
    >
      <ambientLight intensity={0.75} color={"#ffffff"} />
      <directionalLight position={[5, 10, 2]} intensity={1.1} color={PALETTE.chlorophyll} />
      <CapillaryLight />
      <IsoFloor />
      <CannabisLeaves />
      <DNAHelix />
      <Trichomes />
      <NodeGraph />
      <GenotypeTag />
    </Canvas>
  );
}
