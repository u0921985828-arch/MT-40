//==============================================================================
//  Trichomes.tsx
//
//  Realistic resin-gland haze. Instead of a uniform rising column, the glands
//  cluster in a rounded volume hugging the plant, each one a soft glistening
//  bead that twinkles (living shimmer) and carries a maturity tint — most
//  translucent green-white, some amber, a few magenta-frost — the way real
//  trichomes ripen clear -> milky -> amber. They drift and sway rather than
//  march upward, and thicken with grain activity / grain-density preset.
//==============================================================================

import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { PALETTE, TRICHOMES, GLOW } from "./theme";
import { telemetry, usePhenotypeStore } from "../store/usePhenotypeStore";

// Soft round "resin gland" sprite — a radial glow with a bright wet core.
function makeGlowSprite(): THREE.Texture {
  const s = 64;
  const c = document.createElement("canvas");
  c.width = c.height = s;
  const ctx = c.getContext("2d")!;
  const g = ctx.createRadialGradient(s / 2, s / 2, 0, s / 2, s / 2, s / 2);
  g.addColorStop(0, "rgba(255,255,255,1)");
  g.addColorStop(0.22, "rgba(255,255,255,0.75)");
  g.addColorStop(0.5, "rgba(255,255,255,0.28)");
  g.addColorStop(1, "rgba(255,255,255,0)");
  ctx.fillStyle = g;
  ctx.fillRect(0, 0, s, s);
  const tex = new THREE.CanvasTexture(c);
  tex.needsUpdate = true;
  return tex;
}

const frosty = new THREE.Color("#c9ffe6").multiplyScalar(GLOW.trichome * 0.9); // clear/milky
const amber = new THREE.Color("#ffc36b").multiplyScalar(GLOW.trichome * 0.8);  // ripe
const berry = new THREE.Color(PALETTE.ledMagenta).multiplyScalar(GLOW.trichome * 0.7);

export function Trichomes() {
  const pointsRef = useRef<THREE.Points>(null);

  // Base X/Z kept so per-frame sway is a small offset, not a random walk.
  const base = useMemo(() => new Float32Array(TRICHOMES.count * 2), []);

  const geometry = useMemo(() => {
    const g = new THREE.BufferGeometry();
    const pos = new Float32Array(TRICHOMES.count * 3);
    const siz = new Float32Array(TRICHOMES.count);
    const phase = new Float32Array(TRICHOMES.count);
    const col = new Float32Array(TRICHOMES.count * 3);
    for (let i = 0; i < TRICHOMES.count; i++) {
      // Cluster around the plant: denser toward the core, thinning outward and
      // upward, so it reads as resin ON the plant, not a floating cylinder.
      const r = Math.pow(Math.random(), 0.7) * TRICHOMES.radius * 0.62;
      const a = Math.random() * Math.PI * 2;
      const h = -TRICHOMES.height * 0.28 + Math.pow(Math.random(), 1.4) * TRICHOMES.height * 0.85;
      const x = Math.cos(a) * r;
      const z = Math.sin(a) * r;
      pos[i * 3] = x;
      pos[i * 3 + 1] = h;
      pos[i * 3 + 2] = z;
      base[i * 2] = x;
      base[i * 2 + 1] = z;
      siz[i] = 0.45 + Math.random() * Math.random() * 2.0; // mostly fine, few fat
      phase[i] = Math.random() * Math.PI * 2;

      // Maturity tint: clear/milky majority, some amber, a few berry.
      const m = Math.random();
      const c = m < 0.62 ? frosty : m < 0.86 ? amber : berry;
      col[i * 3] = c.r; col[i * 3 + 1] = c.g; col[i * 3 + 2] = c.b;
    }
    g.setAttribute("position", new THREE.BufferAttribute(pos, 3));
    g.setAttribute("aSize", new THREE.BufferAttribute(siz, 1));
    g.setAttribute("aPhase", new THREE.BufferAttribute(phase, 1));
    g.setAttribute("color", new THREE.BufferAttribute(col, 3));
    return g;
  }, [base]);

  const material = useMemo(() => {
    const m = new THREE.PointsMaterial({
      map: makeGlowSprite(),
      size: 0.17,
      sizeAttenuation: true,
      transparent: true,
      opacity: 0.5,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      vertexColors: true,
      toneMapped: false,
    });
    // Per-particle size + a living twinkle carried into the fragment alpha.
    m.onBeforeCompile = (shader) => {
      shader.uniforms.uTime = { value: 0 };
      shader.vertexShader =
        "attribute float aSize;\nattribute float aPhase;\nuniform float uTime;\nvarying float vTw;\n" +
        shader.vertexShader
          .replace(
            "gl_PointSize = size;",
            "vTw = 0.55 + 0.45 * sin(uTime * 2.2 + aPhase);\n  gl_PointSize = size * aSize * (0.7 + 0.5 * vTw);",
          );
      shader.fragmentShader =
        "varying float vTw;\n" +
        shader.fragmentShader.replace(
          "vec4 diffuseColor = vec4( diffuse, opacity );",
          "vec4 diffuseColor = vec4( diffuse, opacity * (0.35 + 0.65 * vTw) );",
        );
      m.userData.shader = shader;
    };
    return m;
  }, []);

  useFrame((state, delta) => {
    const pts = pointsRef.current;
    if (!pts) return;
    const t = state.clock.elapsedTime;
    const attr = geometry.getAttribute("position") as THREE.BufferAttribute;
    const arr = attr.array as Float32Array;
    const top = TRICHOMES.height * 0.62;
    const bot = -TRICHOMES.height * 0.3;
    for (let i = 0; i < TRICHOMES.count; i++) {
      const j = i * 3;
      // Slow buoyant rise + gentle turbulent sway around the base position.
      arr[j + 1] += delta * TRICHOMES.rise * 0.6;
      if (arr[j + 1] > top) arr[j + 1] = bot;
      const bx = base[i * 2]!, bz = base[i * 2 + 1]!;
      arr[j] = bx + Math.sin(t * 0.5 + bz * 1.2 + i) * 0.11;
      arr[j + 2] = bz + Math.cos(t * 0.42 + bx * 1.2 + i * 0.7) * 0.11;
    }
    attr.needsUpdate = true;

    if (material.userData.shader) material.userData.shader.uniforms.uTime.value = t;

    // Preset signature: grain density/size set a resin floor + gland size, so a
    // dense preset frosts up even in silence; stereo width spreads the cloud.
    const P = usePhenotypeStore.getState().params;
    const live = Math.min(1, telemetry.activeGrains / 48);
    const density = Math.max(live, P.grainDensity * 0.85);
    material.opacity = 0.24 + density * 0.55;
    material.size = 0.17 * (0.55 + P.grainSize * 1.3);

    const spread = 0.7 + P.stereoWidth * 1.0;
    pts.scale.set(spread, 1, spread);
  });

  return <points ref={pointsRef} geometry={geometry} material={material} />;
}
