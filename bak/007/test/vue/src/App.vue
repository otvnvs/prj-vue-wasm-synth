<!-- test/vue/src/App.vue -->
<script setup>
import { onMounted, onBeforeUnmount, ref } from 'vue';
import { useSynth } from './composables/useSynth';
const{
	isReady,
	isStreaming,
	globalVolume,
	activeWaveform,
	init,
	startStream,
	noteOn,
	noteOff,
	attack,
	decay,
	sustain,
	release,
	updateAdsr,
	tremoloActive,
	tremoloSpeed,
	tremoloDepth,
	updateTremolo,
	echoActive,
	echoTime,
	echoFeedback,
	echoMix,
	updateEcho,
	drawbars,
	updateCustomHarmonics,
	distActive,
	distDrive,
	distBlend,
	updateDistortion,
	chorusActive,
	chorusRate,
	chorusDepth,
	updateChorus
} = useSynth();




// Tracks currently held down QWERTY keys to eliminate OS keyboard repeating/spamming
const keysPressedTracker = new Set();

// 1. Expanded Piano Key frequencies spanning Octave 4 and Octave 5
const pianoKeys = [
  // 4th Octave
  { note: 'c4',  freq: Math.pow(2, (60 - 69) / 12) * 440, isBlack: false, id: 60 },
  { note: 'c#4', freq: Math.pow(2, (61 - 69) / 12) * 440, isBlack: true,  id: 61 },
  { note: 'd4',  freq: Math.pow(2, (62 - 69) / 12) * 440, isBlack: false, id: 62 },
  { note: 'd#4', freq: Math.pow(2, (63 - 69) / 12) * 440, isBlack: true,  id: 63 },
  { note: 'e4',  freq: Math.pow(2, (64 - 69) / 12) * 440, isBlack: false, id: 64 },
  { note: 'f4',  freq: Math.pow(2, (65 - 69) / 12) * 440, isBlack: false, id: 65 },
  { note: 'f#4', freq: Math.pow(2, (66 - 69) / 12) * 440, isBlack: true,  id: 66 },
  { note: 'g4',  freq: Math.pow(2, (67 - 69) / 12) * 440, isBlack: false, id: 67 },
  { note: 'g#4', freq: Math.pow(2, (68 - 69) / 12) * 440, isBlack: true,  id: 68 },
  { note: 'a4',  freq: Math.pow(2, (69 - 69) / 12) * 440, isBlack: false, id: 69 },
  { note: 'a#4', freq: Math.pow(2, (70 - 69) / 12) * 440, isBlack: true,  id: 70 },
  { note: 'b4',  freq: Math.pow(2, (71 - 69) / 12) * 440, isBlack: false, id: 71 },

  // 5th Octave
  { note: 'c5',  freq: Math.pow(2, (72 - 69) / 12) * 440, isBlack: false, id: 72 },
  { note: 'c#5', freq: Math.pow(2, (73 - 69) / 12) * 440, isBlack: true,  id: 73 },
  { note: 'd5',  freq: Math.pow(2, (74 - 69) / 12) * 440, isBlack: false, id: 74 },
  { note: 'd#5', freq: Math.pow(2, (75 - 69) / 12) * 440, isBlack: true,  id: 75 },
  { note: 'e5',  freq: Math.pow(2, (76 - 69) / 12) * 440, isBlack: false, id: 76 },
  { note: 'f5',  freq: Math.pow(2, (77 - 69) / 12) * 440, isBlack: false, id: 77 },
  { note: 'f#5', freq: Math.pow(2, (78 - 69) / 12) * 440, isBlack: true,  id: 78 },
  { note: 'g5',  freq: Math.pow(2, (79 - 69) / 12) * 440, isBlack: false, id: 79 },
  { note: 'g#5', freq: Math.pow(2, (80 - 69) / 12) * 440, isBlack: true,  id: 80 },
  { note: 'a5',  freq: Math.pow(2, (81 - 69) / 12) * 440, isBlack: false, id: 81 },
  { note: 'a#5', freq: Math.pow(2, (82 - 69) / 12) * 440, isBlack: true,  id: 82 },
  { note: 'b5',  freq: Math.pow(2, (83 - 69) / 12) * 440, isBlack: false, id: 83 },

  // 6th Octave
  { note: 'c6',  freq: Math.pow(2, (84 - 69) / 12) * 440, isBlack: false, id: 84 },
  { note: 'c#6', freq: Math.pow(2, (85 - 69) / 12) * 440, isBlack: true,  id: 85 },
  { note: 'd6',  freq: Math.pow(2, (86 - 69) / 12) * 440, isBlack: false, id: 86 },
  { note: 'd#6', freq: Math.pow(2, (87 - 69) / 12) * 440, isBlack: true,  id: 87 },
  { note: 'e6',  freq: Math.pow(2, (88 - 69) / 12) * 440, isBlack: false, id: 88 },
  { note: 'f6',  freq: Math.pow(2, (89 - 69) / 12) * 440, isBlack: false, id: 89 },
  { note: 'f#6', freq: Math.pow(2, (90 - 69) / 12) * 440, isBlack: true,  id: 90 },
  { note: 'g6',  freq: Math.pow(2, (91 - 69) / 12) * 440, isBlack: false, id: 91 }
];


// 2. Exact user-requested structural layout map pairing piano note strings to keyboard event key values
const shortcutMap = {
  "z": "c4", "s": "c#4", "x": "d4",  "d": "d#4", "c": "e4", "v": "f4",  "g": "f#4", "b": "g4",  "h": "g#4", "n": "a4",  "j": "a#4","m": "b4",
  ",": "c5", "l": "c#5", ".": "d5",  ";": "d#5", "/": "e5",
  "q": "c5", "2": "c#5", "w": "d5",  "3": "d#5", "e": "e5", "r": "f5",  "5": "f#5", "t": "g5",  "6": "g#5", "y": "a5",  "7": "a#5","u": "b5",
  "i": "c6", "9": "c#6", "o": "d6",  "0": "d#6", "p": "e6", "[": "f6", "=": "f#6", "]": "g6" 
};
// State flag to update dynamic CSS highlighting styles when a QWERTY shortcut is active
const activeVisualKeys = ref(new Set());

const handleEngageEngine = async () => {
  await init();
  await startStream();
};

// 3. Dynamic KeyDown Logic Interceptor
const globalKeyDownHandler = (e) => {
  if (!isStreaming.value) return;
  
  // Ignore inputs if the user is typing inside an input field or select dropdown
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;

  const inputChar = e.key.toLowerCase();
  const targetedNoteName = shortcutMap[inputChar];

  if (targetedNoteName) {
    // Critical repetition guard: if this key is already being held down, stop execution
    if (keysPressedTracker.has(inputChar)) return;

    const matchedKeyData = pianoKeys.find(k => k.note === targetedNoteName);
    if (matchedKeyData) {
      keysPressedTracker.add(inputChar);
      activeVisualKeys.value.add(matchedKeyData.id); // Triggers visual feedback
      noteOn(matchedKeyData.id, matchedKeyData.freq);
    }
  }
};

// 4. Dynamic KeyUp Logic Interceptor
const globalKeyUpHandler = (e) => {
  if (!isStreaming.value) return;

  const inputChar = e.key.toLowerCase();
  const targetedNoteName = shortcutMap[inputChar];

  if (targetedNoteName) {
    const matchedKeyData = pianoKeys.find(k => k.note === targetedNoteName);
    if (matchedKeyData) {
      keysPressedTracker.delete(inputChar);
      activeVisualKeys.value.delete(matchedKeyData.id); // Clears visual feedback
      noteOff(matchedKeyData.id);
    }
  }
};

// Bind listeners natively to global window object on component lifecycle initialization
onMounted(() => {
  window.addEventListener('keydown', globalKeyDownHandler);
  window.addEventListener('keyup', globalKeyUpHandler);
});

// Remove window tracking hooks to completely prevent memory leaks or floating background loops
onBeforeUnmount(() => {
  window.removeEventListener('keydown', globalKeyDownHandler);
  window.removeEventListener('keyup', globalKeyUpHandler);
});
</script>

<template>
  <main class="synth-piano">
    <h1>Polyphonic WASM Piano Keyboard</h1>
    <p>Play chords smoothly using your mouse or mapping shortcuts via your typing keyboard.</p>

    <section class="controls panel">
      <button v-if="!isStreaming" @click="handleEngageEngine" class="btn-power">Power On Piano Engine</button>

      <div v-else class="config-grid">
        <div class="row">
          <div>
            <label>Master Velocity: </label>
            <input type="range" min="0" max="1" step="0.05" v-model="globalVolume" />
          </div>
        </div>

        <!-- NEW: ADSR SLIDERS RENDER LAYOUT -->
        <!-- FIXED: Explicit type-casting for text rendering and input updates -->
        <div class="adsr-panel">
          <h4>ADSR Envelope Contours</h4>
          <div class="adsr-sliders">
            
            <div class="slider-box">
              <label>Attack: <span>{{ Math.round(parseFloat(attack) * 1000) }}</span>ms</label>
              <input type="range" min="0" max="2" step="0.01" :value="attack" @input="attack = $event.target.value; updateAdsr();" />
            </div>

            <div class="slider-box">
              <label>Decay: <span>{{ Math.round(parseFloat(decay) * 1000) }}</span>ms</label>
              <input type="range" min="0" max="2" step="0.01" :value="decay" @input="decay = $event.target.value; updateAdsr();" />
            </div>

            <div class="slider-box">
              <label>Sustain: <span>{{ Math.round(parseFloat(sustain) * 100) }}</span>%</label>
              <input type="range" min="0" max="1" step="0.01" :value="sustain" @input="sustain = $event.target.value; updateAdsr();" />
            </div>

            <div class="slider-box">
              <label>Release: <span>{{ Math.round(parseFloat(release) * 1000) }}</span>ms</label>
              <input type="range" min="0" max="4" step="0.01" :value="release" @input="release = $event.target.value; updateAdsr();" />
            </div>

          </div>
        </div>
      </div>

<div class="effects-panel">
  <h4>Master Effects Architecture</h4>
  <div class="control-row">
    <label>
      <input type="checkbox" v-model="tremoloActive" @change="updateTremolo" /> 
      Enable Tremolo LFO Modulation
    </label>
  </div>
  <div class="adsr-sliders" v-if="tremoloActive">
    <div class="slider-box">
      <label>Speed: <span>{{ tremoloSpeed }}</span> Hz</label>
      <input type="range" min="1" max="20" step="0.1" v-model="tremoloSpeed" @input="updateTremolo" />
    </div>
    <div class="slider-box">
      <label>Depth: <span>{{ Math.round(tremoloDepth * 100) }}</span>%</label>
      <input type="range" min="0" max="1" step="0.01" v-model="tremoloDepth" @input="updateTremolo" />
    </div>
  </div>
</div>

        <!-- TAPE ECHO CONTROL INTERFACE INTERCEPTS -->
        <div class="effect-module-block">
          <label class="toggle-label">
            <input type="checkbox" :value="echoActive" @change="echoActive = !echoActive; updateEcho();" /> 
            Enable Vintage Tape Echo Delay
          </label>
          
          <div class="adsr-sliders" v-if="echoActive">
            <div class="slider-box">
              <label>Echo Time: <span>{{ Math.round(parseFloat(echoTime) * 1000) }}</span>ms</label>
              <input type="range" min="0.05" max="1.5" step="0.01" :value="echoTime" @input="echoTime = $event.target.value; updateEcho();" />
            </div>
            
            <div class="slider-box">
              <label>Feedback: <span>{{ Math.round(parseFloat(echoFeedback) * 100) }}</span>%</label>
              <input type="range" min="0" max="0.95" step="0.01" :value="echoFeedback" @input="echoFeedback = $event.target.value; updateEcho();" />
            </div>
            
            <div class="slider-box">
              <label>Mix Blend: <span>{{ Math.round(parseFloat(echoMix) * 100) }}</span>%</label>
              <input type="range" min="0" max="1" step="0.01" :value="echoMix" @input="echoMix = $event.target.value; updateEcho();" />
            </div>
          </div>
        </div>
        <div>
          <label>Waveform Variant: </label>
          <select v-model="activeWaveform">
            <option value="sine">Pure Sine Wave</option>
            <option value="square">Custom Programmable F-Table</option>
            <option value="sawtooth">Optimized Additive Sawtooth</option>
          </select>
        </div>

        <!-- NEW: THE DRAWBAR CONTROL MATRIX -->
        <div class="drawbar-panel" v-if="activeWaveform === 'square'">
          <h4>Harmonic Drawbar Mixing Matrix</h4>
          <p class="hint">Adjust sliders to blend overtones dynamically (H1 = Fundamental, H2 = Octave, etc.)</p>
          <div class="drawbar-grid">
            <div v-for="(val, index) in drawbars" :key="index" class="drawbar-box">
              <span class="drawbar-label">H{{ index + 1 }}</span>
              <input 
                type="range" 
                min="0" 
                max="1" 
                step="0.02" 
                v-model="drawbars[index]" 
                @input="updateCustomHarmonics" 
                class="vertical-slider"
              />
              <span class="drawbar-value">{{ Math.round(parseFloat(val) * 100) }}%</span>
            </div>
          </div>
        </div>

        <!-- DISTORTION UI BLOCK -->
        <div class="effect-module-block">
          <label class="toggle-label">
            <input type="checkbox" :checked="distActive" @change="distActive = $event.target.checked; updateDistortion();" /> 
            Enable Valve Saturation Smasher (Distortion)
          </label>
          <div class="adsr-sliders" v-if="distActive">
            <div class="slider-box">
              <label>Drive Gain: <span>{{ distDrive }}x</span></label>
              <input type="range" min="1" max="20" step="0.5" :value="distDrive" @input="distDrive = $event.target.value; updateDistortion();" />
            </div>
            <div class="slider-box">
              <label>Wet Mix: <span>{{ Math.round(parseFloat(distBlend) * 100) }}</span>%</label>
              <input type="range" min="0" max="1" step="0.01" :value="distBlend" @input="distBlend = $event.target.value; updateDistortion();" />
            </div>
          </div>
        </div>

        <!-- ANALOG CHORUS UI BLOCK -->
        <div class="effect-module-block">
          <label class="toggle-label">
            <input type="checkbox" :checked="chorusActive" @change="chorusActive = $event.target.checked; updateChorus();" /> 
            Enable Vintage Modulated Stereo Chorus
          </label>
          <div class="adsr-sliders" v-if="chorusActive">
            <div class="slider-box">
              <label>LFO Rate: <span>{{ chorusRate }}</span>Hz</label>
              <input type="range" min="0.1" max="5" step="0.1" :value="chorusRate" @input="chorusRate = $event.target.value; updateChorus();" />
            </div>
            <div class="slider-box">
              <label>Depth Width: <span>{{ Math.round(parseFloat(chorusDepth) * 100) }}</span>%</label>
              <input type="range" min="0" max="1" step="0.01" :value="chorusDepth" @input="chorusDepth = $event.target.value; updateChorus();" />
            </div>
          </div>
        </div>

    </section>

    <!-- COMPONENT KEYBOARD RENDERING TRACKS -->
    <section class="keyboard-container" :class="{ 'disabled-lock': !isStreaming }">
      <div class="keyboard">
        <div 
          v-for="key in pianoKeys" 
          :key="key.id"
          :class="[
            'key', 
            key.isBlack ? 'black-key' : 'white-key',
            { 'key-active': activeVisualKeys.has(key.id) }
          ]"
          @mousedown="noteOn(key.id, key.freq)"
          @mouseup="noteOff(key.id)"
          @mouseleave="noteOff(key.id)"
        >
          <!-- Display both the musical note and its mapped QWERTY shortcut key for user guidance -->
          <div class="label">
            <div class="shortcut-indicator">{{ Object.keys(shortcutMap).find(k => shortcutMap[k] === key.note) }}</div>
            <div>{{ key.note.toUpperCase() }}</div>
          </div>
        </div>
      </div>
    </section>
  </main>
</template>

<style scoped>
.synth-piano { font-family: system-ui, sans-serif; max-width: 850px; margin: 40px auto; padding: 10px; user-select: none; }
.panel { padding: 20px; border: 1px solid #ddd; border-radius: 8px; background: #fff; margin-bottom: 20px; text-align: center; }
.btn-power { background: #ff5722; color: #fff; padding: 12px 24px; font-weight: bold; border: none; font-size: 16px; border-radius: 4px; cursor: pointer; }
.config-bar { display: flex; justify-content: space-around; align-items: center; }
.keyboard-container { background: #111; padding: 30px 15px; border-radius: 8px; box-shadow: 0 8px 16px rgba(0,0,0,0.3); overflow-x: auto; }
.keyboard { display: flex; position: relative; height: 200px; width: 100%; min-width: 780px; }
.key { cursor: pointer; display: flex; align-items: flex-end; justify-content: center; padding-bottom: 12px; border-radius: 0 0 5px 5px; transition: background 0.05s, transform 0.05s; position: relative; }

.white-key { background: linear-gradient(to bottom, #ffffff 0%, #f3f3f3 100%); height: 100%; width: 55px; margin: 0 1px; border: 1px solid #bbb; color: #333; z-index: 1; }
/* Styles for when a white key is active (pressed down via mouse or typing shortcut) */
.white-key.key-active, .white-key:active { background: #cbd5e1; transform: translateY(2px); }

.black-key { background: linear-gradient(to bottom, #444 0%, #111 100%); height: 60%; width: 34px; margin-left: -18px; margin-right: -18px; color: #fff; z-index: 2; border: 1px solid #000; }
/* Styles for when a black key is active */
.black-key.key-active, .black-key:active { background: #475569; transform: translateY(2px); }

.label { font-size: 9px; font-weight: bold; pointer-events: none; text-align: center; display: flex; flex-direction: column; align-items: center; width: 100%; }
.shortcut-indicator { background: rgba(0,0,0,0.08); border-radius: 3px; padding: 1px 4px; margin-bottom: 4px; font-size: 11px; text-transform: uppercase; color: #666; font-family: monospace; }
.black-key .shortcut-indicator { color: #f1f5f9; background: rgba(255,255,255,0.15); }
.disabled-lock { opacity: 0.3; pointer-events: none; }
.config-grid { display: flex; flex-direction: column; gap: 20px; }
.row { display: flex; justify-content: space-around; width: 100%; }
.adsr-panel { border-top: 1px solid #eee; padding-top: 15px; text-align: left; }
.adsr-panel h4 { margin: 0 0 10px 0; color: #555; }
.adsr-sliders { display: grid; grid-template-columns: repeat(4, 1fr); gap: 15px; }
.slider-box { display: flex; flex-direction: column; }
.slider-box label { font-size: 12px; color: #666; margin-bottom: 4px; }
.slider-box span { font-weight: bold; color: #222; }
.drawbar-panel { border-top: 1px solid #eee; margin-top: 20px; padding-top: 15px; text-align: left; }
.drawbar-panel h4 { margin: 0; color: #334155; }
.hint { font-size: 12px; color: #64748b; margin: 4px 0 15px 0; }
.drawbar-grid { display: flex; justify-content: space-around; background: #f8fafc; padding: 15px; border-radius: 6px; border: 1px solid #e2e8f0; }
.drawbar-box { display: flex; flex-direction: column; align-items: center; gap: 8px; }
.drawbar-label { font-size: 11px; font-weight: bold; color: #475569; }
.drawbar-value { font-size: 10px; color: #64748b; width: 35px; text-align: center; }

/* Turns standard range sliders vertical */
.vertical-slider {
  writing-mode: vertical-lr; /* For modern browser engines */
  direction: rtl;            /* Reverses orientation so 100% is at the top */
  width: 15px;
  height: 120px;
  padding: 0;
  cursor: pointer;
}
</style>

