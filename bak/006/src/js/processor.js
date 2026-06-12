// src/js/processor.js
class WasmStreamProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.isReady = false;
        
        // NEW: An internal queue to store events if keys are pressed 
        // before the WASM compilation completes.
        this.pendingMessages = [];
        
        this.port.onmessage = async (event) => {
            if (event.data.type === 'INIT_WASM') {
                const wasmBytes = event.data.binary;
                const imports = { env: { memory: new WebAssembly.Memory({ initial: 256, maximum: 256 }) } };
                try {
                    const { instance } = await WebAssembly.instantiate(wasmBytes, imports);
                    this.wasm = instance.exports;
                    if (this.wasm._initialize) this.wasm._initialize();
                    
                    this.bufferSize = 128; 
                    this.bufferPtr = this.wasm.malloc(this.bufferSize * 4);
                    this.audioHeap = new Float32Array(this.wasm.memory.buffer, this.bufferPtr, this.bufferSize);
                    
                    this.isReady = true;
                    this.port.postMessage({ type: 'STATUS', status: 'READY' });

                    // NEW: Process any key-press commands that were queued up during compilation
                    while (this.pendingMessages.length > 0) {
                        const msg = this.pendingMessages.shift();
                        this.executeAudioMessage(msg);
                    }
                } catch (err) {
                    console.error("Instantiation failed inside AudioWorklet context:", err);
                }
                return;
            }
            
            // If the WebAssembly runtime is ready, process the message immediately.
            // Otherwise, push it into the queue.
            if (this.isReady) {
                this.executeAudioMessage(event.data);
            } else {
                this.pendingMessages.push(event.data);
            }
        };
    }

    // NEW: Centralized message execution routing block
    executeAudioMessage(data) {
        if (data.type === 'NOTE_ON') {
            this.wasm.note_on(data.keyId, data.waveType, data.frequency, data.volume);
        }
        if (data.type === 'NOTE_OFF') {
            this.wasm.note_off(data.keyId);
        }
    }

    process(inputs, outputs, parameters) {
        if (!this.isReady) return true; // Keep thread alive, outputting silence while initializing
        const channel = outputs[0]?.[0]; // Extract the mono track channel buffer array safely
        if (!channel) return true;

        // Drive the dynamic polyphonic linked-list synthesis block
        this.wasm.stream_mix_polyphonic(this.bufferPtr, channel.length, sampleRate);
        
        // Push frames directly to hardware output view
        channel.set(this.audioHeap);
        return true; 
    }
}

registerProcessor('wasm-stream-processor', WasmStreamProcessor);

