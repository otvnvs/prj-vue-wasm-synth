<!-- test/vue/src/App.vue -->
<script setup>
import { onMounted, onBeforeUnmount, ref } from 'vue';
import { useSynth } from './composables/useSynth';

const {
  isReady, activeNodes, activeConnections,
  init, spawnNode, removeNode, plugCable, unplugCable, updateParam, noteOn, noteOff
} = useSynth();

const keysPressedTracker = new Set();
const activeVisualKeys = ref(new Set());

const selectedClassId = ref('synth_core');
const newNodeCustomId = ref('node_' + Math.floor(Math.random() * 100));

const cableSourceNodeId = ref('');
const cableTargetNodeId = ref('');

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

const handleSpawnClick = async () => {
  if (!newNodeCustomId.value.trim()) return;
  await spawnNode(selectedClassId.value, newNodeCustomId.value.trim());
  newNodeCustomId.value = 'node_' + Math.floor(Math.random() * 100);
};

const handleConnectCable = () => {
  if (!cableSourceNodeId.value || !cableTargetNodeId.value) return;
  plugCable(cableSourceNodeId.value, cableTargetNodeId.value);
  cableSourceNodeId.value = '';
  cableTargetNodeId.value = '';
};

const globalKeyDownHandler = (e) => {
  if (!isReady.value) return;
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
    <h1>WASM Virtual Patchbay Workspace</h1>
    <p>Hot-load nodes and route patch cables dynamically. Play the piano rows using your mouse or typing keys.</p>

    <!-- INITIALIZATION PANEL -->
    <section class="panel activation">
      <button v-if="!isReady" @click="init" class="btn-power">Power On System Rack</button>
      <div v-else class="status-success">✓ Audio Graph Thread Active (Sane Defaults Loaded)</div>
    </section>

    <!-- COMPONENT SPAWNER TOOLBAR -->
    <section class="panel spawner-toolbar" :class="{ 'locked': !isReady }">
      <h3>🔌 Hardware Rack Component Spawner</h3>
      <div class="spawner-controls">
        <div class="field">
          <label>Select Hardware Component Model:</label>
          <select v-model="selectedClassId">
            <option value="synth_core">Polyphonic Oscillator Synth (Generator)</option>
            <option value="resonant_filter">State Variable Low-Pass Filter (Processor)</option>
            <option value="tape_echo">Vintage Magnetic Tape Echo Loop (Processor)</option>
          </select>
        </div>
        <div class="field">
          <label>Custom Instance Node ID Name:</label>
          <input type="text" v-model="newNodeCustomId" placeholder="e.g. filter_main" />
        </div>
        <button @click="handleSpawnClick" class="btn-spawn">Spawn Component Module</button>
      </div>
    </section>

    <!-- SIGNAL CABLE PATCHBAY MATRIX -->
    <section class="panel patchbay-matrix" :class="{ 'locked': !isReady }">
      <h3>🎛️ Virtual Signal Patchbay Cable Matrix</h3>
      <div class="matrix-controls">
        <div class="field">
          <label>Plug Cable Output Jack:</label>
          <select v-model="cableSourceNodeId">
            <option value="" disabled selected>Select Source Output Node...</option>
            <option v-for="(node, id) in activeNodes" :key="id" v-show="node.outputs > 0" :value="id">
              {{ id }} ({{ node.name }}) ➜ Output
            </option>
          </select>
        </div>
        
        <div class="cable-arrow">➔</div>

        <div class="field">
          <label>Into Input Jack Target Terminal:</label>
          <select v-model="cableTargetNodeId">
            <option value="" disabled selected>Select Input Jack...</option>
            <option value="main_output">🔊 Hardwired Speakers Master (main_output)</option>
            <option v-for="(node, id) in activeNodes" :key="id" v-show="node.inputs > 0" :value="id">
              📥 {{ id }} ({{ node.name }}) Input
            </option>
          </select>
        </div>

        <button @click="handleConnectCable" :disabled="!cableSourceNodeId || !cableTargetNodeId" class="btn-connect">
          Plug In Cable
        </button>
      </div>

      <!-- ACTIVE PATCH CABLE INDICATOR TAGS -->
      <div class="active-cables-list">
        <strong>Currently Connected Patch Cables:</strong>
        <span v-if="activeConnections.length === 0" class="empty-hint"> No cables patched. Rack is silent.</span>
        <div class="cable-container" v-else>
          <div v-for="(cable, idx) in activeConnections" :key="idx" class="cable-cord-tag">
            <span>🔌 [{{ cable.sourceId }}] Out ➔ [{{ cable.targetId }}] In</span>
            <button @click="unplugCable(cable.sourceId, cable.targetId)" class="btn-unplug" title="Pull patch cable out">✕</button>
          </div>
        </div>
      </div>
    </section>

    <!-- AUTOMATED REFLECTIVE NODE HARDWARE CHASSIS DISPLAY GRID -->
    <section class="rack-enclosure" v-if="Object.keys(activeNodes).length > 0">
      <h3>⚙️ Active Hardware Rack Chassis Configurations</h3>
      <div class="rack-grid">
        <div v-for="(node, id) in activeNodes" :key="id" class="rack-chassis-module">
          <div class="chassis-header">
            <h4>{{ node.name }} <span class="instance-id">[{{ id }}]</span></h4>
            <button @click="removeNode(id)" class="btn-remove-node" title="Delete module from rack">Unmount</button>
          </div>
          <div class="jack-indicators">
            <span class="jack-badge in" v-if="node.inputs > 0">🔵 IN JACKS: {{ node.inputs }}</span>
            <span class="jack-badge out" v-if="node.outputs > 0">🟢 OUT JACKS: {{ node.outputs }}</span>
          </div>

          <!-- SLIDERS GENERATED SEAMLESSLY VIA THE REFLECTED MANIFEST JSON -->
          <div class="sliders-grid" v-if="node.parameters && node.parameters.length">
            <div v-for="param in node.parameters" :key="param.id" class="slider-box">
              
              <!-- WAVEFORM SELECTOR DROPDOWNS -->
              <div v-if="param.name === 'WaveformType'" class="control-wrapper">
                <label class="param-title">{{ param.name }}:</label>
                <select :value="param.value" @change="updateParam(id, param.id, $event.target.value)">
                  <option :value="0">Sine Wave</option>
                  <option :value="1">Square Wave</option>
                  <option :value="2">Sawtooth Wave</option>
                </select>
              </div>

              <!-- REGULAR SLIDERS FOR GENERAL AUDIO CONTROLS -->
              <div v-else class="control-wrapper">
                <label class="param-title">
                  {{ param.name }}: 
                  <span class="val-metric">{{ typeof param.value === 'number' ? param.value.toFixed(2) : param.value }}</span>
                </label>
                <input 
                  type="range" 
                  :min="param.min" 
                  :max="param.max" 
                  step="0.01" 
                  :value="param.value" 
                  @input="updateParam(id, param.id, $event.target.value)" 
                />
              </div>

            </div>
          </div>
        </div>
      </div>
    </section>

    <!-- KEYBOARD TOUCH ROW -->
    <section class="keyboard-container" :class="{ 'locked': Object.keys(activeNodes).length === 0 }">
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
/* ==========================================================================
   WASM AUDIO GRAPH VIRTUAL PATCHBAY WORKSPACE - CORE STYLESHEET
   ========================================================================== */

/* 1. Main Root Workspace Layout Container */
.patchbay-app { 
    font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; 
    max-width: 900px; 
    margin: 30px auto; 
    padding: 10px; 
    user-select: none; 
    background-color: #f8fafc;
}

/* 2. Standardized Configuration Panel Enclosures */
.panel { 
    padding: 20px; 
    border: 1px solid #e2e8f0; 
    border-radius: 8px; 
    background: #ffffff; 
    margin-bottom: 15px; 
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
}

/* 3. Action Buttons & Operational Command Elements */
.btn-power { 
    background: #10b981; 
    color: #ffffff; 
    padding: 12px 24px; 
    font-weight: bold; 
    border: none; 
    border-radius: 5px; 
    cursor: pointer; 
    font-size: 16px; 
    transition: background 0.15s ease;
}
.btn-power:hover {
    background: #059669;
}

.btn-spawn { 
    background: #4f46e5; 
    color: #ffffff; 
    border: none; 
    padding: 10px 18px; 
    border-radius: 4px; 
    font-weight: bold; 
    cursor: pointer; 
    transition: background 0.15s ease;
}
.btn-spawn:hover {
    background: #4338ca;
}

.btn-connect { 
    background: #2563eb; 
    color: #ffffff; 
    border: none; 
    padding: 10px 18px; 
    border-radius: 4px; 
    font-weight: bold; 
    cursor: pointer; 
    transition: background 0.15s ease;
}
.btn-connect:hover:not(:disabled) {
    background: #1d4ed8;
}
.btn-connect:disabled { 
    opacity: 0.5; 
    cursor: not-allowed; 
}

/* 4. Spawner Toolbars & Patchbay Input Control Grids */
.spawner-controls, 
.matrix-controls { 
    display: flex; 
    align-items: flex-end; 
    gap: 15px; 
    flex-wrap: wrap; 
}

.field { 
    display: flex; 
    flex-direction: column; 
    gap: 6px; 
}
.field label { 
    font-size: 12px; 
    font-weight: bold; 
    color: #475569; 
}

select, 
input[type="text"], 
input[type="range"] { 
    padding: 8px 12px; 
    font-size: 14px; 
    border-radius: 4px; 
    border: 1px solid #cbd5e1; 
    background: #f8fafc; 
    box-sizing: border-box;
}

.cable-arrow { 
    font-size: 20px; 
    color: #94a3b8; 
    padding-bottom: 6px; 
}

/* 5. Virtual Patch Cable Interface Tags */
.active-cables-list { 
    margin-top: 15px; 
    border-top: 1px solid #f1f5f9; 
    padding-top: 15px; 
    font-size: 14px; 
}
.empty-hint { 
    color: #94a3b8; 
    font-style: italic; 
}
.cable-container { 
    display: flex; 
    gap: 10px; 
    flex-wrap: wrap; 
    margin-top: 8px; 
}
.cable-cord-tag { 
    background: #fff7ed; 
    border: 1px solid #ffedd5; 
    color: #c2410c; 
    padding: 4px 10px; 
    border-radius: 20px; 
    font-weight: bold; 
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; 
    display: inline-flex; 
    align-items: center; 
    gap: 8px; 
    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.05); 
}
.btn-unplug { 
    background: none; 
    border: none; 
    color: #94a3b8; 
    cursor: pointer; 
    font-weight: bold; 
    font-size: 12px; 
    padding: 0 2px; 
    transition: color 0.1s ease;
}
.btn-unplug:hover { 
    color: #ef4444; 
}

/* 6. Hardware Rack Enclosure Case Frame & Chassis Layout */
.rack-enclosure { 
    margin-bottom: 20px; 
    background: #334155; 
    padding: 20px; 
    border-radius: 8px; 
    color: #f8fafc; 
    box-shadow: inset 0 4px 8px rgba(0, 0, 0, 0.2), 0 4px 6px -1px rgba(0, 0, 0, 0.1); 
}
.rack-enclosure h3 { 
    margin-top: 0; 
    color: #94a3b8; 
    border-bottom: 1px solid #475569; 
    padding-bottom: 10px; 
}
.rack-grid { 
    display: flex; 
    flex-direction: column; 
    gap: 15px; 
}
.rack-chassis-module { 
    background: #1e293b; 
    border-left: 6px solid #6366f1; 
    padding: 15px; 
    border-radius: 4px; 
    border: 1px solid #475569; 
}
.chassis-header { 
    display: flex; 
    justify-content: space-between; 
    align-items: center; 
    border-bottom: 1px solid #334155; 
    padding-bottom: 8px; 
    margin-bottom: 10px; 
}
.chassis-header h4 { 
    margin: 0; 
    font-size: 16px; 
    color: #ffffff; 
}
.instance-id { 
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; 
    color: #38bdf8; 
    font-size: 14px; 
    margin-left: 5px; 
}
.btn-remove-node { 
    background: #ef4444; 
    color: #ffffff; 
    border: none; 
    padding: 4px 10px; 
    font-size: 12px; 
    border-radius: 3px; 
    cursor: pointer; 
    transition: background 0.15s ease;
}
.btn-remove-node:hover {
    background: #dc2626;
}

/* 7. Hardware Node I/O Jack Status Badges */
.jack-indicators { 
    display: flex; 
    gap: 10px; 
    margin-bottom: 12px; 
}
.jack-badge { 
    font-size: 10px; 
    font-weight: bold; 
    padding: 2px 6px; 
    border-radius: 3px; 
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; 
}
.jack-badge.in { 
    background: #1e3a8a; 
    color: #93c5fd; 
}
.jack-badge.out { 
    background: #064e3b; 
    color: #6ee7b7; 
}

/* 8. Dynamic Sliders & Parameter Control Rows */
.sliders-grid { 
    display: grid; 
    grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); 
    gap: 15px; 
    width: 100%; 
}
.slider-box { 
    display: flex; 
    flex-direction: column; 
    background: #0f172a; 
    padding: 10px; 
    border-radius: 4px; 
    border: 1px solid #334155; 
}
.control-wrapper { 
    display: flex; 
    flex-direction: column; 
    width: 100%; 
}
.param-title { 
    font-size: 12px; 
    font-weight: bold; 
    color: #94a3b8; 
    margin-bottom: 4px; 
    display: flex; 
    justify-content: space-between; 
}
.val-metric { 
    color: #38bdf8; 
    font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; 
}

/* 9. Hardware Polyphonic Musical Piano Touch Rows */
.keyboard-container { 
    background: #1e293b; 
    padding: 20px 10px; 
    border-radius: 8px; 
    box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
}
.keyboard { 
    display: flex; 
    height: 140px; 
    width: 100%; 
    position: relative; 
}
.key { 
    cursor: pointer; 
    display: flex; 
    align-items: flex-end; 
    justify-content: center; 
    padding-bottom: 6px; 
    border-radius: 0 0 4px 4px; 
    border: 1px solid #64748b; 
    transition: background 0.05s ease, transform 0.05s ease;
}

.white { 
    background: linear-gradient(to bottom, #f8fafc 0%, #f1f5f9 100%); 
    width: 60px; 
    height: 100%; 
    z-index: 1; 
    margin: 0 1px; 
    color: #475569; 
}
.white.active, .white:active { 
    background: linear-gradient(to bottom, #60a5fa 0%, #3b82f6 100%); 
    color: #ffffff;
    transform: translateY(2px);
}

.black { 
    background: linear-gradient(to bottom, #1e293b 0%, #0f172a 100%); 
    width: 34px; 
    height: 60%; 
    z-index: 2; 
    margin-left: -18px; 
    margin-right: -18px; 
    color: #ffffff; 
    border-color: #020617; 
}
.black.active, .black:active { 
    background: linear-gradient(to bottom, #2563eb 0%, #1d4ed8 100%); 
    transform: translateY(2px);
}

.key-label { 
    font-size: 9px; 
    font-weight: bold; 
    pointer-events: none; 
}

/* 10. Engine Locks, Status Badges & Safe Loading Placeholders */
.status-success { 
    color: #059669; 
    font-weight: bold; 
    font-style: italic; 
}
.loading-text { 
    font-size: 13px; 
    color: #94a3b8; 
    font-style: italic; 
}
.locked { 
    opacity: 0.3; 
    pointer-events: none; 
    user-select: none;
}
</style>
