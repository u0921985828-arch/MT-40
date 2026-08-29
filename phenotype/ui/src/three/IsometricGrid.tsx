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
import { PALETTE } from "./theme";
import { telemetry, usePhenotypeStore } from "../store/usePhenotypeStore";

// The scene's only two pigments. The illumination is their emulsion, weighted by
// cross-synth (oil & water — the dominant chromosome lights the room), so every
// glow belongs to the same family; nothing floats an alien hue.
const chlA = new THREE.Color(PALETTE.chlorophyll);
const chlB = new THREE.Color(PALETTE.ledMagenta);
const emul = new THREE.Color();

// Lava-lamp emulsion pool on the grow floor: green (A) and magenta (B) metaballs
// drift, merge and split. Cross-synth sets each phase's mass — the dominant
// chromosome is the continuous medium, the recessive one floats as blobs.
const LAVA_FRAG = /* glsl */ `
  precision highp float;
  uniform float uTime, uCross, uOpacity;
  uniform vec3 uColA, uColB;
  varying vec2 vUv;
  void main() {
    vec2 p = vUv * 2.0 - 1.0;
    float ga = 0.0, gb = 0.0;
    for (int i = 0; i < 7; i++) {
      float fi = float(i);
      vec2 c = vec2(
        sin(uTime * 0.16 + fi * 1.7) * 0.62 + sin(uTime * 0.07 + fi) * 0.2,
        cos(uTime * 0.13 + fi * 2.3) * 0.62 + cos(uTime * 0.05 + fi * 1.3) * 0.2
      );
      float d2 = dot(p - c, p - c) + 0.015;
      float field = 0.10 / d2;
      if (mod(fi, 2.0) < 0.5) ga += field * (0.3 + 1.5 * (1.0 - uCross));
      else                    gb += field * (0.3 + 1.5 * uCross);
    }
    float tot = ga + gb;
    vec3 col = (uColA * ga + uColB * gb) / max(tot, 0.001);
    float body = smoothstep(0.55, 1.7, tot);
    // Fade fully to zero well inside the plane so its straight edges never show
    // (a soft round pool, never a clipped diagonal).
    float edge = smoothstep(0.9, 0.12, length(p));
    float a = body * edge * uOpacity;
    gl_FragColor = vec4(col * (0.6 + 0.9 * body), a);
  }`;

const LAVA_VERT = /* glsl */ `
  varying vec2 vUv;
  void main() {
    vUv = uv;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }`;

function LavaFloor() {
  const meshRef = useRef<THREE.Mesh>(null);
  const mat = useMemo(
    () => new THREE.ShaderMaterial({
      uniforms: {
        uTime: { value: 0 },
        uCross: { value: 0.5 },
        uOpacity: { value: 0.5 },
        uColA: { value: new THREE.Color(PALETTE.chlorophyll) },
        uColB: { value: new THREE.Color(PALETTE.ledMagenta) },
      },
      vertexShader: LAVA_VERT,
      fragmentShader: LAVA_FRAG,
      transparent: true,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
    }),
    [],
  );

  useFrame((state) => {
    const P = usePhenotypeStore.getState().params;
    const breath = 0.9 + 0.1 * Math.sin(state.clock.elapsedTime * 0.4);
    mat.uniforms.uTime.value = state.clock.elapsedTime;
    mat.uniforms.uCross.value = P.crossBlend;
    mat.uniforms.uOpacity.value = (0.26 + telemetry.capillary * 0.3 + P.filterCutoff * 0.28) * breath;
    if (meshRef.current) {
      // Keep the pool inside the ~15-unit visible width so its glow fades
      // within the frame instead of being clipped at the viewport edge.
      const sc = 0.68 + P.reverbSize * 0.36 + P.reverbMix * 0.12;
      meshRef.current.scale.set(sc, sc, 1);
    }
  });

  return (
    <mesh ref={meshRef} position={[0, -0.97, 0]} rotation={[-Math.PI / 2, 0, 0]} material={mat}>
      <planeGeometry args={[12, 12]} />
    </mesh>
  );
}

function CapillaryLight() {
  const a = useRef<THREE.PointLight>(null);
  const b = useRef<THREE.PointLight>(null);
  useFrame(() => {
    const c = telemetry.capillary;
    const cb = usePhenotypeStore.getState().params.crossBlend;
    // Dominant chromosome throws the stronger light (oil & water).
    if (a.current) a.current.intensity = (6 + c * 30) * (0.35 + 1.3 * (1 - cb));
    if (b.current) b.current.intensity = (6 + (1 - c) * 30) * (0.35 + 1.3 * cb);
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

// Reverb sets the room: more wet/size pulls the fog in for a hazier, deeper
// space; a dry preset opens the stage up.
function AtmosphereRig() {
  const { scene } = useThree();
  useFrame(() => {
    const fog = scene.fog as THREE.Fog | null;
    if (!fog) return;
    const P = usePhenotypeStore.getState().params;
    const wet = Math.min(1, P.reverbMix * 0.7 + P.reverbSize * 0.5);
    fog.near = 16 - wet * 6;
    fog.far = 34 - wet * 12;
    // The haze is a very dark wash of the dominant chromosome, not an alien hue.
    emul.copy(chlA).lerp(chlB, P.crossBlend).multiplyScalar(0.09);
    fog.color.copy(emul);
  });
  return null;
}

export function IsometricGrid() {
  return (
    <Canvas
      orthographic
      camera={{ position: [8, 6.6, 8], zoom: 56, near: 0.1, far: 100 }}
      gl={{ antialias: true, alpha: false }}
      onCreated={({ gl, scene, camera }) => {
        gl.setClearColor(new THREE.Color(PALETTE.background), 1);
        scene.fog = new THREE.Fog(new THREE.Color(PALETTE.fog).getHex(), 15, 32);
        camera.lookAt(0, 0.4, 0);
      }}
    >
      <ambientLight intensity={0.18} color={"#20302a"} />
      <CapillaryLight />
      <LavaFloor />
      <DNAHelix />
      <Trichomes />
      <NodeGraph />
      <CameraRig />
      <AtmosphereRig />

      <EffectComposer multisampling={0}>
        {/* Only true HDR highlights bloom — darks stay crisp. */}
        <Bloom
          intensity={0.72}
          luminanceThreshold={0.5}
          luminanceSmoothing={0.88}
          mipmapBlur
          radius={0.92}
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
