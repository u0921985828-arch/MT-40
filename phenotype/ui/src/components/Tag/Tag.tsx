//==============================================================================
//  Tag.tsx — neon pill badge (section tags, library labels).
//
//  Presentation-only. Extends the native <span> so callers pass children,
//  title, aria-*, etc. freely (Open/Closed); the only addition is `tone`.
//==============================================================================

import type { ComponentProps } from "react";
import type { Tone } from "../../design/tokens";
import "./Tag.css";

export interface TagProps extends ComponentProps<"span"> {
  tone?: Tone;
}

export function Tag({ tone = "chlorophyll", className, ...rest }: TagProps) {
  return <span className={`tag tag--${tone}${className ? ` ${className}` : ""}`} {...rest} />;
}
