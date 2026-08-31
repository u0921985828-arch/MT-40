//==============================================================================
//  IconButton.tsx — square glass control button (nav arrows, import, …).
//
//  Extends the native <button> (so onClick, disabled, aria-label, title all
//  pass through); adds a `tone` accent. Defaults type to "button" so it never
//  submits a surrounding form by accident.
//==============================================================================

import type { ComponentProps } from "react";
import type { Tone } from "../../design/tokens";
import "./IconButton.css";

export interface IconButtonProps extends ComponentProps<"button"> {
  tone?: Tone;
}

export function IconButton({ tone = "chlorophyll", className, type = "button", ...rest }: IconButtonProps) {
  return (
    <button
      type={type}
      className={`icon-btn icon-btn--${tone}${className ? ` ${className}` : ""}`}
      {...rest}
    />
  );
}
