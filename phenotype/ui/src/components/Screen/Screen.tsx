//==============================================================================
//  Screen.tsx — HUD readout screen: a tone-tagged label, a centred title, and
//  a right-aligned meta figure. Presentation only — the caller owns the data
//  and its navigation (Separation of Concerns).
//==============================================================================

import { Tag } from "../Tag/Tag";
import "./Screen.css";

export interface ScreenProps {
  label: string;
  title: string;
  meta?: string | undefined;
}

export function Screen({ label, title, meta }: ScreenProps) {
  return (
    <div className="screen">
      <Tag className="screen__label">{label}</Tag>
      <span className="screen__title">{title}</span>
      <span className="screen__meta">{meta}</span>
    </div>
  );
}
