//==============================================================================
//  SplineLink.tsx
//
//  A vector spline connecting two isometric nodes. The line pulses (emissive
//  width + colour bias) from live backend telemetry: FFT energy in the link's
//  band drives brightness, the capillary phase drives a travelling highlight.
//==============================================================================

import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { PALETTE } from "./theme";
import { telemetry } from "../store/usePhenotypeStore";

interface SplineLinkProps {
  from: THREE.Vector3;
  to: THREE.Vector3;
  /** FFT bin fraction [0,1) this link listens to. */
  band: number;
  /** 0 = chromosome A (chlorophyll), 1 = chromosome B (magenta). */
  chromosome: 0 | 1;
}

const SEGMENTS = 40;

export function SplineLink({ from, to, band, chromosome }: SplineLinkProps) {
  const lineRef = useRef<THREE.Line>(null);
  const baseColor = useMemo(
    () => new THREE.Color(chromosome === 0 ? PALETTE.chlorophyll : PALETTE.ledMagenta),
    [chromosome],
  );

  // Curved control point lifted along +Y so links read as arcs in ortho view.
  const curve = useMemo(() => {
    const mid = from.clone().add(to).multiplyScalar(0.5);
    mid.y += from.distanceTo(to) * 0.35;
    return new THREE.QuadraticBezierCurve3(from.clone(), mid, to.clone());
  }, [from, to]);

  const geometry = useMemo(() => {
    const pts = curve.getPoints(SEGMENTS);
    return new THREE.BufferGeometry().setFromPoints(pts);
  }, [curve]);

  const material = useMemo(
    () =>
      new THREE.LineBasicMaterial({
        color: baseColor.clone(),
        transparent: true,
        opacity: 0.35,
      }),
    [baseColor],
  );

  useFrame(() => {
    const { fft, capillary } = telemetry;
    const bin = Math.min(fft.length - 1, Math.floor(band * fft.length));
    const energy = fft[bin] ?? 0;

    const mat = material;
    // Brightness tracks band energy; the ink base keeps low signal readable.
    const lift = 0.25 + energy * 0.75;
    mat.opacity = 0.08 + energy * 0.5;   // quieter — ambient ring, not wiring
    mat.color.copy(baseColor).lerp(new THREE.Color(PALETTE.ink), 1 - lift);

    // Capillary phase adds a slow global shimmer synced to the drain/fill.
    mat.color.multiplyScalar(0.7 + 0.3 * capillary);

    if (lineRef.current) lineRef.current.visible = true;
  });

  return (
    // @ts-expect-error r3f line primitive
    <line ref={lineRef} geometry={geometry} material={material} />
  );
}
