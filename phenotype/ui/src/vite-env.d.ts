/// <reference types="vite/client" />

// Type surface for the vendored JUCE frontend helper (plain JS, no bundled
// types). Only the members Phenotype consumes are declared.
declare module "../juce/index.js" {
  export function getNativeFunction(
    name: string,
  ): (...args: unknown[]) => Promise<unknown>;
}
declare module "*/juce/index.js" {
  export function getNativeFunction(
    name: string,
  ): (...args: unknown[]) => Promise<unknown>;
}
