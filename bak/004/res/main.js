// res/main.js
let audioCtx = null;
let isWasmReady = false;
let streamNode = null;
let activeWaveId = 0; 
let currentFrequency = 440.0;
let currentVolume = 0.5;

const waveMap = { 'sine': 0, 'square': 1, 'sawtooth': 2 };

window.Module = {
    onRuntimeInitialized: function() {
        isWasmReady = true;
        document.getElementById('status').innerText = "Status: Modules Ready!";
    }
};

export function setWaveformType(typeName) {
    activeWaveId = waveMap[typeName];
    if (streamNode) {
        streamNode.port.postMessage({ type: 'SET_WAVE_TYPE', value: activeWaveId });
    }
}

// NEW: Export setter to route values to either the upcoming buffer or the live stream thread
export function setFrequencyValue(hzValue) {
    currentFrequency = parseFloat(hzValue);
    console.log(`Frequency updated to: ${currentFrequency} Hz`);
    
    if (streamNode) {
        // Post the value straight to the AudioWorklet thread without interrupting playback
        streamNode.port.postMessage({ type: 'SET_FREQUENCY', value: currentFrequency });
    }
}

export function setVolumeValue(percentValue) {
    currentVolume = parseFloat(percentValue);
    console.log(`Volume updated to: ${currentVolume} Hz`);
    
    if (streamNode) {
        // Post the value straight to the AudioWorklet thread without interrupting playback
        streamNode.port.postMessage({ type: 'SET_VOLUME', value: currentVolume });
    }
}


export function handlePlayback() {
    if (!isWasmReady || typeof Module === 'undefined' || !Module._malloc) return;

    if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    if (audioCtx.state === 'suspended') audioCtx.resume();

    const sampleRate = audioCtx.sampleRate || 44100; 
    const sampleCount = sampleRate * 2.0;

    const bufferPtr = Module._malloc(sampleCount * 4);
    
    // MODIFIED: Uses currentFrequency instead of hardcoded 440.0
    Module._generate_wave(activeWaveId, bufferPtr, sampleCount, currentFrequency, sampleRate, currentVolume);

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
        streamNode.port.postMessage({ type: 'SET_WAVE_TYPE', value: activeWaveId });
        streamNode.port.postMessage({ type: 'SET_FREQUENCY', value: currentFrequency });
        streamNode.port.postMessage({ type: 'SET_VOLUME', value: currentVolume});

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

