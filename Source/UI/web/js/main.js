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
function knobFaceHtml() {
  let ticks = "";
  for (let t = 0; t <= 10; t++) {
    const a = (-135 + t * 27) * Math.PI / 180;
    const r1 = 47;
    const r2 = t % 5 === 0 ? 39 : 43;
    const x1 = 50 + r1 * Math.sin(a), y1 = 50 - r1 * Math.cos(a);
    const x2 = 50 + r2 * Math.sin(a), y2 = 50 - r2 * Math.cos(a);
    ticks += `<line x1="${x1.toFixed(1)}" y1="${y1.toFixed(1)}" x2="${x2.toFixed(1)}" y2="${y2.toFixed(1)}"${t % 5 === 0 ? ' class="maj"' : ""}/>`;
  }
  return `<div class="knob-face">
            <svg class="ticks" viewBox="0 0 100 100">${ticks}</svg>
            <div class="skirt"><div class="dial"><span class="pointer"></span></div></div>
          </div>`;
}

function setupKnob(el) {
  const id = el.dataset.param;
  const caption = el.dataset.caption ?? "";

  el.innerHTML = knobFaceHtml() +
    `<span class="caption">${caption}</span><span class="value">--</span>`;
  const dial = el.querySelector(".dial");
  const valueEl = el.querySelector(".value");

  let state;
  try {
    state = getSliderState(id);
  } catch (e) {
    return; // not running inside the JUCE backend
  }

  const render = () => {
    const norm = state.getNormalisedValue();
    dial.style.setProperty("--rot", (-135 + norm * 270).toFixed(1) + "deg");
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
// Combo boxes  ->  WebComboBoxRelay
// ---------------------------------------------------------------------------
function setupCombo(el) {
  const id = el.dataset.param;
  const caption = el.dataset.caption ?? "";

  el.innerHTML = `<select></select><span class="caption">${caption}</span>`;
  const select = el.querySelector("select");

  let state;
  try {
    state = getComboBoxState(id);
  } catch (e) {
    return;
  }

  const rebuild = () => {
    const choices = state.properties.choices ?? [];
    if (select.options.length !== choices.length) {
      select.innerHTML = "";
      choices.forEach((c, i) => {
        const opt = document.createElement("option");
        opt.value = i;
        opt.textContent = c;
        select.appendChild(opt);
      });
    }
    select.value = String(state.getChoiceIndex());
  };
  state.propertiesChangedEvent.addListener(rebuild);
  state.valueChangedEvent.addListener(rebuild);
  rebuild();

  select.addEventListener("change", () =>
    state.setChoiceIndex(parseInt(select.value, 10))
  );
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
  const select = document.getElementById("presetSelect");
  const saveBtn = document.getElementById("presetSave");
  if (!select || !saveBtn) return;

  let getPresets, loadPreset, savePreset;
  try {
    getPresets = getNativeFunction("getPresets");
    loadPreset = getNativeFunction("loadPreset");
    savePreset = getNativeFunction("savePreset");
  } catch (e) { return; }

  const refresh = async () => {
    const res = await getPresets();
    select.innerHTML = "";
    (res.names || []).forEach((n) => {
      const opt = document.createElement("option");
      opt.value = n; opt.textContent = n;
      select.appendChild(opt);
    });
    if (res.current) select.value = res.current;
  };

  select.addEventListener("change", () => loadPreset(select.value));
  saveBtn.addEventListener("click", async () => {
    const name = window.prompt("Preset name:", select.value || "My Preset");
    if (name) { await savePreset(name); await refresh(); select.value = name; }
  });

  refresh();
}

// ---------------------------------------------------------------------------
// Boot
// ---------------------------------------------------------------------------
(async () => {
  await loadFrontend();

  document.querySelectorAll(".knob").forEach(setupKnob);
  document.querySelectorAll(".rocker").forEach(setupRocker);
  document.querySelectorAll(".combo").forEach(setupCombo);
  setupKeyboard();
  setupVisualiser();
  setupPresets();
})();
