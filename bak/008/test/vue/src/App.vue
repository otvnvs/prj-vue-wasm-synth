<!-- test/vue/src/App.vue -->
<script setup>
import { onMounted, onBeforeUnmount, ref } from 'vue';
import { useSynth } from './composables/useSynth';

const {
  isReady, activeConnections,
  attack, decay, sustain, release, waveType,
  echoTime, echoFeedback, echoMix,
  init, connectNodes, resetPatchbay, setParam, noteOn, noteOff,
  filterCutoff, filterResonance 
} = useSynth();

const keysPressedTracker = new Set();
const activeVisualKeys = ref(new Set());

// Piano 12-Tone Scale Configuration
const pianoKeys = [
  { note: 'c4',  freq: 261.63, id: 60 }, { note: 'c#4', freq: 277.18, id: 61 },
  { note: 'd4',  freq: 293.66, id: 62 }, { note: 'd#4', freq: 311.13, id: 63 },
  { note: 'e4',  freq: 329.63, id: 64 }, { note: 'f4',  freq: 349.23, id: 65 },
  { note: 'f#4', freq: 369.99, id: 66 }, { note: 'g4',  freq: 392.00, id: 67 },
  { note: 'g#4', freq: 415.30, id: 68 }, { note: 'a4',  freq: 440.00, id: 69 },
  { note: 'a#4', freq: 466.16, id: 70 }, { note: 'b4',  freq: 493.88, id: 71 },
  { note: 'c5',  freq: 523.25, id: 72 }
];

const shortcutMap = { "z":"c4", "s":"c#4", "x":"d4", "d":"d#4", "c":"e4", "v":"f4", "g":"f#4", "b":"g4", "h":"g#4", "n":"a4", "j":"a#4", "m":"b4", "i":"c5" };

// Dynamic Cable Patch Combinations
const currentRoutingMode = ref('bypass');

const applyRouting = (mode) => {
  currentRoutingMode.value = mode;
  resetPatchbay();

  if (mode === 'direct') {
    connectNodes('synth_1', 'main_output');
  } else if (mode === 'filter') {
    // Path: Synth -> Filter -> Speakers
    connectNodes('synth_1', 'filter_1');
    connectNodes('filter_1', 'main_output');
  } else if (mode === 'chain') {
    // Multi-Node Cascade Chain: Synth -> Filter -> Echo -> Speakers
    connectNodes('synth_1', 'filter_1');
    connectNodes('filter_1', 'echo_1');
    connectNodes('echo_1', 'main_output');
  }
};

// Global Event Listeners
const globalKeyDownHandler = (e) => {
  if (!isReady.value || currentRoutingMode.value === 'bypass') return;
  const key = e.key.toLowerCase();
  const noteName = shortcutMap[key];
  if (noteName && !keysPressedTracker.has(key)) {
    const keyData = pianoKeys.find(k => k.note === noteName);
    if (keyData) {
      keysPressedTracker.add(key);
      activeVisualKeys.value.add(keyData.id);
      noteOn(keyData.id, keyData.freq);
    }
  }
};

const globalKeyUpHandler = (e) => {
  if (!isReady.value) return;
  const key = e.key.toLowerCase();
  const noteName = shortcutMap[key];
  if (noteName) {
    const keyData = pianoKeys.find(k => k.note === noteName);
    if (keyData) {
      keysPressedTracker.delete(key);
      activeVisualKeys.value.delete(keyData.id);
      noteOff(keyData.id);
    }
  }
};

onMounted(() => {
  window.addEventListener('keydown', globalKeyDownHandler);
  window.addEventListener('keyup', globalKeyUpHandler);
});
onBeforeUnmount(() => {
  window.removeEventListener('keydown', globalKeyDownHandler);
  window.removeEventListener('keyup', globalKeyUpHandler);
});
</script>

<template>
  <main class="patchbay-app">
    <h1>WASM Graph Matrix Router & Patchbay</h1>
    <p>Hot-load independent WebAssembly audio components and route their stream pointers in real time.</p>

    <!-- BOOT SYSTEM CORE -->
    <section class="panel activation">
      <button v-if="!isReady" @click="init" class="btn-power">Power On Graph Architecture</button>
      <div v-else class="status-success">✓ Audio Graph Thread Active</div>
    </section>

    <!-- THE VIRTUAL PATCHBAY PANEL -->
    <section class="panel" :class="{ 'locked': !isReady }">
      <h3>1. Virtual Audio Cable Matrix</h3>
      <div class="patch-options">
        <button :class="{ active: currentRoutingMode === 'bypass' }" @click="applyRouting('bypass')">Disconnect All</button>
        <button :class="{ active: currentRoutingMode === 'direct' }" @click="applyRouting('direct')">Synth ➜ Speakers</button>
        <button :class="{ active: currentRoutingMode === 'filter' }" @click="applyRouting('filter')">Synth ➜ Filter ➜ Speakers</button>
        <button :class="{ active: currentRoutingMode === 'chain' }" @click="applyRouting('chain')">Synth ➜ Filter ➜ Echo ➜ Speakers</button>
      </div>
      <div class="topology-display">
        <strong>Active Graph Routing Connections:</strong>
        <span v-if="activeConnections.length === 0"> None (System Muted)</span>
        <span v-for="(c, idx) in activeConnections" :key="idx" class="cable-tag">
          [{{ c.source }} ➔ {{ c.target }}]
        </span>
      </div>
    </section>

    <!-- NODE INTERNAL CONTROLS -->
    <section class="panel configuration" :class="{ 'locked': currentRoutingMode === 'bypass' }">
      <h3>2. Independent Node Property Modulators</h3>
      
      <div class="node-block">
        <h4>[Node: synth_1] - Polyphonic Generator</h4>
        <label>Waveform:</label>
        <select :value="waveType" @change="waveType = $event.target.value; setParam('synth_1', 4, waveType)">
          <option :value="0">Sine Wave</option>
          <option :value="1">Square Wave</option>
          <option :value="2">Sawtooth Wave</option>
        </select>
        <div class="sliders-grid">
          <label>Attack: <input type="range" min="0" max="1" step="0.01" :value="attack" @input="attack = $event.target.value; setParam('synth_1', 0, attack)" /></label>
          <label>Release: <input type="range" min="0" max="2" step="0.01" :value="release" @input="release = $event.target.value; setParam('synth_1', 3, release)" /></label>
        </div>
      </div>

      <div class="node-block" v-if="currentRoutingMode === 'echo' || currentRoutingMode === 'chain'">
        <h4>[Node: echo_1] - Tape Delay Loop</h4>
        <div class="sliders-grid">
          <label>Time: <input type="range" min="0.1" max="1.5" step="0.01" :value="echoTime" @input="echoTime = $event.target.value; setParam('echo_1', 0, echoTime)" /></label>
          <label>Feedback: <input type="range" min="0" max="0.95" step="0.01" :value="echoFeedback" @input="echoFeedback = $event.target.value; setParam('echo_1', 1, echoFeedback)" /></label>
        </div>
      </div>

      <div class="node-block" v-if="currentRoutingMode === 'filter' || currentRoutingMode === 'chain'">
        <h4>[Node: filter_1] - Resonant Low-Pass Filter</h4>
        <div class="sliders-grid">
          <div class="slider-box">
            <label>Cutoff Frequency: <span>{{ filterCutoff }}</span> Hz</label>
            <input type="range" min="100" max="4000" step="1" :value="filterCutoff" @input="filterCutoff = $event.target.value; setParam('filter_1', 0, filterCutoff)" />
          </div>
          <div class="slider-box">
            <label>Resonance Peak (Q): <span>{{ parseFloat(filterResonance).toFixed(1) }}</span></label>
            <input type="range" min="0.5" max="8" step="0.1" :value="filterResonance" @input="filterResonance = $event.target.value; setParam('filter_1', 1, filterResonance)" />
          </div>
        </div>
      </div>

    </section>

    <!-- INTERACTIVE KEYBOARD KEY ROWS -->
    <section class="keyboard-container" :class="{ 'locked': currentRoutingMode === 'bypass' }">
      <div class="keyboard">
        <div 
          v-for="key in pianoKeys" :key="key.id"
          :class="['key', key.note.includes('#') ? 'black' : 'white', { active: activeVisualKeys.has(key.id) }]"
          @mousedown="noteOn(key.id, key.freq)" @mouseup="noteOff(key.id)" @mouseleave="noteOff(key.id)"
        >
          <span class="key-label">{{ key.note.toUpperCase() }}</span>
        </div>
      </div>
    </section>
  </main>
</template>

<style scoped>
.patchbay-app { font-family: system-ui, sans-serif; max-width: 100%; margin: 40px auto; padding: 10px; }
.panel { padding: 25px; border: 1px solid #e2e8f0; border-radius: 8px; background: #fff; margin-bottom: 20px; }
.btn-power { background: #10b981; color: white; padding: 12px 24px; font-weight: bold; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }
.status-success { color: #059669; font-weight: bold; font-style: italic; }
.patch-options button { padding: 10px 18px; font-size: 14px; margin-right: 12px; cursor: pointer; border-radius: 4px; border: 1px solid #cbd5e1; background: #f8fafc; font-weight: 500; }
.patch-options button.active { background: #3b82f6; color: white; border-color: #2563eb; }
.topology-display { margin-top: 15px; font-size: 13px; color: #475569; }
.cable-tag { background: #eff6ff; color: #1e40af; border: 1px solid #bfdbfe; padding: 3px 8px; border-radius: 4px; margin-left: 8px; font-family: monospace; font-weight: bold; }
.node-block { border-bottom: 1px dashed #e2e8f0; padding-bottom: 15px; margin-bottom: 15px; }
.node-block:last-child { border: none; margin: 0; padding: 0; }
.sliders-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin-top: 10px; }
.sliders-grid label { display: flex; flex-direction: column; font-size: 12px; color: #64748b; }
.keyboard-container { background: #1e293b; padding: 25px 15px; border-radius: 8px; }
.keyboard { display: flex; height: 160px; width: 100%; position: relative; }
.key { cursor: pointer; display: flex; align-items: flex-end; justify-content: center; padding-bottom: 8px; border-radius: 0 0 4px 4px; border: 1px solid #94a3b8; }
.white { background: #f8fafc; width: 60px; height: 100%; z-index: 1; margin: 0 2px; }
.white.active { background: #93c5fd; }
.black { background: #0f172a; width: 36px; height: 60%; z-index: 2; margin-left: -20px; margin-right: -20px; color: white; border-color: #020617; }
.black.active { background: #1d4ed8; }
.key-label { font-size: 10px; font-weight: bold; pointer-events: none; }
.locked { opacity: 0.3; pointer-events: none; }
</style>

