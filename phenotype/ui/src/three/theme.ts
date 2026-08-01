//==============================================================================
//  theme.ts — C40 visual system + genetic/cannabis motif constants (spec §4).
//==============================================================================

export const PALETTE = {
  background: "#F4F4F4",
  ink: "#222222",
  chlorophyll: "#00FF00", // accent — chromosome A / growth
  ledMagenta: "#FF00FF", // accent — chromosome B / signal
  grid: "#D8D8D8",
  leafA: "#00FF00",
  leafB: "#FF00FF",
} as const;

export const NODE_COUNT = 8; // expression loci orbiting the genome
export const GRID_HALF = 6; // isometric floor extends -HALF..HALF

// --- DNA double helix -------------------------------------------------------
export const HELIX = {
  pairs: 28, // base pairs
  radius: 1.05, // strand radius
  step: 0.26, // vertical spacing per pair
  twist: 0.52, // radians of twist per pair
  spin: 0.35, // rad/s idle rotation
} as const;

// --- Trichomes (drifting resin particles) -----------------------------------
export const TRICHOMES = {
  count: 520,
  radius: 5.5,
  height: 7.0,
  rise: 0.35, // units/s upward drift
} as const;
