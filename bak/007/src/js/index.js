// src/js/index.js

export class WebAudioSynth {
    /**
     * @param {Object} config 
     * @param {string} config.wasmUrl - Resolved path to the waveform.wasm binary
     * @param {string} config.processorUrl - Resolved path to the stream-processor.js script
     */
    constructor(config) {
        if (!config || !config.wasmUrl || !config.processorUrl) {
            throw new Error("WebAudioSynth requires both 'wasmUrl' and 'processorUrl' configuration strings.");
        }
        
        this.config = config;
        this.audioCtx = null;
        this.streamNode = null;
        this.isInitialized = false;
        
        // Runtime configurations synchronized with the C Core layout
        this.waveMap = { 'sine': 0, 'square': 1, 'sawtooth': 2 };
        this.activeWaveId = 0; 
        this.currentFrequency = 440.0;
        this.currentVolume = 0.5;
    }

    /**
     * Initializes the main thread audio constraints and pre-caches assets
     */
    async init() {
        if (this.isInitialized) return;
        
        // Setup cross-browser Audio Context safely
        this.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        this.isInitialized = true;
        console.log("WebAudioSynth base wrapper initialized successfully.");
    }

    /**
     * Updates active waveform parameter type
     * @param {'sine'|'square'|'sawtooth'} typeName 
     */
    setWaveform(typeName) {
        if (!this.waveMap.hasOwnProperty(typeName)) {
            console.warn(`Unknown waveform type "${typeName}". Defaulting to sine.`);
            typeName = 'sine';
        }
        this.activeWaveId = this.waveMap[typeName];
        if (this.streamNode) {
            this.streamNode.port.postMessage({ type: 'SET_WAVE_TYPE', value: this.activeWaveId });
        }
    }

    /**
     * Updates synthesis runtime frequency
     * @param {number} hzValue 
     */
    setFrequency(hzValue) {
        this.currentFrequency = parseFloat(hzValue);
        if (this.streamNode) {
            this.streamNode.port.postMessage({ type: 'SET_FREQUENCY', value: this.currentFrequency });
        }
    }

    /**
     * Updates amplitude volume scaling
     * @param {number} scalarValue - Volume level ranging from 0.0 to 1.0
     */
    setVolume(scalarValue) {
        this.currentVolume = Math.max(0.0, Math.min(1.0, parseFloat(scalarValue)));
        if (this.streamNode) {
            this.streamNode.port.postMessage({ type: 'SET_VOLUME', value: this.currentVolume });
        }
    }

    /**
     * Feature 1: Triggers a discrete snapshot evaluation output via static system heap
     * @param {number} duration - Clip sample runtime in seconds (defaults to 2.0)
     */
    /**
     * Feature 1: Triggers a discrete snapshot evaluation output via instance heap
     * @param {number} duration - Clip sample runtime in seconds (defaults to 2.0)
     */
    async playBuffer(duration = 2.0) {
        if (!this.isInitialized) throw new Error("Synth engine must be initialized via .init() before execution.");
        if (this.audioCtx.state === 'suspended') this.audioCtx.resume();

        // 1. Fetch and compile the WASM binary bytes on the main thread if not already cached
        if (!this.mainWasmInstance) {
            console.log("Compiling WebAssembly engine on main thread...");
            const response = await fetch(this.config.wasmUrl);
            const wasmBinary = await response.arrayBuffer();
            const imports = { env: { memory: new WebAssembly.Memory({ initial: 256, maximum: 256 }) } };
            
            const { instance } = await WebAssembly.instantiate(wasmBinary, imports);
            this.mainWasmInstance = instance.exports;
            
            if (this.mainWasmInstance._initialize) {
                this.mainWasmInstance._initialize();
            }
        }

        const sampleRate = this.audioCtx.sampleRate || 44100;
        const sampleCount = sampleRate * duration;
        
        // 2. Allocate memory using the instance's own isolated malloc utility
        const bufferPtr = this.mainWasmInstance.malloc(sampleCount * 4);

        // 3. Call the core multi-oscillator controller function
        this.mainWasmInstance.generate_wave(this.activeWaveId, bufferPtr, sampleCount, this.currentFrequency, sampleRate, this.currentVolume);

        // 4. Safely extract data from the active instance memory buffer
        const rawBuffer = this.mainWasmInstance.memory.buffer;
        const audioData = new Float32Array(rawBuffer, bufferPtr, sampleCount);

        // 5. Populate browser audio buffer and play back sound
        const audioBuffer = this.audioCtx.createBuffer(1, sampleCount, sampleRate);
        audioBuffer.getChannelData(0).set(audioData);

        const source = this.audioCtx.createBufferSource();
        source.buffer = audioBuffer;
        source.connect(this.audioCtx.destination);
        source.start();

        // 6. Free the buffer reference to prevent RAM leaks
        this.mainWasmInstance.free(bufferPtr);
    }

    /**
     * Feature 2: Spins up background thread pipeline for live streaming
     */
    async startStream() {
        if (!this.isInitialized) throw new Error("Synth engine must be initialized via .init() before execution.");
        if (this.audioCtx.state === 'suspended') this.audioCtx.resume();
        if (this.streamNode) return; // Stream already running

        try {
            // Register worker module using provided Vite configurable path string
            await this.audioCtx.audioWorklet.addModule(this.config.processorUrl);
            
            // Retrieve binary from custom asset routing path
            const response = await fetch(this.config.wasmUrl);
            const wasmBinary = await response.arrayBuffer();

            this.streamNode = new AudioWorkletNode(this.audioCtx, 'wasm-stream-processor');
            
            // Push thread pipeline configuration events
            this.streamNode.port.postMessage({ type: 'INIT_WASM', binary: wasmBinary });
            this.streamNode.port.postMessage({ type: 'SET_WAVE_TYPE', value: this.activeWaveId });
            this.streamNode.port.postMessage({ type: 'SET_FREQUENCY', value: this.currentFrequency });
            this.streamNode.port.postMessage({ type: 'SET_VOLUME', value: this.currentVolume });

            this.streamNode.connect(this.audioCtx.destination);
            console.log("Continuous WebAssembly audio pipeline processing stream started.");
        } catch (err) {
            console.error("Failed to start the AudioWorklet live synthesis stream pipeline:", err);
        }
    }

    /**
     * Disconnects the active real-time continuous background thread stream node
     */
    stopStream() {
        if (this.streamNode) {
            this.streamNode.disconnect();
            this.streamNode = null;
            console.log("Continuous live audio streaming disconnected.");
        }
    }

    /**
     * Hard release hook to close Context connections securely and clear pipeline leaks
     */
    destroy() {
        this.stopStream();
        if (this.audioCtx) {
            this.audioCtx.close();
            this.audioCtx = null;
        }
        this.isInitialized = false;
    }
}

