// test/vue/src/composables/useSynth.js
import { ref, onBeforeUnmount } from 'vue';
import { GraphSynthEngine } from '../../../../dist/index.js';

// Load our unbundled WASM binaries using Vite URL mapping parameters
import synthWasmUrl from '../../../../dist/plugin_synth.wasm?url';
import echoWasmUrl from '../../../../dist/plugin_echo.wasm?url';
import processorUrl from '../../../../dist/stream-processor.js?url';
import filterWasmUrl from '../../../../dist/plugin_filter.wasm?url'

export function useSynth() {
  const engine = ref(null);
  const isReady = ref(false);
  const activeConnections = ref([]);

  // Node Variable Trackers (Cached Locally for UI Sliders)
  const attack = ref(0.05);
  const decay = ref(0.15);
  const sustain = ref(0.60);
  const release = ref(0.40);
  const waveType = ref(0);

  const echoTime = ref(0.3);
  const echoFeedback = ref(0.4);
  const echoMix = ref(0.35);

  const filterCutoff = ref(800);
  const filterResonance = ref(1.0);

  const init = async () => {
    if (engine.value) return;

    try {
      engine.value = new GraphSynthEngine(processorUrl);
      await engine.value.init();

      // 1. Hot-load our standalone binaries into the active background thread map
      await engine.value.loadPluginNode('synth_1', synthWasmUrl, 'synth');
      await engine.value.loadPluginNode('echo_1', echoWasmUrl, 'effect');
      await engine.value.loadPluginNode('filter_1', filterWasmUrl, 'effect');


      // 2. Sync initial structural variable values to WebAssembly
      syncParameters();

      isReady.value = true;
      console.log('Modular graph engine successfully mounted inside composable.');
    } catch (err) {
      console.error('Failed to initialize the graph synth composable:', err);
    }
  };

  const syncParameters = () => {
    if (!engine.value) return;
    engine.value.setNodeParameter('synth_1', 0, attack.value);
    engine.value.setNodeParameter('synth_1', 1, decay.value);
    engine.value.setNodeParameter('synth_1', 2, sustain.value);
    engine.value.setNodeParameter('synth_1', 3, release.value);
    engine.value.setNodeParameter('synth_1', 4, waveType.value);

    engine.value.setNodeParameter('echo_1', 0, echoTime.value);
    engine.value.setNodeParameter('echo_1', 1, echoFeedback.value);
    engine.value.setNodeParameter('echo_1', 2, echoMix.value);

    engine.value.setNodeParameter('filter_1', 0, filterCutoff.value);
    engine.value.setNodeParameter('filter_1', 1, filterResonance.value);
  };

  // Graph Modulators
  const connectNodes = (source, target) => {
    if (!engine.value) return;
    engine.value.patchCable(source, target);
    activeConnections.value.push({ source, target });
  };

  const resetPatchbay = () => {
    if (!engine.value) return;
    engine.value.disconnectAll();
    activeConnections.value = [];
  };

  const setParam = (nodeId, paramId, val) => {
    if (!engine.value) return;
    engine.value.setNodeParameter(nodeId, paramId, parseFloat(val));
  };

  // Keyboard TriggerProxies
  const noteOn = (keyId, freq) => {
    if (engine.value) engine.value.noteOn('synth_1', keyId, freq);
  };

  const noteOff = (keyId) => {
    if (engine.value) engine.value.noteOff('synth_1', keyId);
  };

  onBeforeUnmount(() => {
    if (engine.value) {
      resetPatchbay();
      engine.value = null;
    }
  });

  return {
    isReady,
    activeConnections,
    attack, decay, sustain, release, waveType,
    echoTime, echoFeedback, echoMix,
    filterCutoff, filterResonance,
    init,
    connectNodes,
    resetPatchbay,
    setParam,
    noteOn,
    noteOff
  };
}

