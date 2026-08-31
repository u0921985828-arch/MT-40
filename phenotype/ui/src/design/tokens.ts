//==============================================================================
//  tokens.ts — central design tokens for TS/JS consumers.
//
//  styles.css (:root) stays the runtime source of truth for stylesheet values;
//  this module mirrors the same decisions for code that needs them in
//  TypeScript — accent selection inside the Knob, and the tone→identity shared
//  by the UI primitives (Tag, IconButton). Keep the two in sync.
//==============================================================================

import { PALETTE } from "../three/theme";

/** Semantic accent identity shared by the primitives and controls. */
export type Tone = "chlorophyll" | "magenta" | "ink";

/** Accent colour for a tone — matches CSS --chlorophyll / --magenta / --ink. */
export const toneColor: Record<Tone, string> = {
  chlorophyll: PALETTE.chlorophyll,
  magenta: PALETTE.ledMagenta,
  ink: PALETTE.ink,
};
