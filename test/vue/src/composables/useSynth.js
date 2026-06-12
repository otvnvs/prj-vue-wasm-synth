import { ref, onBeforeUnmount } from "vue";
import { GraphSynthEngine } from "../assets/wasm/index.js";
import synthWasmUrl from "../assets/wasm/synth.wasm?url";
import synth2WasmUrl from "../assets/wasm/synth2.wasm?url";
import echoWasmUrl from "../assets/wasm/echo.wasm?url";
import filterWasmUrl from "../assets/wasm/filter.wasm?url";
import flangerWasmUrl from "../assets/wasm/flanger.wasm?url";
import waveshaperWasmUrl from "../assets/wasm/waveshaper.wasm?url";
import auto_wahWasmUrl from "../assets/wasm/auto_wah.wasm?url";
import processorUrl from "../assets/wasm/stream-processor.js?url";

export function useSynth() {
    const engine = ref(null);
    const isReady = ref(false);
    const activeNodes = ref({});
    const activeConnections = ref([]);

    const binaryRegistry = {
        synth_core: synthWasmUrl,
        organ_blender: synth2WasmUrl,
        tape_echo: echoWasmUrl,
        resonant_filter: filterWasmUrl,
        flanger: flangerWasmUrl,
        waveshaper: waveshaperWasmUrl,
        auto_wah: auto_wahWasmUrl
    };

    const init = async (useOrganSynth = false) => {
        if (engine.value) return;
        try {
            engine.value = new GraphSynthEngine(processorUrl);
            await engine.value.init();

            const activeGeneratorKey = useOrganSynth ? "organ_blender" : "synth_core";
            await spawnNode(activeGeneratorKey, "synth_1", "generator");
            await spawnNode("resonant_filter", "filter_1", "effect");
            await spawnNode("tape_echo", "echo_1", "effect");

            plugCable("synth_1", "filter_1");
            plugCable("filter_1", "echo_1");
            plugCable("echo_1", "main_output");

            isReady.value = true;
            console.log("Rack workspace booted cleanly with resolved asset routes.");
        } catch (err) {
            console.error("Failed graph configuration initialization:", err);
        }
    };

    const spawnNode = async (binaryType, nodeId, nodeType) => {
        if (!engine.value || activeNodes.value[nodeId]) return;
        const targetUrl = binaryRegistry[binaryType];
        if (!targetUrl) return;

        const discoveredManifest = await engine.value.loadPluginNodeAndDiscover(nodeId, targetUrl, nodeType);
        if (discoveredManifest) {
            activeNodes.value[nodeId] = discoveredManifest;
            discoveredManifest.parameters.forEach(param => {
                engine.value.setNodeParameter(nodeId, param.id, param.value);
            });
        }
    };

    const removeNode = (nodeId) => {
        if (!engine.value || !activeNodes.value[nodeId]) return;
        activeConnections.value = activeConnections.value.filter(conn => {
            if (conn.sourceId === nodeId || conn.targetId === nodeId) {
                engine.value.unpatchCable(conn.sourceId, conn.targetId);
                return false;
            }
            return true;
        });
        delete activeNodes.value[nodeId];
    };

    const plugCable = (sourceId, targetId) => {
        if (!engine.value || sourceId === targetId) return;
        const exists = activeConnections.value.some(c => c.sourceId === sourceId && c.targetId === targetId);
        if (exists) return;

        engine.value.patchCable(sourceId, targetId);
        activeConnections.value.push({
            id: `${sourceId}->${targetId}`,
            sourceId: sourceId,
            targetId: targetId
        });
    };

    const unplugCable = (sourceId, targetId) => {
        if (!engine.value) return;
        engine.value.unpatchCable(sourceId, targetId);
        activeConnections.value = activeConnections.value.filter(c => c.sourceId !== sourceId || c.targetId !== targetId);
    };

    const updateParam = (nodeId, paramId, value) => {
        if (!engine.value) return;
        const node = activeNodes.value[nodeId];
        if (node) {
            const p = node.parameters.find(x => x.id === paramId);
            if (p) p.value = parseFloat(value);
        }
        engine.value.setNodeParameter(nodeId, paramId, value);
    };

    const noteOn = (keyId, frequency, volume = 0.4) => {
        Object.keys(activeNodes.value).forEach(nodeId => {
            if (activeNodes.value[nodeId].type === "generator") {
                engine.value.noteOn(nodeId, keyId, frequency, volume);
            }
        });
    };

    const noteOff = (keyId) => {
        Object.keys(activeNodes.value).forEach(nodeId => {
            if (activeNodes.value[nodeId].type === "generator") {
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
