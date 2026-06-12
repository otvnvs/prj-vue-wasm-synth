// src/js/processor.js

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
            'silent_input': new Float32Array(this.sampleBlockSize),
            'main_output': new Float32Array(this.sampleBlockSize)   
        };

        this.port.onmessage = async (event) => {
            const payload = event.data;

            if (payload.type === 'HOT_LOAD_NODE') {
                await this.registerWasmNode(payload.nodeId, payload.wasmBinary, payload.nodeType);
            }

            if (payload.type === 'SET_NODE_PARAM') {
                const node = this.nodes.get(payload.nodeId);
                if (node) {
                    node.wasmExports.set_parameter(node.instancePtr, payload.paramId, parseFloat(payload.value));
                }
            }

            if (payload.type === 'TRIGGER_NOTE_ON') {
                const node = this.nodes.get(payload.nodeId);
                if (node && node.type === 'synth') {
                    node.wasmExports.trigger_note_on(node.instancePtr, payload.keyId, parseFloat(payload.frequency), parseFloat(payload.volume));
                }
            }
            if (payload.type === 'TRIGGER_NOTE_OFF') {
                const node = this.nodes.get(payload.nodeId);
                if (node && node.type === 'synth') {
                    node.wasmExports.trigger_note_off(node.instancePtr, payload.keyId);
                }
            }

            if (payload.type === 'PATCH_CABLE') {
                this.connections.push({ sourceId: payload.sourceId, targetId: payload.targetId });
                this.recompileGraphTopology();
            }
            if (payload.type === 'DISCONNECT_GRAPH') {
                this.connections = [];
                this.executionOrder = [];
                this.buses['main_output'].fill(0.0);
            }
        };
    }

    async registerWasmNode(nodeId, wasmBytes, nodeType) {
        const imports = {
            env: {
                memory: new WebAssembly.Memory({ initial: 256, maximum: 256 })
            }
        };

        try {
            const { instance } = await WebAssembly.instantiate(wasmBytes, imports);
            const exports = instance.exports;

            if (exports._initialize) exports._initialize();

            const instancePtr = exports.create_instance();
            const blockPtr = exports.malloc(this.bufferSizeInBytes);
            const blockHeapView = new Float32Array(exports.memory.buffer, blockPtr, this.sampleBlockSize);

            const inputBufferPtr = exports.malloc(this.bufferSizeInBytes);
            const inputHeapView = new Float32Array(exports.memory.buffer, inputBufferPtr, this.sampleBlockSize);

            this.nodes.set(nodeId, {
                wasmExports: exports,
                instancePtr: instancePtr,
                type: nodeType,
                outputBufferPtr: blockPtr,
                outputHeapView: blockHeapView,
                inputBufferPtr: inputBufferPtr,
                inputHeapView: inputHeapView
            });

            this.buses[nodeId] = new Float32Array(this.sampleBlockSize);
            this.port.postMessage({ type: 'NODE_READY', nodeId: nodeId });
            this.recompileGraphTopology();
        } catch (err) {
            console.error(`Dynamic WebAssembly hot-load allocation exception on Node [${nodeId}]:`, err);
        }
    }

    /**
     * FIXED Topological Sort: Walks backwards from dependencies 
     * to guarantee that producers (synths) always execute BEFORE consumers (effects).
     */
    recompileGraphTopology() {
        const visited = new Set();
        const tempMarked = new Set();
        const sortedList = [];

        const visit = (nodeId) => {
            if (tempMarked.has(nodeId)) throw new Error("Cyclic audio loop connection detected!");
            if (!visited.has(nodeId)) {
                tempMarked.add(nodeId);
                
                // FIXED DIRECTION: Find upstream dependencies feeding INTO this node
                const incoming = this.connections.filter(edge => edge.targetId === nodeId);
                for (const edge of incoming) {
                    visit(edge.sourceId);
                }

                tempMarked.delete(nodeId);
                visited.add(nodeId);
                sortedList.push(nodeId); // Pushed sequentially
            }
        };

        try {
            // Also evaluate explicit destinations like main_output
            const destinations = new Set(this.connections.map(e => e.targetId));
            if (this.connections.some(e => e.targetId === 'main_output')) {
                destinations.add('main_output');
            }

            for (const nodeId of this.nodes.keys()) {
                destinations.add(nodeId);
            }

            for (const nodeId of destinations) {
                if (this.nodes.has(nodeId) && !visited.has(nodeId)) {
                    visit(nodeId);
                }
            }
            
            this.executionOrder = sortedList;
            console.log("DAG Compiled Order (Correct Generator->Effect sequence):", this.executionOrder);
        } catch (cycleError) {
            console.error("Audio patch topology compiler failure:", cycleError);
            this.connections.pop();
        }
    }
// src/js/processor.js (Update the process method)

    process(inputs, outputs) {
        const outputChannel = outputs?.[0]?.[0];
        if (!outputChannel) return true;

        // Reset the master output buffer channel
        this.buses['main_output'].fill(0.0);

        // Step sequentially through our topologically sorted execution list
        for (const nodeId of this.executionOrder) {
            const node = this.nodes.get(nodeId);
            if (!node) continue;

            // NEW OPTIMIZATION HOOK:
            // Check if this specific node is part of the currently active patch route.
            // A node is active if it has an incoming connection, an outgoing connection, 
            // or is a synth that points to an explicit output destination.
            const hasInputs = this.connections.some(edge => edge.targetId === nodeId);
            const hasOutputs = this.connections.some(edge => edge.sourceId === nodeId);
            
            // If the effect is completely isolated and bypassed in the current matrix, 
            // skip its processing cycles entirely to prevent silent data cancellation!
            if (!hasInputs && !hasOutputs && node.type !== 'synth') {
                continue;
            }

            // 1. Gather audio input blocks from parent buses
            node.inputHeapView.fill(0.0);
            if (hasInputs) {
                const parentConnections = this.connections.filter(edge => edge.targetId === nodeId);
                for (let e = 0; e < parentConnections.length; e++) {
                    const edge = parentConnections[e];
                    const sourceDataStream = this.buses[edge.sourceId];
                    if (sourceDataStream) {
                        for (let i = 0; i < this.sampleBlockSize; i++) {
                            node.inputHeapView[i] += sourceDataStream[i];
                        }
                    }
                }
            } else {
                node.inputHeapView.set(this.buses['silent_input']);
            }

            // 2. Invoke the WebAssembly C plugin execution function pointer
            node.wasmExports.process(
                node.instancePtr,
                node.inputBufferPtr,
                node.outputBufferPtr,
                this.sampleBlockSize,
                sampleRate
            );

            // 3. Mirror the output frames to the shared tracking bus map layer
            this.buses[nodeId].set(node.outputHeapView);

            // 4. Route audio to the speakers if the node is explicitly patched to main_output
            const connectsToSpeakers = this.connections.some(edge => edge.sourceId === nodeId && edge.targetId === 'main_output');
            if (connectsToSpeakers) {
                for (let i = 0; i < this.sampleBlockSize; i++) {
                    this.buses['main_output'][i] += node.outputHeapView[i];
                }
            }
        }

        // Send the completed mixed signal stream directly out to the hardware speakers
        outputChannel.set(this.buses['main_output']);
        return true;
    }

}

registerProcessor('wasm-graph-processor', WasmGraphProcessor);

