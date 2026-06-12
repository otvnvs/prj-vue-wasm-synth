// stream-processor.js
class WasmStreamProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.isReady = false;
        
        this.port.onmessage = async (event) => {
            if (event.data.type === 'INIT_WASM') {
                const wasmBytes = event.data.binary;
                
                // Pure standalone WASM does not expect any external environment imports
                const imports = {
                    env: {
                        memory: new WebAssembly.Memory({ initial: 256, maximum: 256 })
                    }
                };

                try {
                    // Try instantiating completely raw or with bare minimums
                    const { instance } = await WebAssembly.instantiate(wasmBytes, imports);
                    this.wasm = instance.exports;
                    
                    // Standalone targets export memory directly out to JavaScript
                    this.memoryBuffer = this.wasm.memory.buffer;
                    
                    this.bufferSize = 128; 
                    this.bufferPtr = this.wasm.malloc(this.bufferSize * 4);
                    
                    // Map a secure float view onto the WASM instance's internal memory
                    this.audioHeap = new Float32Array(this.memoryBuffer, this.bufferPtr, this.bufferSize);
                    
                    this.isReady = true;
                    this.port.postMessage({ type: 'STATUS', status: 'READY' });
                } catch (err) {
                    console.error("Failed to load WASM inside AudioWorklet:", err);
                }
            }
        };
    }

    process(inputs, outputs, parameters) {
        if (!this.isReady) return true;

        const output = outputs[0];
        const channel = output[0]; // Left/Mono channel
        if (!channel) return true;

        const sampleCount = channel.length; // 128 samples
        const frequency = 440.0;
        const currentSampleRate = sampleRate; 

        // Call the streaming C logic
        this.wasm.stream_sine_wave(this.bufferPtr, sampleCount, frequency, currentSampleRate);

        // Copy the audio heap frame data directly to the soundcard channel
        channel.set(this.audioHeap);

        return true; 
    }
}

registerProcessor('wasm-stream-processor', WasmStreamProcessor);

