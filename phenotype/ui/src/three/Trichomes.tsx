//==============================================================================
//  Trichomes.tsx
//
//  Drifting resin-gland particles — the frosty trichome haze of a mature
//  cannabis flower. A single Points cloud rises slowly and wraps; overall
//  brightness tracks live grain activity so the plant "resinates" as the
//  granular cloud thickens.
//==============================================================================

import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { PALETTE, TRICHOMES, GLOW } from "./theme";
import { telemetry } from "../store/usePhenotypeStore";

const baseGreen = new THREE.Color(PALETTE.chlorophyll).multiplyScalar(GLOW.trichome);
const baseMag = new THREE.Color(PALETTE.ledMagenta).multiplyScalar(GLOW.trichome);

export function Trichomes() {
  const pointsRef = useRef<THREE.Points>(null);

  const geometry = useMemo(() => {
    const g = new THREE.BufferGeometry();
    const pos = new Float32Array(TRICHOMES.count * 3);
    for (let i = 0; i < TRICHOMES.count; i++) {
      const r = Math.sqrt(Math.random()) * TRICHOMES.radius;
      const a = Math.random() * Math.PI * 2;
      pos[i * 3] = Math.cos(a) * r;
      pos[i * 3 + 1] = Math.random() * TRICHOMES.height - TRICHOMES.height * 0.5;
      pos[i * 3 + 2] = Math.sin(a) * r;
    }
    g.setAttribute("position", new THREE.BufferAttribute(pos, 3));
    return g;
  }, []);

  const material = useMemo(
    () =>
      new THREE.PointsMaterial({
        color: baseGreen.clone(),
        size: 0.07,
        sizeAttenuation: true,
        transparent: true,
        opacity: 0.55,
        depthWrite: false,
        blending: THREE.AdditiveBlending,
        toneMapped: false,
      }),
    [],
  );

  useFrame((_, delta) => {
    const pts = pointsRef.current;
    if (!pts) return;
    const attr = geometry.getAttribute("position") as THREE.BufferAttribute;
    const arr = attr.array as Float32Array;
    const top = TRICHOMES.height * 0.5;
    for (let i = 1; i < arr.length; i += 3) {
      arr[i] += delta * TRICHOMES.rise;
      if (arr[i] > top) arr[i] -= TRICHOMES.height;
    }
    attr.needsUpdate = true;

    // Resin density from grain activity (0..~48 grains -> 0..1).
    const density = Math.min(1, telemetry.activeGrains / 48);
    material.opacity = 0.3 + density * 0.55;
    material.color.copy(baseGreen).lerp(baseMag, 0.5 * telemetry.capillary);
  });

  return <points ref={pointsRef} geometry={geometry} material={material} />;
}
