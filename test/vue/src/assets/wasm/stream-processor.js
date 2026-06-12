const log = function(...args) {
	//console.log(...args);
};

class WasmGraphProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.nodes = new Map();
        this.connections = [];
        this.executionOrder = [];
        this.sampleBlockSize = 128;
        this.bytesPerFloat = 4;
        this.bufferSizeInBytes = this.sampleBlockSize * this.bytesPerFloat;
        
        this.buses = {
            silent_input: new Float32Array(this.sampleBlockSize),
            main_output: new Float32Array(this.sampleBlockSize)
        };

        this.port.onmessage = async (e) => {
            const msg = e.data;
            if (msg.type === "HOT_LOAD_NODE") {
                await this.registerWasmNode(msg.nodeId, msg.wasmBinary, msg.nodeType);
            }
            if (msg.type === "SET_NODE_PARAM") {
                const node = this.nodes.get(msg.nodeId);
                if (node) node.wasmExports.set_parameter(node.instancePtr, msg.paramId, parseFloat(msg.value));
            }
            if (msg.type === "TRIGGER_NOTE_ON") {
                const node = this.nodes.get(msg.nodeId);
                if (node && node.type === "generator") {
                    node.wasmExports.trigger_note_on(node.instancePtr, msg.keyId, parseFloat(msg.frequency), parseFloat(msg.volume));
                }
            }
            if (msg.type === "TRIGGER_NOTE_OFF") {
                const node = this.nodes.get(msg.nodeId);
                if (node && node.type === "generator") {
                    node.wasmExports.trigger_note_off(node.instancePtr, msg.keyId);
                }
            }
            if (msg.type === "PATCH_CABLE") {
                this.connections.push({ sourceId: msg.sourceId, targetId: msg.targetId });
                this.recompileGraphTopology();
            }
            if (msg.type === "UNPATCH_CABLE") {
                this.connections = this.connections.filter(c => c.sourceId !== msg.sourceId || c.targetId !== msg.targetId);
                this.recompileGraphTopology();
            }
            if (msg.type === "DISCONNECT_GRAPH") {
                this.connections = [];
                this.executionOrder = [];
                this.buses.main_output.fill(0);
            }
        };
    }

    async registerWasmNode(nodeId, wasmBinary, nodeType) {
        const importObject = { env: { memory: new WebAssembly.Memory({ initial: 256, maximum: 256 }) } };
        try {
            const { instance } = await WebAssembly.instantiate(wasmBinary, importObject);
            const exports = instance.exports;
            if (exports._initialize) exports._initialize();
            
            const instancePtr = exports.create_instance();
            const outPtr = exports.malloc(this.bufferSizeInBytes);
            const outView = new Float32Array(exports.memory.buffer, outPtr, this.sampleBlockSize);
            const inPtr = exports.malloc(this.bufferSizeInBytes);
            const inView = new Float32Array(exports.memory.buffer, inPtr, this.sampleBlockSize);

            this.nodes.set(nodeId, {
                wasmExports: exports,
                instancePtr: instancePtr,
                type: nodeType,
                outputBufferPtr: outPtr,
                outputHeapView: outView,
                inputBufferPtr: inPtr,
                inputHeapView: inView
            });

            this.buses[nodeId] = new Float32Array(this.sampleBlockSize);
            this.port.postMessage({ type: "NODE_READY", nodeId: nodeId });
            this.recompileGraphTopology();
        } catch (err) {
            console.error(`Dynamic WebAssembly hot-load allocation exception on Node [${nodeId}]:`, err);
        }
    }

    recompileGraphTopology() {
        const visited = new Set();
        const tempMark = new Set();
        const order = [];

        const visit = (id) => {
            if (tempMark.has(id)) throw new Error("Cyclic audio loop connection detected!");
            if (!visited.has(id)) {
                tempMark.add(id);
                const incoming = this.connections.filter(c => c.targetId === id);
                for (const edge of incoming) {
                    visit(edge.sourceId);
                }
                tempMark.delete(id);
                visited.add(id);
                order.push(id);
            }
        };

        try {
            const targets = new Set(this.connections.map(c => c.targetId));
            if (this.connections.some(c => c.targetId === "main_output")) targets.add("main_output");
            for (const id of this.nodes.keys()) targets.add(id);
            for (const id of targets) {
                if (this.nodes.has(id) && !visited.has(id)) visit(id);
            }
            this.executionOrder = order;
        } catch (err) {
            console.error("Audio patch topology compiler failure:", err);
            this.connections.pop();
        }
    }

    process(inputs, outputs) {
        const outputChannel = outputs?.[0]?.[0];
        if (!outputChannel) return true;
        
        this.buses.main_output.fill(0);

        for (const nodeId of this.executionOrder) {
            const node = this.nodes.get(nodeId);
            if (!node) continue;

            const hasInput = this.connections.some(c => c.targetId === nodeId);
            const hasOutput = this.connections.some(c => c.sourceId === nodeId);
            if (!hasInput && !hasOutput && node.type !== "generator") continue;

            node.inputHeapView.fill(0);

            if (hasInput) {
                const inboundCables = this.connections.filter(c => c.targetId === nodeId);
                for (let i = 0; i < inboundCables.length; i++) {
                    const sourceBus = this.buses[inboundCables[i].sourceId];
                    if (sourceBus) {
                        for (let sample = 0; sample < this.sampleBlockSize; sample++) {
                            node.inputHeapView[sample] += sourceBus[sample];
                        }
                    }
                }
            } else {
                node.inputHeapView.set(this.buses.silent_input);
            }

            node.wasmExports.process(node.instancePtr, node.inputBufferPtr, node.outputBufferPtr, this.sampleBlockSize, sampleRate);
            this.buses[nodeId].set(node.outputHeapView);

            if (this.connections.some(c => c.sourceId === nodeId && c.targetId === "main_output")) {
                for (let sample = 0; sample < this.sampleBlockSize; sample++) {
                    this.buses.main_output[sample] += node.outputHeapView[sample];
                }
            }
        }

        outputChannel.set(this.buses.main_output);
        return true;
    }
}

registerProcessor("wasm-graph-processor", WasmGraphProcessor);
