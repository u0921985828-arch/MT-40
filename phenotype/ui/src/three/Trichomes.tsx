//==============================================================================
//  Trichomes.tsx
//
//  Botanically-typed resin glands (per Cervantes, Ch.1) instead of flat dots:
//
//    * Capitate-stalked  — the iconic gland: a slender stalk topped by a
//      bulbous glandular head (the bulk of the resin). Majority.
//    * Capitate-sessile  — a head sitting almost flush on the surface, tiny
//      stalk.
//    * Bulbous           — the smallest, a lone micro-head.
//    * Non-glandular      — a bare hair (cystolith), no gland head.
//
//  Heads are instanced glowing beads that twinkle (wet glisten) and ripen
//  clear -> milky -> amber; stalks/hairs are instanced translucent filaments.
//  The field clusters around the plant, drifts, and thickens with grain
//  activity / the grain-density preset.
//==============================================================================

import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { PALETTE, TRICHOMES, GLOW } from "./theme";
import { telemetry, usePhenotypeStore } from "../store/usePhenotypeStore";

// Maturity palette — clear/milky majority, some amber, a few berry (chr. B).
const milky = new THREE.Color("#d6ffe9").multiplyScalar(GLOW.trichome * 0.9);
const amber = new THREE.Color("#ffc46a").multiplyScalar(GLOW.trichome * 0.85);
const berry = new THREE.Color(PALETTE.ledMagenta).multiplyScalar(GLOW.trichome * 0.7);
const stalkCol = new THREE.Color("#bfffe0");

const up = new THREE.Vector3(0, 1, 0);
const quat = new THREE.Quaternion();
const dummy = new THREE.Object3D();

export function Trichomes() {
  const groupRef = useRef<THREE.Group>(null);

  const { heads, stalks, headMat, stalkMat } = useMemo(() => {
    const N = TRICHOMES.count > 700 ? 480 : TRICHOMES.count; // instanced glands
    type G = {
      pos: THREE.Vector3;
      dir: THREE.Vector3;
      stalkLen: number;
      stalkR: number;
      headR: number;
      col: THREE.Color;
      hasHead: boolean;
    };
    const glands: G[] = [];
    for (let i = 0; i < N; i++) {
      // Cluster around the plant (denser toward the core, thinning out/up).
      const rr = Math.pow(Math.random(), 0.7) * TRICHOMES.radius * 0.55;
      const a = Math.random() * Math.PI * 2;
      const h = -TRICHOMES.height * 0.24 + Math.pow(Math.random(), 1.5) * TRICHOMES.height * 0.62;
      const pos = new THREE.Vector3(Math.cos(a) * rr, h, Math.sin(a) * rr);
      // Glands point outward from the plant axis, biased upward — as if growing
      // off a surface in every direction (the frosty coat).
      const d = new THREE.Vector3(pos.x, 0, pos.z).normalize().multiplyScalar(0.5)
        .add(new THREE.Vector3((Math.random() - 0.5) * 0.6, 0.7 + Math.random() * 0.5, (Math.random() - 0.5) * 0.6))
        .normalize();

      const t = Math.random();
      let stalkLen: number, headR: number, hasHead: boolean, stalkR: number;
      if (t < 0.6) {            // capitate-stalked
        stalkLen = 0.08 + Math.random() * 0.09;
        headR = 0.035 + Math.random() * 0.03;
        stalkR = 0.008 + Math.random() * 0.004;
        hasHead = true;
      } else if (t < 0.8) {     // capitate-sessile
        stalkLen = 0.02 + Math.random() * 0.02;
        headR = 0.03 + Math.random() * 0.022;
        stalkR = 0.009;
        hasHead = true;
      } else if (t < 0.9) {     // bulbous (lone micro-head)
        stalkLen = 0.0;
        headR = 0.018 + Math.random() * 0.014;
        stalkR = 0.0;
        hasHead = true;
      } else {                  // non-glandular hair
        stalkLen = 0.11 + Math.random() * 0.1;
        headR = 0.0;
        stalkR = 0.005 + Math.random() * 0.003;
        hasHead = false;
      }

      const m = Math.random();
      const col = !hasHead ? stalkCol : m < 0.6 ? milky : m < 0.85 ? amber : berry;
      glands.push({ pos, dir: d, stalkLen, stalkR, headR, col, hasHead });
    }

    const headList = glands.filter((g) => g.hasHead);
    const stalkList = glands.filter((g) => g.stalkLen > 0.001);

    // --- Heads (bulbous glandular heads) ---
    const headGeo = new THREE.SphereGeometry(1, 8, 8);
    const headMat = new THREE.MeshBasicMaterial({
      transparent: true,
      opacity: 0.9,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      toneMapped: false,
    });
    const phase = new Float32Array(headList.length);
    const heads = new THREE.InstancedMesh(headGeo, headMat, headList.length);
    headList.forEach((g, i) => {
      const head = g.pos.clone().addScaledVector(g.dir, g.stalkLen);
      dummy.position.copy(head);
      dummy.scale.setScalar(g.headR);
      dummy.quaternion.identity();
      dummy.updateMatrix();
      heads.setMatrixAt(i, dummy.matrix);
      heads.setColorAt(i, g.col);
      phase[i] = Math.random() * Math.PI * 2;
    });
    headGeo.setAttribute("aPhase", new THREE.InstancedBufferAttribute(phase, 1));
    heads.instanceMatrix.needsUpdate = true;

    // Living wet-glisten twinkle carried into the head fragment brightness.
    headMat.onBeforeCompile = (shader) => {
      shader.uniforms.uTime = { value: 0 };
      shader.vertexShader =
        "attribute float aPhase;\nuniform float uTime;\nvarying float vTw;\n" +
        shader.vertexShader.replace(
          "#include <begin_vertex>",
          "#include <begin_vertex>\n  vTw = 0.55 + 0.45 * sin(uTime * 2.2 + aPhase);",
        );
      shader.fragmentShader =
        "varying float vTw;\n" +
        shader.fragmentShader.replace(
          "vec4 diffuseColor = vec4( diffuse, opacity );",
          "vec4 diffuseColor = vec4( diffuse, opacity );\n  diffuseColor.rgb *= (0.45 + 0.75 * vTw);",
        );
      headMat.userData.shader = shader;
    };

    // --- Stalks / hairs (translucent filaments) ---
    const stalkGeo = new THREE.CylinderGeometry(0.6, 1.0, 1, 5, 1, true);
    const stalkMat = new THREE.MeshBasicMaterial({
      color: stalkCol.clone().multiplyScalar(0.5),
      transparent: true,
      opacity: 0.28,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      toneMapped: false,
      side: THREE.DoubleSide,
    });
    const stalks = new THREE.InstancedMesh(stalkGeo, stalkMat, stalkList.length);
    stalkList.forEach((g, i) => {
      quat.setFromUnitVectors(up, g.dir);
      const mid = g.pos.clone().addScaledVector(g.dir, g.stalkLen * 0.5);
      dummy.position.copy(mid);
      dummy.quaternion.copy(quat);
      dummy.scale.set(g.stalkR, g.stalkLen, g.stalkR);
      dummy.updateMatrix();
      stalks.setMatrixAt(i, dummy.matrix);
    });
    stalks.instanceMatrix.needsUpdate = true;

    return { heads, stalks, headMat, stalkMat };
  }, []);

  useFrame((state) => {
    const t = state.clock.elapsedTime;
    if (headMat.userData.shader) headMat.userData.shader.uniforms.uTime.value = t;

    // Slow buoyant drift + sway so the frost feels alive without per-instance cost.
    const g = groupRef.current;
    if (g) {
      g.rotation.y = Math.sin(t * 0.08) * 0.12;
      g.position.y = Math.sin(t * 0.3) * 0.06;
    }

    // Preset signature: grain density/size set the resin floor and gland scale;
    // stereo width spreads the coat.
    const P = usePhenotypeStore.getState().params;
    const live = Math.min(1, telemetry.activeGrains / 48);
    const density = Math.max(live, P.grainDensity * 0.85);
    headMat.opacity = 0.35 + density * 0.55;
    stalkMat.opacity = 0.14 + density * 0.28;
    if (g) {
      const spread = 0.7 + P.stereoWidth * 1.0;
      const grow = 0.75 + P.grainSize * 0.7;
      g.scale.set(spread, grow, spread);
    }
  });

  return (
    <group ref={groupRef}>
      <primitive object={stalks} />
      <primitive object={heads} />
    </group>
  );
}
