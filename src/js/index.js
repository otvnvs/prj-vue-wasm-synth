// src/js/index.js
const log = function(...args) {
	  //console.log(...args);
};
export class GraphSynthEngine {
    constructor(processorUrl) {
	    log("GraphSynthEngine:constructor:"+processorUrl);
        this.processorUrl = processorUrl;
        this.audioCtx = null;
        this.graphNode = null;
        this.isInitialized = false;
    }

    async init() {
	    log("GraphSynthEngine:init");
        if (this.isInitialized){
	    log("GraphSynthEngine:init:already initialized");
		return;
	}else{
	    log("GraphSynthEngine:init:initializing");
	}
	    log("GraphSynthEngine:init:0");
        this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
	    log("GraphSynthEngine:init:1");
        
        await this.audioCtx.audioWorklet.addModule(this.processorUrl);
	    log("GraphSynthEngine:init:2");
        this.graphNode = new AudioWorkletNode(this.audioCtx, 'wasm-graph-processor');
	    log("GraphSynthEngine:init:3");
        this.graphNode.connect(this.audioCtx.destination);
	    log("GraphSynthEngine:init:4");
        this.isInitialized = true;
	    log("GraphSynthEngine:init:5");
    }

    /**
     * Hot-loads any WASM binary block and returns its complete hardware-profile manifest
     */
    async loadPluginNodeAndDiscover(nodeId, wasmUrl, nodeType) {
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:",nodeId, wasmUrl, nodeType);
        if (!this.isInitialized){
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:already initialized");
		return null;
	}else{
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:initializing");
	}
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:0");
        const response = await fetch(wasmUrl);
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:1");
        const binaryBuffer = await response.arrayBuffer();
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:2");

        // 1. Instantiation for structural parsing extraction
        const imports = { env: { memory: new WebAssembly.Memory({ initial: 256, maximum: 256 }) } };
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:3");
        const { instance } = await WebAssembly.instantiate(binaryBuffer, imports);
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:4");
        const exports = instance.exports;
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:5");
        let pluginProfile = {
            name: "Unknown Node",
            idTag: "generic",
            type: nodeType,
            inputs: 0,
            outputs: 0,
            parameters: []
        };
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:6");

        if (exports.get_plugin_manifest) {
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:7");
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
        }else{
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:8");
	}

	    log("GraphSynthEngine:loadPluginNodeAndDiscover:9");
        // 2. Post the raw binary code straight to the background audio thread
        this.graphNode.port.postMessage({
            type: 'HOT_LOAD_NODE',
            nodeId: nodeId,
            nodeType: pluginProfile.type, // Sync to manifest classification
            wasmBinary: binaryBuffer
        });
	    log("GraphSynthEngine:loadPluginNodeAndDiscover:10");

        return pluginProfile;
    }

    patchCable(sourceId, targetId) {
	    log("GraphSynthEngine:patchCable:",sourceId, targetId);
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'PATCH_CABLE', sourceId, targetId });
    }

    unpatchCable(sourceId, targetId) {
	    log("GraphSynthEngine:unpatchCable:",sourceId, targetId);
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'UNPATCH_CABLE', sourceId, targetId });
    }

    setNodeParameter(nodeId, paramId, value) {
	    log("GraphSynthEngine:setNodeParameter:",nodeId, paramId, value);
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'SET_NODE_PARAM', nodeId, paramId, value });
    }

    noteOn(nodeId, keyId, frequency, volume = 0.4) {
	    log("GraphSynthEngine:noteOn:",nodeId, keyId, frequency, volume);
        if (!this.isInitialized){console.error("index:noteOn:not initialized"); return;}
        this.graphNode.port.postMessage({ type: 'TRIGGER_NOTE_ON', nodeId, keyId, frequency, volume });
    }

    noteOff(nodeId, keyId) {
	    log("GraphSynthEngine:noteOff:",nodeId, keyId);
        if (!this.isInitialized) return;
        if (!this.isInitialized){console.error("index:noteOff:not initialized"); return;}
        this.graphNode.port.postMessage({ type: 'TRIGGER_NOTE_OFF', nodeId, keyId });
    }

    disconnectAll() {
	    log("GraphSynthEngine:disconnectAll");
        if (!this.isInitialized) return;
        this.graphNode.port.postMessage({ type: 'DISCONNECT_GRAPH' });
    }
}

