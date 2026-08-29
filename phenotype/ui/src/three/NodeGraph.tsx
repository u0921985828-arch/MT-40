//==============================================================================
//  NodeGraph.tsx
//
//  The expression-loci halo: a clean, closed orbital ring around the genome.
//  Nodes sit at a single height (a real ring, not scattered blobs) and pulse
//  their FFT band; a continuous smooth tube threads all of them into one glowing
//  circlet that breathes with the capillary phase. Clicking a node scrubs the
//  granular `position`.
//==============================================================================

import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { PALETTE, NODE_COUNT } from "./theme";
import { telemetry, usePhenotypeStore } from "../store/usePhenotypeStore";

const RING_RADIUS = 4.15;
const RING_Y = 0.05;

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

  useFrame((state) => {
    const { fft } = telemetry;
    const bin = Math.min(fft.length - 1, Math.floor(node.band * fft.length));
    const energy = fft[bin] ?? 0;
    const mesh = meshRef.current;
    if (!mesh) return;
    // Arp signature: when the arpeggiator is on, the ring pulses at its rate.
    const P = usePhenotypeStore.getState().params;
    const pulse = P.arpOn > 0.5
      ? 0.6 + 0.4 * Math.sin(state.clock.elapsedTime * (0.5 + P.arpRate * 7) * Math.PI * 2)
      : 1;
    const s = (0.13 + energy * 0.32) * (0.75 + 0.25 * pulse);
    mesh.scale.setScalar(s);
    const mat = mesh.material as THREE.MeshStandardMaterial;
    mat.emissiveIntensity = (0.4 + energy * 2.6) * pulse;
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

// A closed, smooth ring tube through all nodes — one quiet glowing circlet.
function RingHalo({ nodes }: { nodes: NodeDef[] }) {
  const meshRef = useRef<THREE.Mesh>(null);

  const geo = useMemo(() => {
    const curve = new THREE.CatmullRomCurve3(nodes.map((n) => n.position.clone()), true, "catmullrom", 0.5);
    return new THREE.TubeGeometry(curve, nodes.length * 12, 0.018, 8, true);
  }, [nodes]);

  const mat = useMemo(
    () => new THREE.MeshBasicMaterial({
      color: new THREE.Color(PALETTE.chlorophyll).multiplyScalar(1.1),
      transparent: true,
      opacity: 0.5,
      toneMapped: false,
      blending: THREE.AdditiveBlending,
    }),
    [],
  );

  useFrame((state) => {
    const m = meshRef.current;
    if (!m) return;
    const P = usePhenotypeStore.getState().params;
    const pulse = P.arpOn > 0.5
      ? 0.6 + 0.4 * Math.sin(state.clock.elapsedTime * (0.5 + P.arpRate * 7) * Math.PI * 2)
      : 1;
    (m.material as THREE.MeshBasicMaterial).opacity = (0.32 + telemetry.capillary * 0.5) * pulse;
  });

  return <mesh ref={meshRef} geometry={geo} material={mat} />;
}

export function NodeGraph() {
  const nodes = useMemo<NodeDef[]>(() => {
    const out: NodeDef[] = [];
    for (let i = 0; i < NODE_COUNT; i++) {
      const a = (i / NODE_COUNT) * Math.PI * 2;
      out.push({
        position: new THREE.Vector3(Math.cos(a) * RING_RADIUS, RING_Y, Math.sin(a) * RING_RADIUS),
        band: i / NODE_COUNT,
        chromosome: (i % 2) as 0 | 1,
      });
    }
    return out;
  }, []);

  return (
    <group>
      <RingHalo nodes={nodes} />
      {nodes.map((n, i) => (
        <Node key={`n${i}`} node={n} index={i} />
      ))}
    </group>
  );
}
