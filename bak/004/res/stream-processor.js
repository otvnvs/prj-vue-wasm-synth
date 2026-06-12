// res/stream-processor.js
class WasmStreamProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.isReady = false;
        this.waveType = 0;   // Default: Sine
        this.frequency = 440.0; // Default: A4 pitch
        this.volume = 0.5; // Default: 50% volume
        
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
                    console.error("WASM worker thread error:", err);
                }
            }
            if (event.data.type === 'SET_WAVE_TYPE') {
                this.waveType = event.data.value;
            }
            // NEW: Catch real-time frequency changes from the UI slider
            if (event.data.type === 'SET_FREQUENCY') {
                this.frequency = parseFloat(event.data.value);
            }
            if (event.data.type === 'SET_VOLUME') {
                this.volume = parseFloat(event.data.value);
            }
        };
    }

    process(inputs, outputs, parameters) {
        if (!this.isReady) return true;
        const channel = outputs[0][0]; // Extract the safe left output channel array
        if (!channel) return true;

        // Pass the dynamic runtime frequency parameter straight into the compiled C math core
        this.wasm.stream_wave(this.waveType, this.bufferPtr, channel.length, this.frequency, sampleRate, this.volume);
        
        channel.set(this.audioHeap);
        return true; 
    }
}
registerProcessor('wasm-stream-processor', WasmStreamProcessor);

