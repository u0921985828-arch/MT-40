//==============================================================================
//  PresetBrowser.tsx — full in-UI preset + library browser.
//
//  Turns the flat prev/next stepper into a real instrument browser: the whole
//  roster (factory + imported DLC) fetched once via library("list"), grouped by
//  library, searchable, and click-to-load. Bank management (Import / Rescan)
//  lives in the header. The list is virtualised so tens of thousands of DLC
//  presets scroll smoothly.
//==============================================================================

import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { juceIntegration } from "../../bridge/juceIntegration";
import { Tag, IconButton } from "..";
import "./PresetBrowser.css";

interface Entry { index: number; lib: string; name: string }

const ROW = 34;      // px per row (keep in sync with .pbrow height)
const OVER = 8;      // overscan rows

function splitName(full: string): { lib: string; name: string } {
  const sep = full.indexOf(" > ");
  return sep >= 0
    ? { lib: full.slice(0, sep), name: full.slice(sep + 3) }
    : { lib: "PRESET", name: full };
}

export interface PresetBrowserProps {
  open: boolean;
  currentIndex: number;
  onSelect: (index: number) => void;
  onClose: () => void;
}

export function PresetBrowser({ open, currentIndex, onSelect, onClose }: PresetBrowserProps) {
  const [entries, setEntries] = useState<Entry[]>([]);
  const [query, setQuery] = useState("");
  const [lib, setLib] = useState<string>("");     // "" = all libraries
  const [busy, setBusy] = useState(false);
  const [scrollTop, setScrollTop] = useState(0);
  const [viewH, setViewH] = useState(420);
  const viewRef = useRef<HTMLDivElement>(null);

  const load = useCallback(() => {
    setBusy(true);
    juceIntegration
      .library("list")
      .then((info) => {
        const list = info.presets ?? [];
        setEntries(list.map((full, index) => ({ index, ...splitName(full) })));
      })
      .finally(() => setBusy(false));
  }, []);

  useEffect(() => {
    if (open) load();
  }, [open, load]);

  // measure the scroll viewport so the virtual window is sized right
  useEffect(() => {
    if (!open) return;
    const measure = () => viewRef.current && setViewH(viewRef.current.clientHeight);
    measure();
    window.addEventListener("resize", measure);
    return () => window.removeEventListener("resize", measure);
  }, [open]);

  // libraries with counts, in first-seen order
  const libs = useMemo(() => {
    const m = new Map<string, number>();
    for (const e of entries) m.set(e.lib, (m.get(e.lib) ?? 0) + 1);
    return [...m.entries()].map(([name, count]) => ({ name, count }));
  }, [entries]);

  const filtered = useMemo(() => {
    const q = query.trim().toLowerCase();
    return entries.filter(
      (e) =>
        (lib === "" || e.lib === lib) &&
        (q === "" || e.name.toLowerCase().includes(q) || e.lib.toLowerCase().includes(q)),
    );
  }, [entries, query, lib]);

  const start = Math.max(0, Math.floor(scrollTop / ROW) - OVER);
  const end = Math.min(filtered.length, Math.ceil((scrollTop + viewH) / ROW) + OVER);
  const slice = filtered.slice(start, end);

  const doImport = useCallback(() => {
    setBusy(true);
    juceIntegration.library("import").then(() => load());
  }, [load]);
  const doRescan = useCallback(() => {
    setBusy(true);
    juceIntegration.library("rescan").then(() => load());
  }, [load]);

  useEffect(() => {
    if (!open) return;
    const onKey = (e: KeyboardEvent) => e.key === "Escape" && onClose();
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [open, onClose]);

  if (!open) return null;

  return createPortal(
    <div className="pb" role="dialog" aria-label="Navegador de presets">
      <div className="pb__scrim" onClick={onClose} />
      <div className="pb__panel">
        <header className="pb__hd">
          <div className="pb__title">
            <Tag>LIBRERÍA</Tag>
            <span className="pb__count">{entries.length} presets{busy ? " · …" : ""}</span>
          </div>
          <div className="pb__acts">
            <IconButton tone="magenta" title="Importar banco / DLC" aria-label="Importar" onClick={doImport} disabled={busy}>
              ＋
            </IconButton>
            <IconButton title="Rescan" aria-label="Rescan" onClick={doRescan} disabled={busy}>
              ⟳
            </IconButton>
            <IconButton tone="ink" title="Cerrar" aria-label="Cerrar" onClick={onClose}>
              ✕
            </IconButton>
          </div>
        </header>

        <input
          className="pb__search"
          type="search"
          placeholder="Buscar preset o librería…"
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          autoFocus
        />

        <div className="pb__libs" role="tablist" aria-label="Filtrar por librería">
          <button className={"pb__chip" + (lib === "" ? " pb__chip--on" : "")} onClick={() => setLib("")}>
            Todas <b>{entries.length}</b>
          </button>
          {libs.map((l) => (
            <button
              key={l.name}
              className={"pb__chip" + (lib === l.name ? " pb__chip--on" : "")}
              onClick={() => setLib(l.name === lib ? "" : l.name)}
            >
              {l.name} <b>{l.count}</b>
            </button>
          ))}
        </div>

        <div className="pb__list" ref={viewRef} onScroll={(e) => setScrollTop(e.currentTarget.scrollTop)}>
          <div className="pb__spacer" style={{ height: filtered.length * ROW }}>
            {slice.map((e, i) => {
              const top = (start + i) * ROW;
              const on = e.index === currentIndex;
              return (
                <button
                  key={e.index}
                  className={"pbrow" + (on ? " pbrow--on" : "")}
                  style={{ top }}
                  onClick={() => onSelect(e.index)}
                >
                  <span className="pbrow__lib">{e.lib}</span>
                  <span className="pbrow__name">{e.name}</span>
                  {on && <span className="pbrow__dot" aria-hidden="true" />}
                </button>
              );
            })}
          </div>
          {filtered.length === 0 && !busy && <div className="pb__empty">Sin resultados</div>}
        </div>
      </div>
    </div>,
    document.body,
  );
}
