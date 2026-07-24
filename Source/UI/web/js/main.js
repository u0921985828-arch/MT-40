import {
  getSliderState,
  getToggleState,
  getComboBoxState,
  getNativeFunction,
} from "./juce/index.js";

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
function setupKnob(el) {
  const id = el.dataset.param;
  const caption = el.dataset.caption ?? "";

  el.innerHTML = `<div class="dial"></div>
                  <span class="caption">${caption}</span>
                  <span class="value">--</span>`;
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
// Boot
// ---------------------------------------------------------------------------
document.querySelectorAll(".knob").forEach(setupKnob);
document.querySelectorAll(".rocker").forEach(setupRocker);
document.querySelectorAll(".combo").forEach(setupCombo);
setupKeyboard();
