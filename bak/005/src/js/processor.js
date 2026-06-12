// src/js/processor.js

class WasmStreamProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.isReady = false;
        this.waveType = 0;
        this.frequency = 440.0;
        this.volume = 0.5;
        
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
                } catch (err) {
                    console.error("Isolated AudioWorklet thread instantiation error:", err);
                }
            }
            if (event.data.type === 'SET_WAVE_TYPE') this.waveType = event.data.value;
            if (event.data.type === 'SET_FREQUENCY') this.frequency = parseFloat(event.data.value);
            if (event.data.type === 'SET_VOLUME') this.volume = parseFloat(event.data.value);
        };
    }

    process(inputs, outputs, parameters) {
        if (!this.isReady) return true;
        const channel = outputs[0][0]; // Extract Left/Mono channel reference array
        if (!channel) return true;

        // Drive the modular multi-oscillator dynamic wrapper core via linear WASM memory references
        this.wasm.stream_wave(this.waveType, this.bufferPtr, channel.length, this.frequency, sampleRate, this.volume);
        
        channel.set(this.audioHeap);
        return true; 
    }
}

registerProcessor('wasm-stream-processor', WasmStreamProcessor);

