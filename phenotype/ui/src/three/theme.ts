//==============================================================================
//  theme.ts — Phenotype grow-lab dark neon system + genetic/cannabis motif.
//==============================================================================

export const PALETTE = {
  background: "#07090A", // deep grow-tent black
  fog: "#0A0F0C",
  ink: "#EAF7EE",
  chlorophyll: "#00FF6A", // accent — chromosome A / growth (slightly warm green)
  ledMagenta: "#FF2BD6", // accent — chromosome B / signal
  grid: "#0E241A",
  leafA: "#00FF6A",
  leafB: "#FF2BD6",
} as const;

// HDR emissive multipliers so bloom blows the accents into real glow.
export const GLOW = {
  nucleusA: 2.6,
  nucleusB: 2.6,
  rung: 2.0,
  trichome: 3.0,
  leaf: 1.8,
} as const;

export const NODE_COUNT = 8; // expression loci orbiting the genome
export const GRID_HALF = 7; // isometric floor extends -HALF..HALF

// --- DNA double helix -------------------------------------------------------
export const HELIX = {
  pairs: 34,
  radius: 1.15,
  step: 0.28,
  twist: 0.5,
  spin: 0.4, // rad/s idle rotation
} as const;

// --- Trichomes (drifting resin particles) -----------------------------------
export const TRICHOMES = {
  count: 900,
  radius: 6.5,
  height: 9.0,
  rise: 0.4,
} as const;
