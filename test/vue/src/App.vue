<script setup>
import { onMounted, onBeforeUnmount, ref, nextTick, reactive, computed } from 'vue';
import { useSynth } from './composables/useSynth';

const {
  isReady, activeNodes, activeConnections,
  init, spawnNode, removeNode, plugCable, unplugCable, updateParam, noteOn, noteOff
} = useSynth();

const keysPressedTracker = new Set();
const activeVisualKeys = ref(new Set());
const selectedClassId = ref('synth_core');
const newNodeCustomId = ref('synth_1');

// svg cable state
const svgEl = ref(null);
const isDraggingCable = ref(false);
const dragSourceId = ref('');
const dragStartPos = ref({ x: 0, y: 0 });
const dragCurrentPos = ref({ x: 0, y: 0 });
const jackCoordinates = ref({});

const domElToSVGPoint = (el) => {
  if (!svgEl.value) return { x: 0, y: 0 };
  const rect = el.getBoundingClientRect();
  const pt = svgEl.value.createSVGPoint();
  pt.x = rect.left + rect.width / 2;
  pt.y = rect.top + rect.height / 2;
  return pt.matrixTransform(svgEl.value.getScreenCTM().inverse());
};

const eventToSVGPoint = (event) => {
  if (!svgEl.value) return { x: 0, y: 0 };
  const pt = svgEl.value.createSVGPoint();
  pt.x = event.clientX;
  pt.y = event.clientY;
  return pt.matrixTransform(svgEl.value.getScreenCTM().inverse());
};

const updateJackPositions = () => {
  nextTick(() => {
    if (!svgEl.value) return;
    const coords = {};
    document.querySelectorAll('.jack.out-jack').forEach(el => {
      coords[`out:${el.getAttribute('data-node-id')}`] = domElToSVGPoint(el);
    });
    document.querySelectorAll('.jack.in-jack').forEach(el => {
      coords[`in:${el.getAttribute('data-node-id')}`] = domElToSVGPoint(el);
    });
    jackCoordinates.value = coords;
  });
};

// cable drag
const startCableDrag = (nodeId, event) => {
  event.preventDefault();
  event.stopPropagation(); // dont trigger module drag
  updateJackPositions();
  dragSourceId.value = nodeId;
  isDraggingCable.value = true;
  nextTick(() => {
    const outEl = document.querySelector(`.jack.out-jack[data-node-id="${nodeId}"]`);
    if (outEl) {
      const pos = domElToSVGPoint(outEl);
      dragStartPos.value = pos;
      dragCurrentPos.value = pos;
    }
  });
};

const dropCableOnInput = (targetNodeId, event) => {
  if (event) event.stopPropagation();
  if (!isDraggingCable.value || !dragSourceId.value) return;
  if (dragSourceId.value !== targetNodeId) {
    plugCable(dragSourceId.value, targetNodeId);
    setTimeout(updateJackPositions, 50);
  }
  cancelCableDrag();
};

const cancelCableDrag = () => {
  isDraggingCable.value = false;
  dragSourceId.value = '';
};

const getBezierPath = (x1, y1, x2, y2) => {
  const sagFactor = Math.max(60, Math.abs(x2 - x1) * 0.35);
  return `M ${x1} ${y1} C ${x1} ${y1 + sagFactor}, ${x2} ${y2 + sagFactor}, ${x2} ${y2}`;
};

// module drag
const nodeOrder = ref([]); // array of node IDs in display order
const draggingModuleId = ref(null);
const dragOverModuleId = ref(null);

const syncNodeOrder = () => {
  const current = Object.keys(activeNodes.value);
  nodeOrder.value = [
    ...nodeOrder.value.filter(id => current.includes(id)),
    ...current.filter(id => !nodeOrder.value.includes(id))
  ];
};

const orderedNodes = computed(() => {
    const activeKeys = Object.keys(activeNodes.value);
    
    const syncedOrder = [
        ...nodeOrder.value.filter(id => activeKeys.includes(id)),
        ...activeKeys.filter(id => !nodeOrder.value.includes(id))
    ];
    
    if (nodeOrder.value.length !== syncedOrder.length) {
        nextTick(() => {
            nodeOrder.value = syncedOrder;
        });
    }

    return syncedOrder
        .map(id => [id, activeNodes.value[id]])
        .filter(([, node]) => node);
});

const startModuleDrag = (nodeId, event) => {
  if (isDraggingCable.value) return;
  event.preventDefault();
  draggingModuleId.value = nodeId;
};

const onModuleDragEnter = (nodeId) => {
  if (!draggingModuleId.value || draggingModuleId.value === nodeId) return;
  dragOverModuleId.value = nodeId;
};

const onModuleDrop = (targetId) => {
  if (!draggingModuleId.value || draggingModuleId.value === targetId) {
    draggingModuleId.value = null;
    dragOverModuleId.value = null;
    return;
  }
  const order = [...nodeOrder.value];
  const fromIdx = order.indexOf(draggingModuleId.value);
  const toIdx = order.indexOf(targetId);
  if (fromIdx !== -1 && toIdx !== -1) {
    order.splice(fromIdx, 1);
    order.splice(toIdx, 0, draggingModuleId.value);
    nodeOrder.value = order;
  }
  draggingModuleId.value = null;
  dragOverModuleId.value = null;
  setTimeout(updateJackPositions, 50);
};

const cancelModuleDrag = () => {
  draggingModuleId.value = null;
  dragOverModuleId.value = null;
};

// global mouse handlers
const globalMouseMoveHandler = (event) => {
  if (isDraggingCable.value) {
    dragCurrentPos.value = eventToSVGPoint(event);
  }
};

const globalMouseUpHandler = (event) => {
  if (draggingModuleId.value) cancelModuleDrag();
  if (isDraggingCable.value && !event.target.closest('.in-jack')) cancelCableDrag();
};

// power and spawner
const handleSpawnClick = async () => {
  if (!newNodeCustomId.value.trim()) return;
  await spawnNode(selectedClassId.value, newNodeCustomId.value.trim());
  newNodeCustomId.value = 'node_' + Math.floor(Math.random() * 100);
  syncNodeOrder();
  setTimeout(updateJackPositions, 120);
};

const handlePowerOn = async () => {
  await init();
  syncNodeOrder();
  setTimeout(updateJackPositions, 200);
};

// piano keys
const pianoKeys = [
  { note: 'c4',  freq: 261.63, id: 60 }, { note: 'c#4', freq: 277.18, id: 61 },
  { note: 'd4',  freq: 293.66, id: 62 }, { note: 'd#4', freq: 311.13, id: 63 },
  { note: 'e4',  freq: 329.63, id: 64 }, { note: 'f4',  freq: 349.23, id: 65 },
  { note: 'f#4', freq: 369.99, id: 66 }, { note: 'g4',  freq: 392.00, id: 67 },
  { note: 'g#4', freq: 415.30, id: 68 }, { note: 'a4',  freq: 440.00, id: 69 },
  { note: 'a#4', freq: 466.16, id: 70 }, { note: 'b4',  freq: 493.88, id: 71 },
  { note: 'c5',  freq: 523.25, id: 72 }, { note: 'c#5', freq: 554.37, id: 73 },
  { note: 'd5',  freq: 587.33, id: 74 }, { note: 'd#5', freq: 622.25, id: 75 },
  { note: 'e5',  freq: 659.25, id: 76 }, { note: 'f5',  freq: 698.46, id: 77 },
  { note: 'f#5', freq: 739.99, id: 78 }, { note: 'g5',  freq: 783.99, id: 79 },
  { note: 'g#5', freq: 830.61, id: 80 }, { note: 'a5',  freq: 880.00, id: 81 },
  { note: 'a#5', freq: 932.33, id: 82 }, { note: 'b5',  freq: 987.77, id: 83 },
  { note: 'c6',  freq: 1046.50, id: 84 }, { note: 'c#6', freq: 1108.73, id: 85 },
  { note: 'd6',  freq: 1174.66, id: 86 }, { note: 'd#6', freq: 1244.51, id: 87 },
  { note: 'e6',  freq: 1318.51, id: 88 }, { note: 'f6',  freq: 1396.91, id: 89 },
  { note: 'f#6', freq: 1479.98, id: 90 }, { note: 'g6',  freq: 1567.98, id: 91 },
];

const shortcutMap = {
  "z":"c4",  "x":"d4",  "c":"e4",  "v":"f4",  "b":"g4",  "n":"a4",  "m":"b4",
  "s":"c#4", "d":"d#4", "g":"f#4", "h":"g#4", "j":"a#4",
  "q":"c5",  "w":"d5",  "e":"e5",  "r":"f5",  "t":"g5",  "y":"a5",  "u":"b5",
  "2":"c#5", "4":"d#5", "5":"f#5", "6":"g#5", "7":"a#5",
  ",":"c5",  ".":"d5",  "/":"e5",  ";":"d#5",
  "i":"c6",  "9":"c#6", "o":"d6",  "0":"d#6", "p":"e6",
  "[":"f6",  "=":"f#6", "]":"g6",
};

const resolveKey = (e) => shortcutMap[e.key] ?? shortcutMap[e.key.toLowerCase()] ?? null;
const globalKeyDownHandler = (e) => {
  if (!isReady.value) return;
  const noteName = resolveKey(e);
  if (noteName && !keysPressedTracker.has(e.key)) {
    const kd = pianoKeys.find(k => k.note === noteName);
    if (kd) {
      keysPressedTracker.add(e.key);
      activeVisualKeys.value.add(kd.id);
      noteOn(kd.id, kd.freq);
    }
  }
};

const globalKeyUpHandler = (e) => {
  if (!isReady.value) return;
  const noteName = resolveKey(e);
  if (noteName) {
    const kd = pianoKeys.find(k => k.note === noteName);
    if (kd) {
      keysPressedTracker.delete(e.key);
      activeVisualKeys.value.delete(kd.id);
      noteOff(kd.id);
    }
  }
};
// Track the active key ID currently pressed via touch interactions
let currentTouchKeyId = null;

const globalTouchStartHandler = (e) => {
  if (!isReady.value) return;
  
  // Look at the very first finger point contact
  const touch = e.touches[0];
  const targetElement = document.elementFromPoint(touch.clientX, touch.clientY);
  
  // Find the closest parent key element containing our data-key attributes
  const keyElement = targetElement?.closest('.key');
  if (keyElement) {
    const keyId = keyElement.getAttribute('data-key-id');
    const freq = parseFloat(keyElement.getAttribute('data-key-freq'));
    
    if (keyId && currentTouchKeyId !== keyId) {
      currentTouchKeyId = keyId;
      activeVisualKeys.value.add(keyId);
      noteOn(keyId, freq);
    }
  }
};

const globalTouchMoveHandler = (e) => {
  if (!isReady.value || !currentTouchKeyId) return;
  
  const touch = e.touches[0];
  const targetElement = document.elementFromPoint(touch.clientX, touch.clientY);
  const keyElement = targetElement?.closest('.key');
  
  if (keyElement) {
    const keyId = keyElement.getAttribute('data-key-id');
    const freq = parseFloat(keyElement.getAttribute('data-key-freq'));
    
    // If the finger has slid onto a brand new key element
    if (keyId && currentTouchKeyId !== keyId) {
      // Kill the old active audio node
      if (currentTouchKeyId) {
        activeVisualKeys.value.delete(currentTouchKeyId);
        noteOff(currentTouchKeyId);
      }
      
      // Fire the new audio node context immediately
      currentTouchKeyId = keyId;
      activeVisualKeys.value.add(keyId);
      noteOn(keyId, freq);
    }
  } else {
    // Finger slid completely off the keyboard array space entirely
    if (currentTouchKeyId) {
      activeVisualKeys.value.delete(currentTouchKeyId);
      noteOff(currentTouchKeyId);
      currentTouchKeyId = null;
    }
  }
};

const globalTouchEndHandler = (e) => {
  // If all fingers left the screen surface layout space
  if (currentTouchKeyId) {
    activeVisualKeys.value.delete(currentTouchKeyId);
    noteOff(currentTouchKeyId);
    currentTouchKeyId = null;
  }
};



// lifecycle

onMounted(() => {
  window.addEventListener('keydown', globalKeyDownHandler);
  window.addEventListener('keyup', globalKeyUpHandler);
  window.addEventListener('mousemove', globalMouseMoveHandler);
  window.addEventListener('mouseup', globalMouseUpHandler);
  window.addEventListener('resize', updateJackPositions);
  window.addEventListener('scroll', updateJackPositions, true);
  
  window.addEventListener('touchstart', globalTouchStartHandler, { passive: false });
  window.addEventListener('touchmove', globalTouchMoveHandler, { passive: false });
  window.addEventListener('touchend', globalTouchEndHandler);
  window.addEventListener('touchcancel', globalTouchEndHandler);
});

onBeforeUnmount(() => {
  window.removeEventListener('keydown', globalKeyDownHandler);
  window.removeEventListener('keyup', globalKeyUpHandler);
  window.removeEventListener('mousemove', globalMouseMoveHandler);
  window.removeEventListener('mouseup', globalMouseUpHandler);
  window.removeEventListener('resize', updateJackPositions);
  window.removeEventListener('scroll', updateJackPositions, true);
  
  window.removeEventListener('touchstart', globalTouchStartHandler);
  window.removeEventListener('touchmove', globalTouchMoveHandler);
  window.removeEventListener('touchend', globalTouchEndHandler);
  window.removeEventListener('touchcancel', globalTouchEndHandler);
});

</script>

<template>
  <main class="patchbay-app">
    <!-- KEYBOARD -->
    <section class="keyboard-container" :class="{ 'locked': !isReady || Object.keys(activeNodes).length === 0 }">
      <div class="keyboard-header">
        <span class="section-label">KEYBOARD</span>
        <span class="keyboard-hint">Mouse-click or use keyboard shortcuts</span>
      </div>
      <div class="keyboard-scroll">
        <div class="keyboard">
          <div v-for="key in pianoKeys"
               :key="key.id"
               :data-key-id="key.id"
               :data-key-freq="key.freq"
               :class="['key', key.note.includes('#') ? 'black' : 'white', { active: activeVisualKeys.has(key.id) }]">
            <span class="key-label">{{ key.note.toUpperCase() }}</span>
            <span class="key-shortcut">{{ Object.entries(shortcutMap).find(([,v]) => v === key.note)?.[0] ?? '' }}</span>
          </div>
        </div>
      </div>
    </section>

    <!-- poser panel -->
    <section class="panel activation">
      <button v-if="!isReady" @click="handlePowerOn" class="btn-power">Power On System Rack</button>
      <button v-if="isReady" disabled class="btn-power">Power On System Rack</button>
    </section>

    <!-- spawner toolbar -->
    <section class="panel spawner-toolbar" :class="{ 'locked': !isReady }">
      <div class="spawner-controls">
        <div class="field">
          <label>Component</label>
          <select v-model="selectedClassId">
		  <!-- todo: make dynamic -->
            <option value="synth_core">Polyphonic Synth (Generator)</option>
            <option value="organ_blender">Organ Wave Blender (Slider Generator)</option> 
            <option value="resonant_filter">Low-Pass Filter (Processor)</option>
            <option value="tape_echo">Tape Echo (Processor)</option>
            <option value="flanger">Flanger (Processor)</option>
            <option value="waveshaper">Waveshaper (Processor)</option>
            <option value="auto_wah">Auto-Wah (Processor)</option>
          </select>
        </div>
	<!--
        <div class="field">
          <label>Instance ID</label>
          <input v-model="newNodeCustomId" placeholder="e.g. filter_main">
        </div>
	-->
        <button @click="handleSpawnClick" class="btn-spawn">+ Spawn Module</button>
      </div>
    </section>

    <!-- cable overlay -->
    <svg ref="svgEl" class="patch-cable-canvas" v-if="isReady">
      <g v-for="cable in activeConnections" :key="cable.id">
      <!-- dynamic bezier -->
	<path v-if="jackCoordinates[`out:${cable.sourceId}`] && jackCoordinates[`in:${cable.targetId}`]"
              :d="getBezierPath(
                jackCoordinates[`out:${cable.sourceId}`].x, 
                jackCoordinates[`out:${cable.sourceId}`].y, 
                jackCoordinates[`in:${cable.targetId}`].x, 
                jackCoordinates[`in:${cable.targetId}`].y
              )"
              class="patch-wire" 
              @dblclick="unplugCable(cable.sourceId, cable.targetId)" 
              title="Double-click to disconnect"/>
      </g>
      <!-- drag helper -->
      <path v-if="isDraggingCable" 
            :d="getBezierPath(dragStartPos.x, dragStartPos.y, dragCurrentPos.x, dragCurrentPos.y)" 
            class="patch-wire preview-wire"/>
    </svg>

    <!-- back chassis -->
    <section class="rack-enclosure" v-if="Object.keys(activeNodes).length > 0">
      <div class="rack-grid">
        
	      <!-- static master block -->
        <div class="rack-chassis-module master-output-chassis">
          <div class="chassis-header non-draggable">
            <div class="header-left">
              <span class="drag-hint-icon">📌</span>
              <h4>MAIN OUTPUT <span class="instance-id">[main_output]</span></h4>
            </div>
          </div>
          <div class="jack-rack-row">
            <div class="jack in-jack" data-node-id="main_output" @mouseup.stop="dropCableOnInput('main_output', $event)" :class="{ 'pulse-highlight': isDraggingCable }">
              <div class="jack-socket"></div>
              <span class="jack-label">MAIN IN</span>
            </div>
          </div>
        </div>

	<!-- dynamic module list -->
        <div v-for="[id, node] in orderedNodes" 
             :key="id" 
             class="rack-chassis-module" 
             :class="{ 'is-dragging': draggingModuleId === id, 'drag-over': dragOverModuleId === id }" 
             @dragover.prevent 
             @dragenter="onModuleDragEnter(id)" 
             @drop="onModuleDrop(id)" 
             @mouseup="draggingModuleId && onModuleDrop(id)">
          
          <div class="chassis-header draggable-header" @mousedown="startModuleDrag(id, $event)" @mousemove="draggingModuleId && draggingModuleId !== id && onModuleDragEnter(id)">
            <div class="header-left">
              <span class="drag-handle" title="Drag to reorder">✥</span>
              <h4>{{ node.name }} <span class="instance-id">[{{ id }}]</span></h4>
            </div>
            <button @click.stop="removeNode(id)" class="btn-remove-node">✕</button>
          </div>

          <div class="jack-rack-row">
            <div v-if="node.inputs > 0" class="jack in-jack" :data-node-id="id" @mouseup.stop="dropCableOnInput(id, $event)" :class="{ 'pulse-highlight': isDraggingCable && dragSourceId !== id }">
              <div class="jack-socket"></div>
              <span class="jack-label">SIGNAL IN</span>
            </div>
            <div v-if="node.outputs > 0" class="jack out-jack" :data-node-id="id" @mousedown="startCableDrag(id, $event)">
              <div class="jack-socket"></div>
              <span class="jack-label">MASTER OUT</span>
            </div>
          </div>

	  <!-- parameter and sliders ui -->
          <div class="sliders-grid" v-if="node.parameters && node.parameters.length">
            <div v-for="param in node.parameters" :key="param.id" class="slider-box">
              
		    <!-- old waveform control for synth.wasm -->
              <div v-if="param.name === 'WaveformType'" class="control-wrapper">
                <label class="param-title">{{ param.name }}</label>
                <select :value="param.value" @change="updateParam(id, param.id, $event.target.value)">
                  <option :value="0">Sine Wave</option>
                  <option :value="1">Square Wave</option>
                  <option :value="2">Sawtooth Wave</option>
                </select>
              </div>

	      <!-- general purpose uniform slider layout -->
              <div v-else class="control-wrapper">
                <label class="param-title">
                  {{ param.name }}
                  <span class="val-metric">{{ typeof param.value === 'number' ? param.value.toFixed(2) : param.value }}</span>
                </label>
                <input type="range" 
                       :min="param.min" 
                       :max="param.max" 
                       step="0.01" 
                       :value="param.value" 
                       @input="updateParam(id, param.id, $event.target.value)">
              </div>

            </div>
          </div>

        </div>
      </div>
    </section>
  </main>
</template>

<style scoped>
:root {
  --bg-base:      #09090b;
  --bg-panel:     #111113;
  --bg-raised:    #18181b;
  --bg-sunken:    #050507;
  --border:       #27272a;
  --border-light: #3f3f46;
  --text:         #e4e4e7;
  --text-muted:   #71717a;
  --text-dim:     #52525b;
  --accent-blue:  #3b82f6;
  --accent-green: #10b981;
  --accent-red:   #ef4444;
  --accent-amber: #f59e0b;
  --accent-indigo:#6366f1;
}

* { box-sizing: border-box; }

.patchbay-app {
  font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  max-width: 960px;
  margin: 0 auto;
  padding: 16px;
  user-select: none;
  background: #09090b;
  min-height: 100vh;
  color: #e4e4e7;
}

h1 { display: none; }

/* panels */
.panel {
  padding: 14px 18px;
  border: 1px solid #27272a;
  border-radius: 8px;
  background: #111113;
  margin-bottom: 12px;
}

/* buttons */
.btn-power {
  background: #10b981;
  color: #fff;
  padding: 10px 22px;
  font-weight: 700;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  font-size: 15px;
  letter-spacing: .02em;
  transition: background .15s, box-shadow .15s;
  box-shadow: 0 0 12px rgba(16,185,129,.3);
}
.btn-power:hover { background: #059669; box-shadow: 0 0 18px rgba(16,185,129,.5); }

.btn-spawn {
  background: #18181b;
  color: #a5b4fc;
  border: 1px solid #4f46e5;
  padding: 9px 18px;
  border-radius: 6px;
  font-weight: 700;
  cursor: pointer;
  font-size: 13px;
  transition: background .15s, border-color .15s, box-shadow .15s;
  white-space: nowrap;
}
.btn-spawn:hover {
  background: #1e1b4b;
  border-color: #818cf8;
  box-shadow: 0 0 10px rgba(99,102,241,.35);
}

.btn-remove-node {
  background: transparent;
  color: #71717a;
  border: 1px solid #3f3f46;
  padding: 3px 8px;
  font-size: 12px;
  border-radius: 4px;
  cursor: pointer;
  transition: color .15s, border-color .15s, background .15s;
  flex-shrink: 0;
}
.btn-remove-node:hover {
  color: #ef4444;
  border-color: #ef4444;
  background: rgba(239,68,68,.1);
}

/* spawner */
.spawner-controls { display: flex; align-items: flex-end; gap: 12px; flex-wrap: wrap; }
.field { display: flex; flex-direction: column; gap: 5px; }
.field label {
  font-size: 11px;
  font-weight: 700;
  color: #71717a;
  text-transform: uppercase;
  letter-spacing: .06em;
}

select, input[type="text"] {
  padding: 8px 12px;
  font-size: 13px;
  border-radius: 6px;
  border: 1px solid #3f3f46;
  background: #09090b;
  color: #e4e4e7;
  outline: none;
  transition: border-color .15s, box-shadow .15s;
}
select:focus, input[type="text"]:focus {
  border-color: #6366f1;
  box-shadow: 0 0 0 2px rgba(99,102,241,.2);
}
select option { background: #18181b; color: #e4e4e7; }

input[type="range"] {
  -webkit-appearance: none;
  appearance: none;
  width: 100%;
  height: 4px;
  border-radius: 2px;
  background: #27272a;
  outline: none;
  cursor: pointer;
  border: none;
  padding: 0;
}
input[type="range"]::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 14px; height: 14px;
  border-radius: 50%;
  background: #6366f1;
  border: 2px solid #a5b4fc;
  cursor: pointer;
  transition: transform .1s;
}
input[type="range"]::-webkit-slider-thumb:hover { transform: scale(1.2); }

/* status bar */
.status-success {
  color: #10b981;
  font-weight: 600;
  font-size: 13px;
}
.hint-green { color: #10b981; font-weight: 700; }
.hint-blue  { color: #3b82f6; font-weight: 700; }

/* rack */
.rack-enclosure {
  background: #0c0c0e;
  border: 1px solid #27272a;
  border-radius: 10px;
  padding: 14px;
  margin-bottom: 12px;
  box-shadow: inset 0 2px 8px rgba(0,0,0,.6);
}
.rack-grid { display: flex; flex-direction: column; gap: 10px; }

.rack-chassis-module {
  background: #111113;
  border: 1px solid #27272a;
  border-left: 4px solid #6366f1;
  border-radius: 6px;
  padding: 12px 14px;
  transition: border-color .15s, box-shadow .15s, opacity .15s;
}
.rack-chassis-module.is-dragging {
  opacity: .45;
  box-shadow: none;
}
.rack-chassis-module.drag-over {
  border-color: #818cf8;
  box-shadow: 0 0 0 2px rgba(99,102,241,.35);
}
.master-output-chassis { border-left-color: #ef4444 !important; }

/* chassis header */
.chassis-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding-bottom: 10px;
  margin-bottom: 10px;
  border-bottom: 1px solid #1c1c1f;
}
.header-left { display: flex; align-items: center; gap: 8px; }
.draggable-header { cursor: grab; }
.draggable-header:active { cursor: grabbing; }
.non-draggable { cursor: default; }

.drag-handle {
  font-size: 16px;
  color: #3f3f46;
  line-height: 1;
  transition: color .15s;
  user-select: none;
}
.draggable-header:hover .drag-handle { color: #6366f1; }

.drag-hint-icon { font-size: 14px; opacity: .5; }

.chassis-header h4 {
  margin: 0;
  font-size: 13px;
  font-weight: 700;
  color: #d4d4d8;
  letter-spacing: .02em;
}
.instance-id {
  font-family: ui-monospace, monospace;
  color: #38bdf8;
  font-size: 12px;
  margin-left: 4px;
  font-weight: 400;
}

/* jack bay */
.jack-rack-row {
  display: inline-flex;
  gap: 20px;
  background: #050507;
  padding: 10px 14px;
  border-radius: 6px;
  margin-bottom: 12px;
  border: 1px solid #1c1c1f;
}

.jack { display: flex; flex-direction: column; align-items: center; gap: 5px; cursor: pointer; }

.jack-socket {
  width: 26px; height: 26px;
  background: radial-gradient(circle, #000 30%, #3f3f46 70%, #18181b 100%);
  border: 3px solid #52525b;
  border-radius: 50%;
  box-shadow: inset 0 2px 4px rgba(0,0,0,.9), 0 1px 2px rgba(255,255,255,.05);
  transition: transform .08s, border-color .15s, box-shadow .15s;
}
.in-jack  .jack-socket { border-color: #2563eb; }
.out-jack .jack-socket { border-color: #059669; }
.jack:hover .jack-socket { transform: scale(1.15); }

.in-jack:hover  .jack-socket { border-color: #60a5fa; box-shadow: inset 0 2px 4px rgba(0,0,0,.9), 0 0 8px rgba(96,165,250,.5); }
.out-jack:hover .jack-socket { border-color: #34d399; box-shadow: inset 0 2px 4px rgba(0,0,0,.9), 0 0 8px rgba(52,211,153,.5); }

.pulse-highlight .jack-socket { animation: socket-glow 1.2s infinite ease-in-out; }
@keyframes socket-glow {
  0%,100% { border-color: #2563eb; box-shadow: 0 0 4px #2563eb; }
  50%      { border-color: #60a5fa; box-shadow: 0 0 14px rgba(96,165,250,.8); }
}

.jack-label { font-size: 9px; font-weight: 700; font-family: monospace; color: #52525b; letter-spacing: .05em; }

/* sliders */
.sliders-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(190px, 1fr)); gap: 10px; }
.slider-box { background: #0c0c0e; padding: 10px; border-radius: 6px; border: 1px solid #1c1c1f; }
.control-wrapper { display: flex; flex-direction: column; gap: 6px; }
.param-title {
  font-size: 11px;
  font-weight: 700;
  color: #71717a;
  display: flex;
  justify-content: space-between;
  text-transform: uppercase;
  letter-spacing: .04em;
}
.val-metric { color: #38bdf8; font-family: ui-monospace, monospace; font-weight: 400; }

/* keyboard */
.keyboard-container {
  background: #0c0c0e;
  border: 1px solid #27272a;
  border-radius: 10px;
  padding: 12px 12px 10px;
  margin-bottom: 12px;
  box-shadow: inset 0 2px 6px rgba(0,0,0,.5);
}
.keyboard-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 10px;
}
.section-label {
  font-size: 11px;
  font-weight: 700;
  color: #52525b;
  text-transform: uppercase;
  letter-spacing: .08em;
}
.keyboard-hint {
  font-size: 11px;
  color: #3f3f46;
  font-style: italic;
}
.keyboard-scroll { overflow-x: auto; padding-bottom: 6px; }
.keyboard-scroll::-webkit-scrollbar { height: 5px; }
.keyboard-scroll::-webkit-scrollbar-track { background: #050507; border-radius: 3px; }
.keyboard-scroll::-webkit-scrollbar-thumb { background: #27272a; border-radius: 3px; }
.keyboard { display: inline-flex; height: 140px; position: relative; }

.key {
  cursor: pointer;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: flex-end;
  padding-bottom: 5px;
  border-radius: 0 0 5px 5px;
  flex-shrink: 0;
  transition: background .04s, transform .04s;
}
.white {
  width: 40px; height: 100%; z-index: 1; margin: 0 1px;
  background: linear-gradient(to bottom, #d4d4d8, #a1a1aa);
  border: 1px solid #52525b;
  border-top: none;
  color: #3f3f46;
}
.white.active, .white:active {
  background: linear-gradient(to bottom, #60a5fa, #2563eb);
  color: #fff;
  transform: translateY(2px);
  box-shadow: 0 0 10px rgba(96,165,250,.5);
}
.black {
  width: 25px; height: 60%; z-index: 2;
  margin-left: -13px; margin-right: -13px;
  background: linear-gradient(to bottom, #18181b, #09090b);
  border: 1px solid #000;
  border-top: none;
  color: #71717a;
}
.black.active, .black:active {
  background: linear-gradient(to bottom, #2563eb, #1d4ed8);
  color: #fff;
  transform: translateY(2px);
  box-shadow: 0 0 8px rgba(37,99,235,.6);
}
.key-label   { font-size: 7px; font-weight: 700; pointer-events: none; }
.key-shortcut { font-size: 8px; font-weight: 700; pointer-events: none; color: #f59e0b; font-family: monospace; }
.key {
  /* stop blue overlay highlighting on tap for mobile */
  -webkit-tap-highlight-color: transparent;
  
  /* stop system context menus and text selections while holding keys */
  -webkit-user-select: none;
  user-select: none;
}

/* utility */
.locked { opacity: .25; pointer-events: none; }

/* svg cable canvas */
.patch-cable-canvas {
  position: fixed; top: 0; left: 0;
  width: 100vw; height: 100vh;
  pointer-events: none;
  z-index: 9999;
  overflow: visible;
}
.patch-wire {
  fill: none;
  stroke: #ea580c;
  stroke-width: 5px;
  stroke-linecap: round;
  filter: drop-shadow(0 3px 6px rgba(0,0,0,.6));
  pointer-events: auto;
  cursor: pointer;
}
.patch-wire:hover { stroke: #f97316; filter: drop-shadow(0 0 6px rgba(249,115,22,.7)); }
.preview-wire {
  stroke: #10b981;
  stroke-dasharray: 8 5;
  opacity: .8;
  pointer-events: none;
}
</style>
