// main.js

// 1. App State Variables (PRESERVED)
let audioCtx = null;
let isWasmReady = false;

// 2. Monitor Emscripten Runtime Hook (PRESERVED)
// Attached explicitly to window because modules do not expose variables globally by default.
window.Module = {
    onRuntimeInitialized: function() {
        console.log("WebAssembly logic successfully loaded and initialized!");
        isWasmReady = true;
        const statusEl = document.getElementById('status');
        if (statusEl) statusEl.innerText = "Status: Ready to play!";
    }
};

// 3. Central Playback Controller (PRESERVED EXACTLY AS SPECIFIED)
export function handlePlayback(type) {
    // Check if WASM runtime is accessible yet
    if (!isWasmReady || typeof Module === 'undefined' || !Module._malloc) {
        alert("WebAssembly is still compiling or failed to load. Check console.");
        return;
    }

    // Safe Web Audio Context initializer (Autoplay compliance)
    if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    if (audioCtx.state === 'suspended') {
        audioCtx.resume();
    }

    const sampleRate = audioCtx.sampleRate || 44100; 
    const duration = 2.0; // Seconds to generate
    const sampleCount = sampleRate * duration;

    // Allocate float buffer memory inside the WASM linear heap (4 bytes per float)
    const bufferSize = sampleCount * 4;
    const bufferPtr = Module._malloc(bufferSize);

    // Execute the compiled C logic
    if (type === 'sine') {
        Module._generate_sine_wave(bufferPtr, sampleCount, 440.0, sampleRate);
    } else if (type === 'square') {
        Module._generate_square_wave(bufferPtr, sampleCount, 440.0, sampleRate);
    }

    // Secure memory mapping layer using the recommended safe subarray view
    const floatOffset = bufferPtr / 4;
    const audioData = Module.HEAPF32.subarray(floatOffset, floatOffset + sampleCount);

    // Print snippet to console to confirm math arrays are populated (Not all zeros)
    console.log(`${type.toUpperCase()} sample output (first 5 frames):`, audioData.subarray(0, 5));

    // Populate the browser hardware audio buffer
    const audioBuffer = audioCtx.createBuffer(1, sampleCount, sampleRate);
    audioBuffer.getChannelData(0).set(audioData);

    // Connect nodes and begin playing sound
    const source = audioCtx.createBufferSource();
    source.buffer = audioBuffer;
    source.connect(audioCtx.destination);
    source.start();

    // Always clear heap pointer references immediately to avoid RAM leakage
    Module._free(bufferPtr);
}

// 4. Continuous Live Streaming Controller (NEW ADDITION)
let streamNode = null;

export async function toggleLiveStream() {
    if (!audioCtx) {
        audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }
    if (audioCtx.state === 'suspended') {
        audioCtx.resume();
    }

    // If already streaming, stop it
    if (streamNode) {
        streamNode.disconnect();
        streamNode = null;
        document.getElementById('streamStatus').innerText = "Live Stream: Stopped";
        return;
    }

    document.getElementById('streamStatus').innerText = "Live Stream: Initializing thread...";

    try {
        // Register the background audio processor file
        await audioCtx.audioWorklet.addModule('stream-processor.js');

        // Fetch the raw compiled WebAssembly binary file directly
        const response = await fetch('waveform.wasm');
        const wasmBinary = await response.arrayBuffer();

        // Create the pipeline node
        streamNode = new AudioWorkletNode(audioCtx, 'wasm-stream-processor');

        // Pass the binary array down to the isolated worker thread
        streamNode.port.postMessage({ type: 'INIT_WASM', binary: wasmBinary });

        // Listen for ready status back from the thread
        streamNode.port.onmessage = (event) => {
            if (event.data.type === 'STATUS' && event.data.status === 'READY') {
                document.getElementById('streamStatus').innerText = "Live Stream: Playing continuously!";
            }
        };

        // Route live audio out to speakers
        streamNode.connect(audioCtx.destination);

    } catch (err) {
        console.error("Could not initialize the live audio stream pipeline:", err);
        document.getElementById('streamStatus').innerText = "Live Stream: Failed to start.";
    }
}

