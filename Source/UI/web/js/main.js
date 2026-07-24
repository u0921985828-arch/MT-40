// The JUCE frontend library touches window.__JUCE__ at evaluation time, so wait
// for the native backend to be injected, then load the frontend as a classic
// script (WebKitGTK mishandles ESM sub-resources served by the ResourceProvider,
// returning an empty module namespace). The classic build assigns window.JUCEGLUE.
function waitForBackend(timeoutMs = 5000) {
  return new Promise((resolve) => {
    const start = Date.now();
    (function poll() {
      const j = window.__JUCE__;
      if (j && j.backend && j.initialisationData && j.initialisationData.__juce__sliders)
        return resolve(true);
      if (Date.now() - start > timeoutMs) return resolve(false);
      setTimeout(poll, 20);
    })();
  });
}

// Loads the JUCE frontend and evaluates it in global scope, which defines the
// getSliderState / getToggleState / getComboBoxState / getNativeFunction globals
// used below. Fetch+eval sidesteps WebKitGTK ResourceProvider quirks with ESM
// sub-resources and dynamically injected <script> tags. Note: the holder names
// must NOT be pre-declared with let/const here, or eval throws "duplicate
// variable" against the frontend's own declarations.
async function loadFrontend() {
  await waitForBackend();
  if (window.JUCEGLUE) return;
  try {
    const resp = await fetch("/js/juce/index-classic.js");
    (0, eval)(await resp.text());
  } catch (e) {
    console.warn("JUCE frontend failed to load; UI runs unbound.", e);
  }
}

// ---------------------------------------------------------------------------
// Value formatting for the knob read-outs.
// ---------------------------------------------------------------------------
function formatValue(id, v) {
  if (id === "FILTER_CUTOFF") {
    return v >= 1000 ? (v / 1000).toFixed(2) + " kHz" : Math.round(v) + " Hz";
  }
  if (/ATTACK|DECAY|RELEASE/.test(id)) {
    return v >= 1000 ? (v / 1000).toFixed(2) + " s" : Math.round(v) + " ms";
  }
  if (id === "GLIDE_TIME") return v.toFixed(2) + " s";
  if (id === "PITCH_BEND_RANGE") return Math.round(v) + " st";
  if (/DETUNE|COARSE|MASTER_TUNE/.test(id)) return v.toFixed(2) + " st";
  return v.toFixed(2);
}

// ---------------------------------------------------------------------------
// Knobs  ->  WebSliderRelay
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Procedural material textures (walnut wood + brushed metal) as data-URIs.
// ---------------------------------------------------------------------------
function makeWoodTexture() {
  const w = 512, h = 256, c = document.createElement("canvas"); c.width = w; c.height = h;
  const x = c.getContext("2d");
  const g = x.createLinearGradient(0, 0, w, h);
  g.addColorStop(0, "#5a3a1e"); g.addColorStop(0.5, "#3a2413"); g.addColorStop(1, "#2a1a0d");
  x.fillStyle = g; x.fillRect(0, 0, w, h);
  let seed = 7;
  const rnd = () => { seed = (seed * 1664525 + 1013904223) & 0x7fffffff; return seed / 0x7fffffff; };
  for (let i = 0; i < 340; i++) {
    const yy = rnd() * h, amp = 3 + rnd() * 9, freq = 0.006 + rnd() * 0.02, ph = rnd() * 6.28;
    x.beginPath();
    for (let px = 0; px <= w; px += 4) x.lineTo(px, yy + Math.sin(px * freq + ph) * amp);
    x.strokeStyle = `rgba(${20 + rnd() * 30},${12 + rnd() * 20},${6 + rnd() * 10},${0.05 + rnd() * 0.12})`;
    x.lineWidth = 0.6 + rnd() * 1.6; x.stroke();
  }
  return c.toDataURL("image/png");
}
function makeMetalTexture() {
  const w = 512, h = 96, c = document.createElement("canvas"); c.width = w; c.height = h;
  const x = c.getContext("2d");
  x.fillStyle = "#232327"; x.fillRect(0, 0, w, h);
  let seed = 3; const rnd = () => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed / 0x7fffffff; };
  for (let i = 0; i < 1400; i++) {
    const yy = rnd() * h, len = 40 + rnd() * 300, xx = rnd() * w, gray = 40 + rnd() * 90;
    x.strokeStyle = `rgba(${gray},${gray},${gray + 4},${0.05 + rnd() * 0.08})`;
    x.lineWidth = 0.6; x.beginPath(); x.moveTo(xx, yy); x.lineTo(xx + len, yy + (rnd() - 0.5) * 1.2); x.stroke();
  }
  return c.toDataURL("image/png");
}
function installTextures() {
  const root = document.documentElement.style;
  try { root.setProperty("--wood-tex", `url(${makeWoodTexture()})`); } catch (e) {}
  try { root.setProperty("--metal-tex", `url(${makeMetalTexture()})`); } catch (e) {}
}

// Photoreal knob rendered on a canvas: brushed-metal skirt with a bevel,
// black pointer cap with a specular highlight, tick ring and a cast shadow.
function drawKnob(ctx, size, norm) {
  const cx = size / 2, cy = size / 2, R = size / 2;
  ctx.clearRect(0, 0, size, size);

  ctx.save();
  ctx.shadowColor = "rgba(0,0,0,0.6)"; ctx.shadowBlur = size * 0.09; ctx.shadowOffsetY = size * 0.05;
  ctx.beginPath(); ctx.arc(cx, cy, R * 0.92, 0, Math.PI * 2); ctx.fillStyle = "#000"; ctx.fill();
  ctx.restore();

  let gs = ctx.createRadialGradient(cx, cy - R * 0.5, R * 0.1, cx, cy, R);
  gs.addColorStop(0, "#9a9aa0"); gs.addColorStop(0.45, "#55555b"); gs.addColorStop(0.8, "#2b2b30"); gs.addColorStop(1, "#0a0a0c");
  ctx.beginPath(); ctx.arc(cx, cy, R * 0.97, 0, Math.PI * 2); ctx.fillStyle = gs; ctx.fill();
  ctx.beginPath(); ctx.arc(cx, cy, R * 0.9, Math.PI * 1.05, Math.PI * 1.95); ctx.strokeStyle = "rgba(255,255,255,0.35)"; ctx.lineWidth = R * 0.05; ctx.stroke();

  for (let i = 0; i <= 10; i++) {
    const a = (-135 + i * 27) * Math.PI / 180, maj = i % 5 === 0;
    const r1 = R * 0.99, r2 = R * (maj ? 0.82 : 0.88);
    ctx.beginPath();
    ctx.moveTo(cx + Math.sin(a) * r1, cy - Math.cos(a) * r1);
    ctx.lineTo(cx + Math.sin(a) * r2, cy - Math.cos(a) * r2);
    ctx.strokeStyle = maj ? "rgba(255,255,255,0.7)" : "rgba(255,255,255,0.3)";
    ctx.lineWidth = maj ? 1.6 : 1; ctx.stroke();
  }

  const br = R * 0.7;
  let gb = ctx.createRadialGradient(cx - br * 0.35, cy - br * 0.5, br * 0.1, cx, cy, br);
  gb.addColorStop(0, "#50505a"); gb.addColorStop(0.5, "#1c1c20"); gb.addColorStop(1, "#040405");
  ctx.beginPath(); ctx.arc(cx, cy, br, 0, Math.PI * 2); ctx.fillStyle = gb; ctx.fill();

  ctx.save(); ctx.beginPath(); ctx.arc(cx, cy, br, 0, Math.PI * 2); ctx.clip();
  ctx.beginPath(); ctx.ellipse(cx, cy - br * 0.45, br * 0.72, br * 0.4, 0, 0, Math.PI * 2);
  ctx.fillStyle = "rgba(255,255,255,0.22)"; ctx.fill(); ctx.restore();

  ctx.beginPath(); ctx.arc(cx, cy, br, 0, Math.PI * 2); ctx.strokeStyle = "rgba(0,0,0,0.85)"; ctx.lineWidth = 1; ctx.stroke();

  const a = (-135 + norm * 270) * Math.PI / 180;
  ctx.save(); ctx.shadowColor = "rgba(0,0,0,0.8)"; ctx.shadowBlur = 2;
  ctx.beginPath();
  ctx.moveTo(cx + Math.sin(a) * br * 0.2, cy - Math.cos(a) * br * 0.2);
  ctx.lineTo(cx + Math.sin(a) * br * 0.92, cy - Math.cos(a) * br * 0.92);
  ctx.lineCap = "round"; ctx.lineWidth = Math.max(2, br * 0.16); ctx.strokeStyle = "#f4f4f4"; ctx.stroke();
  ctx.restore();
}

function setupKnob(el) {
  const id = el.dataset.param;
  const caption = el.dataset.caption ?? "";
  const size = el.classList.contains("small") ? 40 : 50;
  const dpr = Math.max(1, Math.min(3, window.devicePixelRatio || 1));

  el.innerHTML = `<canvas class="kcv"></canvas><span class="caption">${caption}</span><span class="value">--</span>`;
  const cv = el.querySelector(".kcv");
  cv.width = size * dpr; cv.height = size * dpr; cv.style.width = size + "px"; cv.style.height = size + "px";
  const ctx = cv.getContext("2d"); ctx.scale(dpr, dpr);
  const valueEl = el.querySelector(".value");

  let state;
  try {
    state = getSliderState(id);
  } catch (e) {
    drawKnob(ctx, size, 0.5);
    return;
  }

  const render = () => {
    const norm = state.getNormalisedValue();
    drawKnob(ctx, size, norm);
    valueEl.textContent = formatValue(id, state.getScaledValue());
  };
  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent.addListener(render);
  render();

  // Vertical drag interaction.
  let dragging = false, startY = 0, startNorm = 0;

  el.addEventListener("pointerdown", (e) => {
    dragging = true;
    startY = e.clientY;
    startNorm = state.getNormalisedValue();
    state.sliderDragStarted();
    el.setPointerCapture(e.pointerId);
    e.preventDefault();
  });
  el.addEventListener("pointermove", (e) => {
    if (!dragging) return;
    const range = e.shiftKey ? 800 : 200; // hold Shift for fine control
    let norm = startNorm + (startY - e.clientY) / range;
    norm = Math.min(1, Math.max(0, norm));
    state.setNormalisedValue(norm);
    render();
  });
  const endDrag = (e) => {
    if (!dragging) return;
    dragging = false;
    state.sliderDragEnded();
    if (e.pointerId != null && el.hasPointerCapture(e.pointerId))
      el.releasePointerCapture(e.pointerId);
  };
  el.addEventListener("pointerup", endDrag);
  el.addEventListener("pointercancel", endDrag);

  // Double-click resets to the default (mid handled by backend on gesture).
  el.addEventListener("dblclick", () => {
    state.sliderDragStarted();
    state.setNormalisedValue(state.getNormalisedValue());
    state.sliderDragEnded();
  });
}

// ---------------------------------------------------------------------------
// Rocker switches  ->  WebToggleButtonRelay
// ---------------------------------------------------------------------------
function setupRocker(el) {
  const id = el.dataset.param;
  const caption = el.dataset.caption;

  el.innerHTML = `<div class="cap"></div>` +
    (caption ? `<span class="caption">${caption}</span>` : "");
  el.classList.add (id.startsWith("MIX_") ? "accent-blue" : "accent-orange");

  let state;
  try {
    state = getToggleState(id);
  } catch (e) {
    return;
  }

  const render = () => el.classList.toggle("on", state.getValue());
  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent.addListener(render);
  render();

  el.addEventListener("click", () => state.setValue(!state.getValue()));
}

// ---------------------------------------------------------------------------
// Custom in-DOM dropdown (replaces native <select>, so it stays inside the
// WebView, opens horizontally below the button, and keeps the metal design).
// ---------------------------------------------------------------------------
let __openDD = null;
function closeAllDD() {
  if (__openDD) { __openDD.classList.remove("open"); __openDD = null; }
}
// Capture-phase so a tap anywhere outside the open dropdown dismisses it.
document.addEventListener("pointerdown", (e) => {
  if (!e.target.closest(".dd")) closeAllDD();
}, true);

// Turns a host element into a metal dropdown. Returns an API to feed it flat
// options { setOptions } or grouped options { setGroups }, set the current
// value ({ setValue }), and receive picks (assign onSelect).
function makeDropdown(host, caption) {
  host.classList.add("dd");
  host.innerHTML =
    `<button class="dd-btn" type="button"><span class="dd-lbl">--</span><span class="dd-arw"></span></button>` +
    `<div class="dd-menu"></div>` +
    (caption ? `<span class="caption">${caption}</span>` : "");
  const btn  = host.querySelector(".dd-btn");
  const lbl  = host.querySelector(".dd-lbl");
  const menu = host.querySelector(".dd-menu");
  const api  = { onSelect: () => {} };

  const commit = (val, label) => {
    lbl.textContent = label;
    menu.querySelectorAll(".dd-opt").forEach((it) =>
      it.classList.toggle("sel", String(it.dataset.val) === String(val)));
    closeAllDD();
    api.onSelect(val);
  };
  const mkOpt = (o) => {
    const it = document.createElement("div");
    it.className = "dd-opt"; it.textContent = o.label; it.dataset.val = o.value;
    it.addEventListener("click", (e) => { e.stopPropagation(); commit(o.value, o.label); });
    return it;
  };

  btn.addEventListener("click", (e) => {
    e.stopPropagation();
    const open = !host.classList.contains("open");
    closeAllDD();
    if (open) {
      host.classList.add("open"); __openDD = host;
      // Always open downward; cap the height to the room left in the window so
      // the menu stays inside the frame and scrolls internally when tall.
      const r = btn.getBoundingClientRect();
      const avail = window.innerHeight - r.bottom - 12;
      menu.style.maxHeight = Math.max(96, Math.min(240, avail)) + "px";
      const sel = menu.querySelector(".dd-opt.sel");
      if (sel) sel.scrollIntoView({ block: "nearest" });
    }
  });

  api.setOptions = (opts) => {
    menu.innerHTML = "";
    opts.forEach((o) => menu.appendChild(mkOpt(o)));
  };
  api.setGroups = (groups) => {
    menu.innerHTML = "";
    groups.forEach((g) => {
      if (g.label) {
        const h = document.createElement("div");
        h.className = "dd-group"; h.textContent = g.label; menu.appendChild(h);
      }
      g.items.forEach((o) => menu.appendChild(mkOpt(o)));
    });
  };
  api.setValue = (val) => {
    let label = "";
    menu.querySelectorAll(".dd-opt").forEach((it) => {
      const on = String(it.dataset.val) === String(val);
      it.classList.toggle("sel", on);
      if (on) label = it.textContent;
    });
    if (label) lbl.textContent = label;
  };
  return api;
}

// ---------------------------------------------------------------------------
// Combo boxes  ->  WebComboBoxRelay
// ---------------------------------------------------------------------------
function setupCombo(el) {
  const id = el.dataset.param;
  const caption = el.dataset.caption ?? "";

  let state;
  try {
    state = getComboBoxState(id);
  } catch (e) {
    return;
  }

  const dd = makeDropdown(el, caption);
  dd.onSelect = (v) => state.setChoiceIndex(parseInt(v, 10));

  let count = -1;
  const rebuild = () => {
    const choices = state.properties.choices ?? [];
    if (choices.length !== count) {
      count = choices.length;
      dd.setOptions(choices.map((c, i) => ({ label: c, value: i })));
    }
    dd.setValue(state.getChoiceIndex());
  };
  state.propertiesChangedEvent.addListener(rebuild);
  state.valueChangedEvent.addListener(rebuild);
  rebuild();
}

// ---------------------------------------------------------------------------
// On-screen keyboard  ->  native noteOn / noteOff functions
// ---------------------------------------------------------------------------
function setupKeyboard() {
  const kb = document.getElementById("keyboard");
  const START = 48;      // C3
  const OCTAVES = 3;
  const whiteOffsets = [0, 2, 4, 5, 7, 9, 11];
  const blackAfterWhite = [0, 1, 3, 4, 5]; // C#, D#, F#, G#, A#
  const totalWhites = OCTAVES * 7;

  let noteOn = () => {}, noteOff = () => {};
  try {
    noteOn = getNativeFunction("noteOn");
    noteOff = getNativeFunction("noteOff");
  } catch (e) { /* dev browser */ }

  const press = (note, el) => { el.classList.add("held"); noteOn(note, 0.85); };
  const release = (note, el) => { el.classList.remove("held"); noteOff(note); };

  const bindKey = (el, note) => {
    el.addEventListener("pointerdown", (e) => {
      press(note, el);
      el.setPointerCapture(e.pointerId);
      e.preventDefault();
    });
    const up = () => release(note, el);
    el.addEventListener("pointerup", up);
    el.addEventListener("pointerleave", (e) => { if (e.buttons) up(); });
    el.addEventListener("pointercancel", up);
  };

  // White keys.
  for (let o = 0; o < OCTAVES; o++) {
    for (let wi = 0; wi < 7; wi++) {
      const key = document.createElement("div");
      key.className = "wkey";
      bindKey(key, START + o * 12 + whiteOffsets[wi]);
      kb.appendChild(key);
    }
  }
  // Black keys, absolutely positioned over the white row.
  for (let o = 0; o < OCTAVES; o++) {
    for (const wi of blackAfterWhite) {
      const key = document.createElement("div");
      key.className = "bkey";
      key.style.left = ((o * 7 + wi + 1) / totalWhites * 100) + "%";
      bindKey(key, START + o * 12 + whiteOffsets[wi] + 1);
      kb.appendChild(key);
    }
  }
}

// ---------------------------------------------------------------------------
// Visualiser  <-  "visualiser" backend event (waveform + spectrum)
// ---------------------------------------------------------------------------
function setupVisualiser() {
  const scope = document.getElementById("scope");
  const spectrum = document.getElementById("spectrum");
  if (!scope || !spectrum) return;
  const sctx = scope.getContext("2d");
  const pctx = spectrum.getContext("2d");

  const drawScope = (wave) => {
    const w = scope.width, h = scope.height;
    sctx.clearRect(0, 0, w, h);
    sctx.strokeStyle = "rgba(80, 220, 120, 0.25)";
    sctx.lineWidth = 1;
    sctx.beginPath(); sctx.moveTo(0, h / 2); sctx.lineTo(w, h / 2); sctx.stroke();

    sctx.strokeStyle = "#5cf08a";
    sctx.lineWidth = 1.5;
    sctx.beginPath();
    wave.forEach((v, i) => {
      const x = (i / (wave.length - 1)) * w;
      const y = h / 2 - v * (h / 2 - 2);
      i === 0 ? sctx.moveTo(x, y) : sctx.lineTo(x, y);
    });
    sctx.stroke();
  };

  const drawSpectrum = (bins) => {
    const w = spectrum.width, h = spectrum.height;
    pctx.clearRect(0, 0, w, h);
    const bw = w / bins.length;
    for (let i = 0; i < bins.length; i++) {
      const bh = bins[i] * h;
      const t = i / bins.length;
      pctx.fillStyle = `rgb(${40 + t * 120}, ${180 - t * 40}, ${220 - t * 120})`;
      pctx.fillRect(i * bw, h - bh, Math.max(1, bw - 0.5), bh);
    }
  };

  const vuL = document.getElementById("vuL");
  const vuR = document.getElementById("vuR");
  const drawVU = (levels) => {
    if (vuL) vuL.style.height = (levels[0] * 100).toFixed(1) + "%";
    if (vuR) vuR.style.height = (levels[1] * 100).toFixed(1) + "%";
  };

  try {
    window.__JUCE__.backend.addEventListener("visualiser", (payload) => {
      if (payload.wave) drawScope(payload.wave);
      if (payload.spectrum) drawSpectrum(payload.spectrum);
      if (payload.levels) drawVU(payload.levels);
    });
  } catch (e) { /* dev browser */ }
}

// ---------------------------------------------------------------------------
// Presets  <->  native getPresets / loadPreset / savePreset
// ---------------------------------------------------------------------------
function setupPresets() {
  const host = document.getElementById("presetSelect");
  const saveBtn = document.getElementById("presetSave");
  if (!host || !saveBtn) return;

  let getBank, loadPreset, savePreset;
  try {
    getBank = getNativeFunction("getPresetBank");
    loadPreset = getNativeFunction("loadPreset");
    savePreset = getNativeFunction("savePreset");
  } catch (e) { return; }

  const dd = makeDropdown(host, "");
  dd.onSelect = (v) => { if (v !== "__init__") loadPreset(v); };

  const refresh = async () => {
    const res = await getBank();
    let bank = { categories: [], presets: {} };
    try { bank = JSON.parse(res.bank || "{}"); } catch (e) {}
    const groups = [{ label: "", items: [{ label: "Init", value: "__init__" }] }];
    (bank.categories || []).forEach((cat) => {
      groups.push({
        label: cat,
        items: (bank.presets[cat] || []).map((p) => ({
          label: p.name, value: cat + "|||" + p.name,
        })),
      });
    });
    dd.setGroups(groups);
    if (res.current) dd.setValue(res.current);
  };

  saveBtn.addEventListener("click", async () => {
    const name = window.prompt("Preset name:", "My Preset");
    if (name) { await savePreset(name); await refresh(); dd.setValue("User|||" + name); }
  });

  refresh();
}

// ---------------------------------------------------------------------------
// Pitch / Mod wheels
// ---------------------------------------------------------------------------
function setupWheel(el) {
  const face = el.querySelector(".wheel-face");
  const kind = el.dataset.wheel;              // "pitch" | "mod"
  const isPitch = kind === "pitch";

  // Visual travel: --wy from 4px (top) to 76px (bottom).
  const setVisual = (norm) => { if (face) face.style.setProperty("--wy", (4 + (1 - norm) * 72) + "px"); };

  let pitchBendFn = null, modState = null;
  if (isPitch) {
    try { pitchBendFn = getNativeFunction("pitchBend"); } catch (e) {}
    setVisual(0.5);
  } else {
    try {
      modState = getSliderState(el.dataset.param);
      const render = () => setVisual(modState.getNormalisedValue());
      modState.valueChangedEvent.addListener(render);
      modState.propertiesChangedEvent.addListener(render);
      render();
    } catch (e) { setVisual(0); }
  }

  let dragging = false, startY = 0, startNorm = 0.5;
  el.addEventListener("pointerdown", (e) => {
    dragging = true; startY = e.clientY;
    startNorm = isPitch ? 0.5 : (modState ? modState.getNormalisedValue() : 0);
    el.setPointerCapture(e.pointerId); e.preventDefault();
  });
  el.addEventListener("pointermove", (e) => {
    if (!dragging) return;
    let norm = Math.min(1, Math.max(0, startNorm + (startY - e.clientY) / 120));
    setVisual(norm);
    if (isPitch && pitchBendFn) pitchBendFn(norm * 2 - 1);        // -1..+1
    else if (modState) modState.setNormalisedValue(norm);
  });
  const end = (e) => {
    if (!dragging) return;
    dragging = false;
    if (isPitch) { setVisual(0.5); if (pitchBendFn) pitchBendFn(0); } // spring back
    if (e.pointerId != null && el.hasPointerCapture(e.pointerId)) el.releasePointerCapture(e.pointerId);
  };
  el.addEventListener("pointerup", end);
  el.addEventListener("pointercancel", end);
}

// ---------------------------------------------------------------------------
// Boot
// ---------------------------------------------------------------------------
(async () => {
  installTextures();
  await loadFrontend();

  document.querySelectorAll(".knob").forEach(setupKnob);
  document.querySelectorAll(".rocker").forEach(setupRocker);
  document.querySelectorAll(".combo").forEach(setupCombo);
  document.querySelectorAll(".wheel").forEach(setupWheel);
  setupKeyboard();
  setupVisualiser();
  setupPresets();
})();
