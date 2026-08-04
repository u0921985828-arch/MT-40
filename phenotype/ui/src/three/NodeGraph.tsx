//==============================================================================
//  NodeGraph.tsx
//
//  The interactive isometric node ring. Each node is a genotype locus; its
//  scale/emissive pulse maps an FFT band, and splines connect adjacent nodes
//  alternating chromosome A/B. Clicking a node scrubs the granular `position`.
//==============================================================================

import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { SplineLink } from "./SplineLink";
import { PALETTE, NODE_COUNT } from "./theme";
import { telemetry, usePhenotypeStore } from "../store/usePhenotypeStore";

interface NodeDef {
  position: THREE.Vector3;
  band: number;
  chromosome: 0 | 1;
}

function Node({ node, index }: { node: NodeDef; index: number }) {
  const meshRef = useRef<THREE.Mesh>(null);
  const setParam = usePhenotypeStore((s) => s.setParam);

  const color = useMemo(
    () => new THREE.Color(node.chromosome === 0 ? PALETTE.chlorophyll : PALETTE.ledMagenta),
    [node.chromosome],
  );

  useFrame(() => {
    const { fft } = telemetry;
    const bin = Math.min(fft.length - 1, Math.floor(node.band * fft.length));
    const energy = fft[bin] ?? 0;
    const mesh = meshRef.current;
    if (!mesh) return;
    const s = 0.18 + energy * 0.4;
    mesh.scale.setScalar(s);
    const mat = mesh.material as THREE.MeshStandardMaterial;
    mat.emissiveIntensity = 0.3 + energy * 2.5;
  });

  return (
    <mesh
      ref={meshRef}
      position={node.position}
      onPointerDown={() => setParam("position", index / NODE_COUNT)}
    >
      <icosahedronGeometry args={[1, 1]} />
      <meshStandardMaterial
        color={"#000000"}
        emissive={color}
        emissiveIntensity={1.6}
        toneMapped={false}
        roughness={0.4}
        metalness={0.1}
      />
    </mesh>
  );
}

export function NodeGraph() {
  const nodes = useMemo<NodeDef[]>(() => {
    const out: NodeDef[] = [];
    const radius = 4.3;
    for (let i = 0; i < NODE_COUNT; i++) {
      const a = (i / NODE_COUNT) * Math.PI * 2;
      out.push({
        position: new THREE.Vector3(Math.cos(a) * radius, 0.1, Math.sin(a) * radius),
        band: i / NODE_COUNT,
        chromosome: (i % 2) as 0 | 1,
      });
    }
    return out;
  }, []);

  const links = useMemo(() => {
    const out: { from: THREE.Vector3; to: THREE.Vector3; band: number; chromosome: 0 | 1 }[] = [];
    for (let i = 0; i < nodes.length; i++) {
      const a = nodes[i]!;
      const b = nodes[(i + 1) % nodes.length]!;
      out.push({ from: a.position, to: b.position, band: a.band, chromosome: a.chromosome });
    }
    // Diploid cross links (A locus <-> opposite B locus).
    const half = Math.floor(nodes.length / 2);
    for (let i = 0; i < half; i++) {
      const a = nodes[i]!;
      const b = nodes[i + half]!;
      out.push({ from: a.position, to: b.position, band: (a.band + b.band) * 0.5, chromosome: 1 });
    }
    return out;
  }, [nodes]);

  return (
    <group>
      {links.map((l, i) => (
        <SplineLink key={`l${i}`} from={l.from} to={l.to} band={l.band} chromosome={l.chromosome} />
      ))}
      {nodes.map((n, i) => (
        <Node key={`n${i}`} node={n} index={i} />
      ))}
    </group>
  );
}
