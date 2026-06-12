// res/main.js
let audioCtx = null;
let isWasmReady = false;
let streamNode = null;
let activeWaveId = 0; // 0=Sine, 1=Square, 2=Sawtooth

// Mapping friendly text names to integer IDs
const waveMap = { 'sine': 0, 'square': 1, 'sawtooth': 2 };

window.Module = {
    onRuntimeInitialized: function() {
        isWasmReady = true;
        document.getElementById('status').innerText = "Status: Modules Ready!";
    }
};

export function setWaveformType(typeName) {
    activeWaveId = waveMap[typeName];
    console.log(`Switched active generator target to ID: ${activeWaveId} (${typeName})`);
    
    // If a continuous live stream is currently active, push the update down to it
    if (streamNode) {
        streamNode.port.postMessage({ type: 'SET_WAVE_TYPE', value: activeWaveId });
    }
}

export function handlePlayback() {
    if (!isWasmReady || typeof Module === 'undefined' || !Module._malloc) return;

    if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    if (audioCtx.state === 'suspended') audioCtx.resume();

    const sampleRate = audioCtx.sampleRate || 44100; 
    const sampleCount = sampleRate * 2.0;

    const bufferPtr = Module._malloc(sampleCount * 4);
    
    // Call the centralized wave generation routing function
    Module._generate_wave(activeWaveId, bufferPtr, sampleCount, 440.0, sampleRate);

    const audioData = Module.HEAPF32.subarray(bufferPtr / 4, (bufferPtr / 4) + sampleCount);

    const audioBuffer = audioCtx.createBuffer(1, sampleCount, sampleRate);
    audioBuffer.getChannelData(0).set(audioData);

    const source = audioCtx.createBufferSource();
    source.buffer = audioBuffer;
    source.connect(audioCtx.destination);
    source.start();

    Module._free(bufferPtr);
}

export async function toggleLiveStream() {
    if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    if (audioCtx.state === 'suspended') audioCtx.resume();

    if (streamNode) {
        streamNode.disconnect();
        streamNode = null;
        document.getElementById('streamStatus').innerText = "Live Stream: Stopped";
        return;
    }

    try {
        await audioCtx.audioWorklet.addModule('stream-processor.js');
        const response = await fetch('waveform.wasm');
        const wasmBinary = await response.arrayBuffer();

        streamNode = new AudioWorkletNode(audioCtx, 'wasm-stream-processor');
        streamNode.port.postMessage({ type: 'INIT_WASM', binary: wasmBinary });
        
        // Pass the currently active waveform type down to the initialized stream
        streamNode.port.postMessage({ type: 'SET_WAVE_TYPE', value: activeWaveId });

        streamNode.port.onmessage = (e) => {
            if (e.data.type === 'STATUS' && e.data.status === 'READY') {
                document.getElementById('streamStatus').innerText = "Live Stream: Streaming...";
            }
        };

        streamNode.connect(audioCtx.destination);
    } catch (err) {
        console.error(err);
    }
}

