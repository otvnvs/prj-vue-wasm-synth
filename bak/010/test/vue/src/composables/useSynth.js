// test/vue/src/composables/useSynth.js
import { ref, onBeforeUnmount } from 'vue';
import { GraphSynthEngine } from '../../../../dist/index.js';

import synthWasmUrl from '../../../../dist/plugin_synth.wasm?url';
import echoWasmUrl from '../../../../dist/plugin_echo.wasm?url';
import filterWasmUrl from '../../../../dist/plugin_filter.wasm?url';
import processorUrl from '../../../../dist/stream-processor.js?url';

export function useSynth() {
  const engine = ref(null);
  const isReady = ref(false);
  const activeConnections = ref([]);
  
  // Dynamic parameters mapping registry holding parsed JSON manifests
  const loadedNodes = ref({});

  const init = async () => {
    if (engine.value) return;

    try {
      engine.value = new GraphSynthEngine(processorUrl);
      await engine.value.init();

      // 1. Hot-load modules AND unpack their internal JSON metadata configurations
      loadedNodes.value['synth_1'] = await engine.value.loadPluginNodeAndDiscover('synth_1', synthWasmUrl, 'synth');
      loadedNodes.value['echo_1']  = await engine.value.loadPluginNodeAndDiscover('echo_1', echoWasmUrl, 'effect');
      loadedNodes.value['filter_1']= await engine.value.loadPluginNodeAndDiscover('filter_1', filterWasmUrl, 'effect');

      // 2. Synchronize initial default values down onto the WebAssembly memory state
      Object.keys(loadedNodes.value).forEach(nodeId => {
          loadedNodes.value[nodeId].forEach(param => {
              engine.value.setNodeParameter(nodeId, param.id, param.value);
          });
      });

      isReady.value = true;
      console.log('Automated JSON manifest tracking initialization successful.');
    } catch (err) {
      console.error('Failed graph initialization inside composable:', err);
    }
  };

  /**
   * Generic parametric router targeting dynamic node slots on the fly
   */
  const updateNodeParameter = (nodeId, paramId, value) => {
      if (!engine.value) return;
      
      // Update our local reactive tracking value inside Vue
      const nodeParams = loadedNodes.value[nodeId];
      if (nodeParams) {
          const param = nodeParams.find(p => p.id === paramId);
          if (param) param.value = parseFloat(value);
      }

      // Pass the value straight down to WebAssembly audio thread
      engine.value.setNodeParameter(nodeId, paramId, value);
  };

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
    loadedNodes,
    init,
    connectNodes,
    resetPatchbay,
    updateNodeParameter,
    noteOn,
    noteOff
  };
}

