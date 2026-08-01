//==============================================================================
//  IsometricGrid.tsx
//
//  Orthographic (isometric) scene wrapper: fixed 35.264° / 45° camera, a
//  chlorophyll/magenta wire floor, lighting, and the node graph. The floor
//  breathes with the capillary phase so the whole substrate reads as "flowing".
//==============================================================================

import { useRef } from "react";
import { Canvas, useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { NodeGraph } from "./NodeGraph";
import { PALETTE, GRID_HALF } from "./theme";
import { telemetry } from "../store/usePhenotypeStore";

function IsoFloor() {
  const gridRef = useRef<THREE.GridHelper>(null);
  const grid = useRef(
    new THREE.GridHelper(GRID_HALF * 2, GRID_HALF * 4, PALETTE.chlorophyll, PALETTE.grid),
  ).current;

  useFrame(() => {
    const mat = grid.material as THREE.Material & { opacity: number; transparent: boolean };
    mat.transparent = true;
    mat.opacity = 0.25 + telemetry.capillary * 0.4;
    if (gridRef.current) gridRef.current.position.y = -0.6 + telemetry.capillary * 0.1;
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

export function IsometricGrid() {
  return (
    <Canvas
      orthographic
      camera={{ position: [8, 8, 8], zoom: 55, near: 0.1, far: 100 }}
      gl={{ antialias: true, alpha: false }}
      onCreated={({ gl, camera }) => {
        gl.setClearColor(new THREE.Color(PALETTE.background), 1);
        camera.lookAt(0, 0, 0);
      }}
    >
      <ambientLight intensity={0.7} color={"#ffffff"} />
      <directionalLight position={[5, 10, 2]} intensity={1.1} color={PALETTE.chlorophyll} />
      <CapillaryLight />
      <IsoFloor />
      <NodeGraph />
    </Canvas>
  );
}
