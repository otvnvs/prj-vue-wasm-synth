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
     * Hot-loads any WASM binary block and returns its complete hardware-profile manifest
     */
    async loadPluginNodeAndDiscover(nodeId, wasmUrl, nodeType) {
        if (!this.isInitialized) return null;
        
        const response = await fetch(wasmUrl);
        const binaryBuffer = await response.arrayBuffer();

        // 1. Instantiation for structural parsing extraction
        const imports = { env: { memory: new WebAssembly.Memory({ initial: 256, maximum: 256 }) } };
        const { instance } = await WebAssembly.instantiate(binaryBuffer, imports);
        const exports = instance.exports;

        let pluginProfile = {
            name: "Unknown Node",
            idTag: "generic",
            type: nodeType,
            inputs: 0,
            outputs: 0,
            parameters: []
        };

        if (exports.get_plugin_manifest) {
            const stringPointer = exports.get_plugin_manifest();
            const liveMemoryView = new Uint8Array(exports.memory.buffer);
            
            let jsonString = "";
            let ptrOffset = stringPointer;
            
            while (liveMemoryView[ptrOffset] !== 0) {
                jsonString += String.fromCharCode(liveMemoryView[ptrOffset]);
                ptrOffset++;
            }

            try {
                const manifest = JSON.parse(jsonString);
                pluginProfile.name = manifest.name || pluginProfile.name;
                pluginProfile.idTag = manifest.id_tag || pluginProfile.idTag;
                pluginProfile.type = manifest.type || pluginProfile.type;
                pluginProfile.inputs = manifest.inputs !== undefined ? manifest.inputs : pluginProfile.inputs;
                pluginProfile.outputs = manifest.outputs !== undefined ? manifest.outputs : pluginProfile.outputs;
                
                if (manifest.parameters) {
                    pluginProfile.parameters = manifest.parameters.map(p => ({
                        id: p.id,
                        name: p.name,
                        min: p.min,
                        max: p.max,
                        value: p.default 
                    }));
                }
            } catch (jsonParseError) {
                console.error(`Malformed JSON text structure on node [${nodeId}]:`, jsonParseError);
            }
        }

        // 2. Post the raw binary code straight to the background audio thread
        this.graphNode.port.postMessage({
            type: 'HOT_LOAD_NODE',
            nodeId: nodeId,
            nodeType: pluginProfile.type, // Sync to manifest classification
            wasmBinary: binaryBuffer
        });

        return pluginProfile;
    }

    patchCable(sourceId, targetId) {
	    console.log("index:patchCable");
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'PATCH_CABLE', sourceId, targetId });
    }

    unpatchCable(sourceId, targetId) {
	    console.log("index:unpatchCable");
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'UNPATCH_CABLE', sourceId, targetId });
    }

    setNodeParameter(nodeId, paramId, value) {
	    console.log("index:setNodeParameter");
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'SET_NODE_PARAM', nodeId, paramId, value });
    }

    noteOn(nodeId, keyId, frequency, volume = 0.4) {
	    console.log("index:noteOn");
        if (!this.isInitialized){console.log("index:noteOn:not initialized"); return;}
	    console.log("index:noteOn:initialized");
        this.graphNode.port.postMessage({ type: 'TRIGGER_NOTE_ON', nodeId, keyId, frequency, volume });
    }

    noteOff(nodeId, keyId) {
	    console.log("index:noteOff");
        if (!this.isInitialized) return;
        if (!this.isInitialized){console.log("index:noteOff:not initialized"); return;}
	    console.log("index:noteOff:initialized");
        this.graphNode.port.postMessage({ type: 'TRIGGER_NOTE_OFF', nodeId, keyId });
    }

    disconnectAll() {
	    console.log("index:disconnectAll");
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'DISCONNECT_GRAPH' });
    }
}

