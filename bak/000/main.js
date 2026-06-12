// 1. App State Variables
let audioCtx = null;
let isWasmReady = false;

// 2. Immediate Event Binding
document.getElementById('playSine').addEventListener('click', () => {
    console.log("Sine button clicked!");
    handlePlayback('sine');
});

document.getElementById('playSquare').addEventListener('click', () => {
    console.log("Square button clicked!");
    handlePlayback('square');
});

// 3. Monitor Emscripten Runtime Hook
// Attached explicitly to window because modules do not expose variables globally by default.
window.Module = {
    onRuntimeInitialized: function() {
        console.log("WebAssembly logic successfully loaded and initialized!");
        isWasmReady = true;
        document.getElementById('status').innerText = "Status: Ready to play!";
    }
};

// 4. Central Playback Controller
function handlePlayback(type) {
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

