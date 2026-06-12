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
  
  const activeNodes = ref({});        
  const activeConnections = ref([]);  

  const libraryUrlMap = {
    'synth_core': synthWasmUrl,
    'tape_echo': echoWasmUrl,
    'resonant_filter': filterWasmUrl
  };

  const init = async () => {
    if (engine.value) return;

    try {
      engine.value = new GraphSynthEngine(processorUrl);
      await engine.value.init();

      // 1. AUTOMATED PRE-LOAD: Spawn all 3 standalone components on startup
      await spawnNode('synth_core', 'synth_1');
      await spawnNode('resonant_filter', 'filter_1');
      await spawnNode('tape_echo', 'echo_1');

      // 2. SANE DEFAULTS: Wire the hardware modules into a classic cascading synth chain
      // Path: Synth ➔ Filter ➔ Echo ➔ Speakers
      plugCable('synth_1', 'filter_1');
      plugCable('filter_1', 'echo_1');
      plugCable('echo_1', 'main_output');

      isReady.value = true;
      console.log('Rack workspace booted with sane cascading default configurations.');
    } catch (err) {
      console.error('Failed graph configuration initialization:', err);
    }
  };

  const spawnNode = async (pluginClassId, desiredNodeId) => {
    if (!engine.value || activeNodes.value[desiredNodeId]) return;

    const targetedUrl = libraryUrlMap[pluginClassId];
    if (!targetedUrl) return;

    const profile = await engine.value.loadPluginNodeAndDiscover(desiredNodeId, targetedUrl, 'effect');
    
    if (profile) {
      activeNodes.value[desiredNodeId] = profile;
      profile.parameters.forEach(param => {
        engine.value.setNodeParameter(desiredNodeId, param.id, param.value);
      });
    }
  };

  const removeNode = (nodeId) => {
    if (!engine.value || !activeNodes.value[nodeId]) return;

    activeConnections.value = activeConnections.value.filter(edge => {
      if (edge.sourceId === nodeId || edge.targetId === nodeId) {
        engine.value.unpatchCable(edge.sourceId, edge.targetId);
        return false;
      }
      return true;
    });

    delete activeNodes.value[nodeId];
  };

  const plugCable = (sourceId, targetId) => {
    if (!engine.value) return;
    const linkExists = activeConnections.value.some(c => c.sourceId === sourceId && c.targetId === targetId);
    if (linkExists) return;

    engine.value.patchCable(sourceId, targetId);
    activeConnections.value.push({ sourceId, targetId });
  };

  const unplugCable = (sourceId, targetId) => {
    if (!engine.value) return;
    engine.value.unpatchCable(sourceId, targetId);
    activeConnections.value = activeConnections.value.filter(c => !(c.sourceId === sourceId && c.targetId === targetId));
  };

  const updateParam = (nodeId, paramId, value) => {
    if (!engine.value) return;
    const node = activeNodes.value[nodeId];
    if (node) {
      const p = node.parameters.find(item => item.id === paramId);
      if (p) p.value = parseFloat(value);
    }
    engine.value.setNodeParameter(nodeId, paramId, value);
  };

  const noteOn = (keyId, freq) => {
    Object.keys(activeNodes.value).forEach(nodeId => {
      if (activeNodes.value[nodeId].type === 'generator' && engine.value) {
        engine.value.noteOn(nodeId, keyId, freq);
      }
    });
  };

  const noteOff = (keyId) => {
    Object.keys(activeNodes.value).forEach(nodeId => {
      if (activeNodes.value[nodeId].type === 'generator' && engine.value) {
        engine.value.noteOff(nodeId, keyId);
      }
    });
  };

  onBeforeUnmount(() => {
    if (engine.value) {
      engine.value.disconnectAll();
      engine.value = null;
    }
  });

  return {
    isReady,
    activeNodes,
    activeConnections,
    init,
    spawnNode,
    removeNode,
    plugCable,
    unplugCable,
    updateParam,
    noteOn,
    noteOff
  };
}

