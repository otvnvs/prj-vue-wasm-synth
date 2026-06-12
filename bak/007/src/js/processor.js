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
                    //if (this.wasm._initialize) this.wasm._initialize();
                    if (this.wasm.init_wasm_synth_core) this.wasm.init_wasm_synth_core();
                    
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
	   console.log('NOT_ON:',data);
            this.wasm.note_on(data.keyId, data.waveType, data.frequency, data.volume);
        }
        if (data.type === 'NOTE_OFF') {
	   console.log('NOT_OFF:',data);
            this.wasm.note_off(data.keyId);
        }
	if (data.type === 'SET_ADSR') {
	   console.log('SET_ADSR:',data);
	    if (this.isReady) {
		this.wasm.set_adsr_parameters(
		    parseFloat(data.attack),
		    parseFloat(data.decay),
		    parseFloat(data.sustain),
		    parseFloat(data.release)
		);
	    }
	}
	if (data.type === 'SET_TREMOLO') {
	   console.log('SET_TREMOLO:',data);
	    if (this.isReady) {
		this.wasm.set_tremolo_parameters(
		    parseInt(data.active),
		    parseFloat(data.speed),
		    parseFloat(data.depth)
		);
	    }
	}
	if (data.type === 'SET_ECHO') {
	   console.log('SET_ECHO:',data);
	    if (this.isReady) {
		this.wasm.set_echo_parameters(
		    parseInt(data.active),
		    parseFloat(data.time),
		    parseFloat(data.feedback),
		    parseFloat(data.mix)
		);
	    }
	}
	// Inside src/js/processor.js
	if (data.type === 'SET_CUSTOM_HARMONICS') {
	   console.log('SET_CUSTOM_HARMONICS:',data);
	    if (this.isReady && this.wasm.set_custom_harmonics) { // <-- Dropped _api
		const jsArray = data.weights;
		const count = jsArray.length;

		const tempPtr = this.wasm.malloc(count * 4);
		const tempHeap = new Float32Array(this.wasm.memory.buffer, tempPtr, count);
		tempHeap.set(jsArray);

		this.wasm.set_custom_harmonics(tempPtr, count); // <-- Dropped _api

		this.wasm.free(tempPtr);
	    }
	}

	if (data.type === 'SET_DISTORTION') {
	   console.log('SET_DISTORTION:',data);
	    if (this.isReady) this.wasm.set_distortion_parameters_api(Number(data.active), Number(data.drive), Number(data.blend));
	}
	if (data.type === 'SET_CHORUS') {
	   console.log('SET_CHORUS:',data);
	    if (this.isReady) this.wasm.set_chorus_parameters_api(Number(data.active), Number(data.rate), Number(data.depth));
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

