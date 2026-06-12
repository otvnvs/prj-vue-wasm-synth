<!-- src/vue/src/App.vue -->
<script setup>
import { useSynth } from './composables/useSynth';

const {
  isReady,
  isStreaming,
  frequency,
  volume,
  waveform,
  init,
  setWaveform,
  setFrequency,
  setVolume,
  playBuffer,
  toggleStream
} = useSynth();
</script>

<template>
  <main class="synth-app">
    <h1>Emscripten Modular Synth + Vue 3</h1>
    <p>A WebAssembly audio wrapper deployed inside a reactive Vite app environment.</p>

    <!-- STEP 1: INITIALIZER MOUNT -->
    <section class="panel">
      <h3>1. Engine Core Context</h3>
      <button v-if="!isReady" @click="init" class="btn-primary">
        Power On Synthesizer
      </button>
      <div v-else class="status-badge success">
        ✓ WebAssembly Core Connected
      </div>
    </section>

    <!-- STEP 2: MODULATORS CONTROL GRID -->
    <section class="panel" :class="{ 'disabled-overlay': !isReady }">
      <h3>2. Synthesizer Configuration</h3>
      
      <div class="control-row">
        <label>Waveform Variant:</label>
        <select :value="waveform" @change="setWaveform($event.target.value)" :disabled="!isReady">
          <option value="sine">Sine Wave</option>
          <option value="square">Square Wave</option>
          <option value="sawtooth">Sawtooth Wave</option>
        </select>
      </div>

      <div class="control-row">
        <label>Pitch Frequency: <strong>{{ frequency }}</strong> Hz</label>
        <input type="range" min="100" max="1500" step="1" :value="frequency" @input="setFrequency($event.target.value)" :disabled="!isReady" />
      </div>

      <div class="control-row">
        <label>Output Gain: <strong>{{ Math.round(volume * 100) }}</strong>%</label>
        <input type="range" min="0" max="1" step="0.01" :value="volume" @input="setVolume($event.target.value)" :disabled="!isReady" />
      </div>
    </section>

    <!-- STEP 3: ACTION OPERATORS -->
    <section class="panel actions" :class="{ 'disabled-overlay': !isReady }">
      <h3>3. Audio Generation Metrics</h3>
      <button @click="playBuffer(2.0)" :disabled="!isReady">
        Fire 2s Buffer Note
      </button>
      <button @click="toggleStream" :disabled="!isReady" :class="{ 'streaming-active': isStreaming }">
        {{ isStreaming ? 'Mute Live Stream' : 'Engage Continuous Stream' }}
      </button>
    </section>
  </main>
</template>

<style scoped>
.synth-app { font-family: system-ui, sans-serif; max-width: 600px; margin: 40px auto; padding: 20px; }
.panel { border: 1px solid #e0e0e0; border-radius: 8px; padding: 20px; margin-bottom: 20px; background: #fff; position: relative; }
.control-row { display: flex; flex-direction: column; margin-top: 15px; }
label { margin-bottom: 6px; font-size: 14px; color: #444; }
select, input[type="range"], button { padding: 10px; font-size: 15px; border-radius: 4px; border: 1px solid #ccc; }
button { background: #e0e0e0; cursor: pointer; transition: background 0.2s; font-weight: bold; margin-right: 10px; }
button:hover:not(:disabled) { background: #d0d0d0; }
.btn-primary { background: #4caf50; color: white; border: none; }
.btn-primary:hover { background: #43a047; }
.streaming-active { background: #f44336 !important; color: white; border: none; }
.status-badge { font-style: italic; color: #2e7d32; font-weight: 500; }
.disabled-overlay { opacity: 0.5; pointer-events: none; }
</style>

