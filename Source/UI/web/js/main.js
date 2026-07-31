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
  g.addColorStop(0, "#6a4526"); g.addColorStop(0.5, "#3a2413"); g.addColorStop(1, "#241608");
  x.fillStyle = g; x.fillRect(0, 0, w, h);
  let seed = 7;
  const rnd = () => { seed = (seed * 1664525 + 1013904223) & 0x7fffffff; return seed / 0x7fffffff; };
  // Deep grain streaks (higher contrast, cathedral-like flow).
  for (let i = 0; i < 520; i++) {
    const yy = rnd() * h, amp = 3 + rnd() * 12, freq = 0.005 + rnd() * 0.022, ph = rnd() * 6.28;
    x.beginPath();
    for (let px = 0; px <= w; px += 3) x.lineTo(px, yy + Math.sin(px * freq + ph) * amp);
    const dark = rnd() > 0.55;
    x.strokeStyle = dark
      ? `rgba(${10 + rnd() * 18},${6 + rnd() * 12},${2 + rnd() * 6},${0.10 + rnd() * 0.20})`
      : `rgba(${120 + rnd() * 70},${80 + rnd() * 40},${40 + rnd() * 24},${0.04 + rnd() * 0.08})`;
    x.lineWidth = 0.5 + rnd() * 1.8; x.stroke();
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

// Photoreal knob rendered on a canvas: machined-aluminium skirt with a chrome
// bevel ring, tick ring with active-arc glow, black anodised cap with a
// specular sheen and a lit pointer, plus a soft cast shadow.
function drawKnob(ctx, size, norm, accent) {
  const cx = size / 2, cy = size / 2, R = size / 2;
  const A = accent || "rgba(90,170,255,0.9)";
  ctx.clearRect(0, 0, size, size);

  // Cast shadow.
  ctx.save();
  ctx.shadowColor = "rgba(0,0,0,0.65)"; ctx.shadowBlur = size * 0.11; ctx.shadowOffsetY = size * 0.06;
  ctx.beginPath(); ctx.arc(cx, cy, R * 0.9, 0, Math.PI * 2); ctx.fillStyle = "#000"; ctx.fill();
  ctx.restore();

  // Chrome bevel ring (outer).
  let gr = ctx.createLinearGradient(cx, cy - R, cx, cy + R);
  gr.addColorStop(0, "#c7ccd4"); gr.addColorStop(0.5, "#3a3d44"); gr.addColorStop(1, "#0c0c0e");
  ctx.beginPath(); ctx.arc(cx, cy, R * 0.97, 0, Math.PI * 2); ctx.fillStyle = gr; ctx.fill();

  // Machined skirt.
  let gs = ctx.createRadialGradient(cx, cy - R * 0.55, R * 0.1, cx, cy, R * 0.9);
  gs.addColorStop(0, "#a7a7ae"); gs.addColorStop(0.45, "#5a5a61"); gs.addColorStop(0.82, "#2c2c31"); gs.addColorStop(1, "#111114");
  ctx.beginPath(); ctx.arc(cx, cy, R * 0.9, 0, Math.PI * 2); ctx.fillStyle = gs; ctx.fill();
  ctx.beginPath(); ctx.arc(cx, cy, R * 0.83, Math.PI * 1.02, Math.PI * 1.98); ctx.strokeStyle = "rgba(255,255,255,0.4)"; ctx.lineWidth = R * 0.045; ctx.stroke();

  // Tick ring with an active-value glow arc.
  const start = -135, sweep = 270;
  ctx.save();
  ctx.beginPath(); ctx.arc(cx, cy, R * 0.995, (start - 90) * Math.PI / 180, (start - 90 + sweep) * Math.PI / 180);
  ctx.strokeStyle = "rgba(0,0,0,0.55)"; ctx.lineWidth = R * 0.07; ctx.stroke();
  ctx.beginPath(); ctx.arc(cx, cy, R * 0.995, (start - 90) * Math.PI / 180, (start - 90 + sweep * norm) * Math.PI / 180);
  ctx.strokeStyle = A; ctx.lineWidth = R * 0.05; ctx.shadowColor = A; ctx.shadowBlur = size * 0.06; ctx.stroke();
  ctx.restore();
  for (let i = 0; i <= 10; i++) {
    const a = (start + i * 27) * Math.PI / 180, maj = i % 5 === 0;
    const r1 = R * 0.92, r2 = R * (maj ? 0.76 : 0.82);
    ctx.beginPath();
    ctx.moveTo(cx + Math.sin(a) * r1, cy - Math.cos(a) * r1);
    ctx.lineTo(cx + Math.sin(a) * r2, cy - Math.cos(a) * r2);
    ctx.strokeStyle = maj ? "rgba(255,255,255,0.75)" : "rgba(255,255,255,0.32)";
    ctx.lineWidth = maj ? 1.7 : 1; ctx.stroke();
  }

  // Anodised cap.
  const br = R * 0.66;
  let gb = ctx.createRadialGradient(cx - br * 0.4, cy - br * 0.55, br * 0.1, cx, cy, br);
  gb.addColorStop(0, "#54545e"); gb.addColorStop(0.5, "#1d1d22"); gb.addColorStop(1, "#040406");
  ctx.beginPath(); ctx.arc(cx, cy, br, 0, Math.PI * 2); ctx.fillStyle = gb; ctx.fill();

  // Specular sheen.
  ctx.save(); ctx.beginPath(); ctx.arc(cx, cy, br, 0, Math.PI * 2); ctx.clip();
  let gh = ctx.createLinearGradient(cx, cy - br, cx, cy + br * 0.2);
  gh.addColorStop(0, "rgba(255,255,255,0.3)"); gh.addColorStop(1, "rgba(255,255,255,0)");
  ctx.beginPath(); ctx.ellipse(cx, cy - br * 0.42, br * 0.78, br * 0.46, 0, 0, Math.PI * 2);
  ctx.fillStyle = gh; ctx.fill(); ctx.restore();
  ctx.beginPath(); ctx.arc(cx, cy, br, 0, Math.PI * 2); ctx.strokeStyle = "rgba(0,0,0,0.9)"; ctx.lineWidth = 1; ctx.stroke();

  // Lit pointer.
  const a = (start + norm * sweep) * Math.PI / 180;
  ctx.save(); ctx.shadowColor = A; ctx.shadowBlur = size * 0.05;
  ctx.beginPath();
  ctx.moveTo(cx + Math.sin(a) * br * 0.18, cy - Math.cos(a) * br * 0.18);
  ctx.lineTo(cx + Math.sin(a) * br * 0.9, cy - Math.cos(a) * br * 0.9);
  ctx.lineCap = "round"; ctx.lineWidth = Math.max(2, br * 0.17); ctx.strokeStyle = "#f6f6f8"; ctx.stroke();
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

  const accent = /CUTOFF|RESO|FILTER|EMPHASIS|CONTOUR/.test(id) ? "rgba(230,140,70,0.95)"
               : /^MIX_|VOLUME|MASTER/.test(id)                 ? "rgba(90,170,255,0.95)"
               : /AMP_|LOUD/.test(id)                           ? "rgba(90,220,140,0.95)"
               :                                                   "rgba(200,180,120,0.95)";

  let state;
  try {
    state = getSliderState(id);
  } catch (e) {
    drawKnob(ctx, size, 0.5, accent);
    return;
  }

  const render = () => {
    const norm = state.getNormalisedValue();
    drawKnob(ctx, size, norm, accent);
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

  const grid = (ctx, w, h, cols, rows, col) => {
    ctx.strokeStyle = col; ctx.lineWidth = 1;
    for (let i = 1; i < cols; i++) { const x = (i / cols) * w; ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke(); }
    for (let j = 1; j < rows; j++) { const y = (j / rows) * h; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke(); }
  };

  const drawScope = (wave) => {
    const w = scope.width, h = scope.height;
    sctx.clearRect(0, 0, w, h);
    grid(sctx, w, h, 8, 4, "rgba(80,220,120,0.10)");
    sctx.strokeStyle = "rgba(80,220,120,0.28)"; sctx.lineWidth = 1;
    sctx.beginPath(); sctx.moveTo(0, h / 2); sctx.lineTo(w, h / 2); sctx.stroke();

    sctx.save();
    sctx.shadowColor = "rgba(92,240,138,0.8)"; sctx.shadowBlur = 6;
    sctx.strokeStyle = "#7dffab"; sctx.lineWidth = 1.6; sctx.lineJoin = "round";
    sctx.beginPath();
    wave.forEach((v, i) => {
      const x = (i / (wave.length - 1)) * w;
      const y = h / 2 - v * (h / 2 - 2);
      i === 0 ? sctx.moveTo(x, y) : sctx.lineTo(x, y);
    });
    sctx.stroke();
    sctx.restore();
  };

  const drawSpectrum = (bins) => {
    const w = spectrum.width, h = spectrum.height;
    pctx.clearRect(0, 0, w, h);
    grid(pctx, w, h, 8, 4, "rgba(90,160,220,0.08)");
    const bw = w / bins.length;
    pctx.save();
    pctx.shadowColor = "rgba(90,180,240,0.5)"; pctx.shadowBlur = 4;
    for (let i = 0; i < bins.length; i++) {
      const bh = bins[i] * h;
      const t = i / bins.length;
      const g = pctx.createLinearGradient(0, h, 0, h - bh);
      g.addColorStop(0, `rgba(${40 + t * 120},${190 - t * 50},${230 - t * 120},0.5)`);
      g.addColorStop(1, `rgb(${70 + t * 150},${210 - t * 40},${240 - t * 120})`);
      pctx.fillStyle = g;
      pctx.fillRect(i * bw, h - bh, Math.max(1, bw - 0.6), bh);
    }
    pctx.restore();
  };

  const vuL = document.getElementById("vuL");
  const vuR = document.getElementById("vuR");
  const vuLp = document.getElementById("vuLp");
  const vuRp = document.getElementById("vuRp");
  let peakHold = [0, 0];
  const drawVU = (levels) => {
    if (vuL) vuL.style.height = (levels[0] * 100).toFixed(1) + "%";
    if (vuR) vuR.style.height = (levels[1] * 100).toFixed(1) + "%";
    for (let c = 0; c < 2; c++) peakHold[c] = levels[c] > peakHold[c] ? levels[c] : peakHold[c] * 0.94;
    if (vuLp) vuLp.style.bottom = (peakHold[0] * 100).toFixed(1) + "%";
    if (vuRp) vuRp.style.bottom = (peakHold[1] * 100).toFixed(1) + "%";
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
// Nested preset browser: library -> category folders -> presets (flyout to
// the side). Kept separate from the flat combo dropdown.
function makePresetTree(host, onSelect) {
  host.classList.add("dd", "pdd");
  host.innerHTML =
    `<div class="pdd-row">` +
    `<button class="dd-nav prev" type="button" title="Previous">\u2039</button>` +
    `<button class="dd-btn" type="button"><span class="dd-lbl">Init</span><span class="dd-arw"></span></button>` +
    `<button class="dd-nav next" type="button" title="Next">\u203A</button>` +
    `</div><div class="dd-menu pmenu"></div>`;
  const btn = host.querySelector(".dd-btn");
  const lbl = host.querySelector(".dd-lbl");
  const menu = host.querySelector(".dd-menu");
  let opts = [];        // {el,val} for menu items
  let order = [];       // {val,label} in menu order (for prev/next stepping)
  let curIdx = 0;

  const pick = (val, label) => {
    lbl.textContent = label;
    opts.forEach((o) => o.el.classList.toggle("sel", String(o.val) === String(val)));
    const i = order.findIndex((o) => String(o.val) === String(val));
    if (i >= 0) curIdx = i;
    closeAllDD();
    onSelect(val);
  };
  const step = (d) => {
    if (!order.length) return;
    curIdx = (curIdx + d + order.length) % order.length;
    const o = order[curIdx]; pick(o.val, o.label);
  };
  const mkOpt = (o, into) => {
    const it = document.createElement("div");
    it.className = "dd-opt"; it.textContent = o.label; it.dataset.val = o.value;
    it.addEventListener("click", (e) => { e.stopPropagation(); pick(o.value, o.label); });
    into.appendChild(it); opts.push({ el: it, val: o.value }); order.push({ val: o.value, label: o.label });
  };
  const positionSub = (fo, sub) => {
    // Descendants inherit their parent folder's direction so the cascade keeps
    // flowing the same way instead of doubling back over the parent column.
    const pf = fo.parentElement.closest(".dd-folder");
    let left;
    if (pf) left = pf.classList.contains("left");
    else { const r = fo.getBoundingClientRect(); left = window.innerWidth - r.right < 200; }
    fo.classList.toggle("left", left);
    if (!sub.classList.contains("nav")) {
      const r = fo.getBoundingClientRect();
      sub.style.maxHeight = Math.max(120, Math.min(320, window.innerHeight - r.top - 12)) + "px";
    }
  };

  const mkFolder = (f, into) => {
    const fo = document.createElement("div"); fo.className = "dd-folder";
    fo.innerHTML = `<span class="dd-fl">${f.label}</span><span class="dd-chev"></span>`;
    const sub = document.createElement("div"); sub.className = "dd-sub";
    (f.folders || []).forEach((sf) => mkFolder(sf, sub));
    (f.items || []).forEach((o) => mkOpt(o, sub));
    if (f.folders && f.folders.length) sub.classList.add("nav");
    fo.appendChild(sub);
    fo.addEventListener("click", (e) => {
      e.stopPropagation();
      const parent = fo.parentElement, willOpen = !fo.classList.contains("open");
      parent.querySelectorAll(":scope > .dd-folder.open").forEach((x) => {
        x.classList.remove("open");
        x.querySelectorAll(".dd-folder.open").forEach((y) => y.classList.remove("open"));
      });
      if (willOpen) { fo.classList.add("open"); positionSub(fo, sub); }
      else fo.querySelectorAll(".dd-folder.open").forEach((y) => y.classList.remove("open"));
    });
    into.appendChild(fo);
  };

  const build = (data) => {
    opts = []; order = []; curIdx = 0; menu.innerHTML = "";
    if (data.lib) {
      const h = document.createElement("div"); h.className = "dd-lib"; h.textContent = data.lib;
      menu.appendChild(h);
    }
    (data.flat || []).forEach((o) => mkOpt(o, menu));
    (data.folders || []).forEach((f) => mkFolder(f, menu));
  };

  btn.addEventListener("click", (e) => {
    e.stopPropagation();
    const open = !host.classList.contains("open");
    closeAllDD();
    menu.querySelectorAll(".dd-folder.open").forEach((x) => x.classList.remove("open"));
    if (open) { host.classList.add("open"); __openDD = host; }
  });
  host.querySelector(".prev").addEventListener("click", (e) => { e.stopPropagation(); step(-1); });
  host.querySelector(".next").addEventListener("click", (e) => { e.stopPropagation(); step(1); });

  return {
    build,
    setValue: (val) => {
      let label = "";
      opts.forEach((o) => {
        const on = String(o.val) === String(val);
        o.el.classList.toggle("sel", on);
        if (on) label = o.el.textContent;
      });
      const i = order.findIndex((o) => String(o.val) === String(val));
      if (i >= 0) curIdx = i;
      if (label) lbl.textContent = label;
    },
  };
}

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

  const tree = makePresetTree(host, (v) => { if (v !== "__init__") loadPreset(v); });

  const refresh = async () => {
    const res = await getBank();
    let bank = { libraries: [] };
    try { bank = JSON.parse(res.bank || "{}"); } catch (e) {}
    const libs = bank.libraries
      || [{ name: "Default", categories: bank.categories || [], presets: bank.presets || {} }];
    tree.build({
      flat: [{ label: "Init", value: "__init__" }],
      folders: libs.map((lib) => ({
        label: lib.name,
        folders: (lib.categories || []).map((cat) => ({
          label: cat,
          items: (lib.presets[cat] || []).map((p) => ({
            label: p.name, value: lib.name + "|||" + cat + "|||" + p.name,
          })),
        })),
      })),
    });
    if (res.current) tree.setValue(res.current);
  };

  saveBtn.addEventListener("click", async () => {
    const name = window.prompt("Preset name:", "My Preset");
    if (name) { await savePreset(name); await refresh(); tree.setValue("User|||User|||" + name); }
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
// Randomize  ->  writes musical random values through the parameter relays
// ---------------------------------------------------------------------------
function setupRandom() {
  const btn = document.getElementById("randomBtn");
  if (!btn) return;
  const rn = (a, b) => a + Math.random() * (b - a);
  const pick = (a) => a[Math.floor(Math.random() * a.length)];
  const setS = (id, norm) => { try { getSliderState(id).setNormalisedValue(Math.max(0, Math.min(1, norm))); } catch (e) {} };
  const setC = (id, ix) => { try { getComboBoxState(id).setChoiceIndex(ix); } catch (e) {} };
  const setT = (id, on) => { try { getToggleState(id).setValue(on); } catch (e) {} };
  btn.addEventListener("click", () => {
    setC("OSC1_WAVE", pick([0,1,2,3])); setC("OSC1_RANGE", pick([1,2,3]));
    setC("OSC2_WAVE", pick([0,1,2,3])); setC("OSC2_RANGE", pick([1,2,3])); setS("OSC2_DETUNE", rn(0.38,0.62));
    setC("OSC3_WAVE", pick([0,2,3]));   setC("OSC3_RANGE", pick([1,2,3])); setS("OSC3_DETUNE", rn(0.42,0.58));
    setT("MIX_OSC1_ON", true);              setS("MIX_OSC1_VOL", rn(0.7,0.95));
    setT("MIX_OSC2_ON", Math.random()<0.8); setS("MIX_OSC2_VOL", rn(0.4,0.8));
    setT("MIX_OSC3_ON", Math.random()<0.5); setS("MIX_OSC3_VOL", rn(0.3,0.7));
    setT("MIX_NOISE_ON", Math.random()<0.3);setS("MIX_NOISE_VOL", rn(0.1,0.4)); setC("NOISE_TYPE", pick([0,1]));
    setS("FILTER_CUTOFF", rn(0.25,0.72)); setS("FILTER_RESO", rn(0.1,0.6)); setS("FILTER_ENV", rn(0.2,0.85));
    setS("FILTER_ATTACK", rn(0.0,0.35)); setS("FILTER_DECAY", rn(0.2,0.6)); setS("FILTER_SUSTAIN", rn(0.2,0.7)); setS("FILTER_RELEASE", rn(0.1,0.5));
    setS("AMP_ATTACK", rn(0.0,0.3)); setS("AMP_DECAY", rn(0.2,0.6)); setS("AMP_SUSTAIN", rn(0.5,0.95)); setS("AMP_RELEASE", rn(0.1,0.45));
    setS("FILTER_DRIVE", rn(0.1,0.6)); setS("DRIFT_AMOUNT", rn(0.15,0.4)); setS("BASS_THIN", rn(0.1,0.45));
    setT("SAMPLE_ON", Math.random()<0.35); setC("SAMPLE_SEL", Math.floor(Math.random()*8)); setS("SAMPLE_VOL", rn(0.3,0.7));
    setT("FX_DRIVE_ON", Math.random()<0.3); setS("FX_DRIVE", rn(0.2,0.6));
    setT("FX_CHORUS_ON", Math.random()<0.4); setS("FX_CHORUS", rn(0.2,0.7));
    setT("FX_PHASER_ON", Math.random()<0.25); setS("FX_PHASER", rn(0.3,0.7));
    setT("FX_CRUSH_ON", Math.random()<0.15); setS("FX_CRUSH", rn(0.2,0.5));
    setT("FX_TONE_ON", Math.random()<0.3); setS("FX_TONE", rn(0.3,0.8));
    setT("FX_DELAY_ON", Math.random()<0.35); setS("FX_DELAY_MIX", rn(0.15,0.45)); setS("FX_DELAY_TIME", rn(0.1,0.5));
    setT("FX_REVERB_ON", Math.random()<0.4); setS("FX_REVERB_MIX", rn(0.15,0.5));
  });
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
  setupRandom();
})();
