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
        
        await this.audioCtx.audioWorklet.addModule(this.processorUrl);
        this.graphNode = new AudioWorkletNode(this.audioCtx, 'wasm-graph-processor');
        this.graphNode.connect(this.audioCtx.destination);
        
        this.isInitialized = true;
    }

    /**
     * Hot-loads a module AND parses its internal C metadata structures to discover parameters.
     * @returns {Promise<Array<{id: number, name: string, min: number, max: number, value: number}>>}
     */
    // Inside src/js/index.js
    // Inside src/js/index.js
    // Inside src/js/index.js
    async loadPluginNodeAndDiscover(nodeId, wasmUrl, nodeType) {
        if (!this.isInitialized) return [];
        
        const response = await fetch(wasmUrl);
        const binaryBuffer = await response.arrayBuffer();

        // 1. Instantiation for structural parsing extraction
        const imports = { env: { memory: new WebAssembly.Memory({ initial: 256, maximum: 256 }) } };
        const { instance } = await WebAssembly.instantiate(binaryBuffer, imports);
        const exports = instance.exports;

        let discoveredParameters = [];

        if (exports.get_plugin_manifest) {
            // Get the raw memory address pointer index from C
            const stringPointer = exports.get_plugin_manifest();
            
            // FIXED: Create a fresh, live window view over the active WASM memory buffer heap
            // This prevents reading from a detached or invalidated buffer reference.
            const liveMemoryView = new Uint8Array(exports.memory.buffer);
            
            let jsonString = "";
            let ptrOffset = stringPointer;
            
            // Scan linearly until we find the null terminator (\0) string ending
            while (liveMemoryView[ptrOffset] !== 0) {
                jsonString += String.fromCharCode(liveMemoryView[ptrOffset]);
                ptrOffset++;
            }

            try {
                // Parse the verified JSON payload string
                const manifest = JSON.parse(jsonString);
                
                // Map to our reactive Vue template slider schema layout structure
                if (manifest.parameters) {
                    discoveredParameters = manifest.parameters.map(p => ({
                        id: p.id,
                        name: p.name,
                        min: p.min,
                        max: p.max,
                        value: p.default 
                    }));
                }
            } catch (jsonParseError) {
                console.error(`Malformed JSON text configuration structure discovered on node [${nodeId}]:`, jsonParseError);
                console.log("Raw string attempted:", jsonString);
            }
        }

        // 2. Pass the compiled binary buffer block down onto the audio hardware background streaming thread
        this.graphNode.port.postMessage({
            type: 'HOT_LOAD_NODE',
            nodeId: nodeId,
            nodeType: nodeType,
            wasmBinary: binaryBuffer
        });

        return discoveredParameters;
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

