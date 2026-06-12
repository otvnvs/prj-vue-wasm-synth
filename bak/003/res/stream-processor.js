// res/stream-processor.js
class WasmStreamProcessor extends AudioWorkletProcessor {
    constructor() {
        super();
        this.isReady = false;
        this.waveType = 0; // Default: Sine
        
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
            // Update the selected waveform type on the fly
            if (event.data.type === 'SET_WAVE_TYPE') {
                this.waveType = event.data.value;
                if (this.wasm && this.wasm.reset_stream_phase) {
                    this.wasm.reset_stream_phase();
                }
            }
        };
    }

    process(inputs, outputs, parameters) {
        if (!this.isReady) return true;
        const channel = outputs[0][0];
        if (!channel) return true;

        // Call the streamlined multi-oscillator controller
        this.wasm.stream_wave(this.waveType, this.bufferPtr, channel.length, 440.0, sampleRate);
        channel.set(this.audioHeap);
        return true; 
    }
}
registerProcessor('wasm-stream-processor', WasmStreamProcessor);

