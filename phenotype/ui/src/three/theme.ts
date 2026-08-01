//==============================================================================
//  theme.ts — C40 visual system (spec §4).
//==============================================================================

export const PALETTE = {
  background: "#F4F4F4",
  ink: "#222222",
  chlorophyll: "#00FF00", // accent — chromosome A / growth
  ledMagenta: "#FF00FF", // accent — chromosome B / signal
  grid: "#D8D8D8",
} as const;

export const NODE_COUNT = 8; // interactive nodes around the isometric ring
export const GRID_HALF = 5; // isometric floor extends -HALF..HALF
