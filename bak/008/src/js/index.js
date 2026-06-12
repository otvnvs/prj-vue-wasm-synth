// src/js/index.js

export class GraphSynthEngine {
    constructor(processorUrl) {
        this.processorUrl = processorUrl;
        this.audioCtx = null;
        this.graphNode = null;
        this.isInitialized = false;
    }

    async init() {
        if (this.isInitialized) return;
        this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        
        // Register our new master graph runner thread worker script
        await this.audioCtx.audioWorklet.addModule(this.processorUrl);
        this.graphNode = new AudioWorkletNode(this.audioCtx, 'wasm-graph-processor');
        this.graphNode.connect(this.audioCtx.destination);
        
        this.isInitialized = true;
        console.log("Modular Graph Synth Engine launched successfully.");
    }

    /**
     * Hot-loads a raw WebAssembly compiled asset directly into the active worker thread graph
     */
    async loadPluginNode(nodeId, wasmUrl, nodeType) {
        if (!this.isInitialized) return;
        
        const response = await fetch(wasmUrl);
        const binaryBuffer = await response.arrayBuffer();
        
        // Post the raw binary code straight to the background audio thread
        this.graphNode.port.postMessage({
            type: 'HOT_LOAD_NODE',
            nodeId: nodeId,
            nodeType: nodeType,
            wasmBinary: binaryBuffer
        });
    }

    patchCable(sourceId, targetId) {
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'PATCH_CABLE', sourceId, targetId });
    }

    setNodeParameter(nodeId, paramId, value) {
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'SET_NODE_PARAM', nodeId, paramId, value });
    }

    noteOn(nodeId, keyId, frequency, volume = 0.4) {
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'TRIGGER_NOTE_ON', nodeId, keyId, frequency, volume });
    }

    noteOff(nodeId, keyId) {
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'TRIGGER_NOTE_OFF', nodeId, keyId });
    }

    disconnectAll() {
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'DISCONNECT_GRAPH' });
    }
}

