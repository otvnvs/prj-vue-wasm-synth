// test/vanilla/test-runner.js
import { WebAudioSynth } from '../../dist/synth-engine.js';

let synth = null;
let isStreaming = false;

// 1. Instantiation Block
function initTestRunner() {
    try {
        // Point the configuration explicitly to your localized build files
        synth = new WebAudioSynth({
            wasmUrl: '../../dist/waveform.wasm',
            processorUrl: '../../dist/stream-processor.js'
        });

        // Initialize user audio permissions layer
        synth.init();

        // Update UI state labels
        document.getElementById('status').innerText = "Status: Synth Instance Created & Ready!";
        
        // Un-lock controls
        document.getElementById('btnPlayBuffer').disabled = false;
        document.getElementById('btnToggleStream').disabled = false;
        document.getElementById('waveSelect').disabled = false;
        document.getElementById('freqSlider').disabled = false;
        document.getElementById('volSlider').disabled = false;

        console.log("WebAudioSynth testing wrapper successfully mounted.");
    } catch (err) {
        console.error("Initialization failure:", err);
        document.getElementById('status').innerText = "Status: Instantiation Failed!";
    }
}

// 2. Continuous Parameters Synchronization
function handleFrequencyChange(e) {
    const val = e.target.value;
    document.getElementById('freqVal').innerText = val;
    if (synth) synth.setFrequency(val);
}

function handleVolumeChange(e) {
    const val = e.target.value;
    document.getElementById('volVal').innerText = Math.round(val * 100);
    if (synth) synth.setVolume(val);
}

function handleWaveformChange(e) {
    const val = e.target.value;
    if (synth) synth.setWaveform(val);
}

// 3. Execution Pipeline Routines
// Inside test/html/test-runner.js
async function triggerStaticBuffer() {
    if (synth) {
        console.log("Evaluating static 2-second clip...");
        // Add await here to handle the asynchronous instantiation cleanly
        await synth.playBuffer(2.0); 
    }
}

function toggleLiveStream() {
    if (!synth) return;

    const streamStatusEl = document.getElementById('streamStatus');

    if (isStreaming) {
        synth.stopStream();
        isStreaming = false;
        streamStatusEl.innerText = "Live Stream: Offline";
    } else {
        synth.startStream();
        isStreaming = true;
        streamStatusEl.innerText = "Live Stream: Streaming continuously!";
    }
}

// 4. Bind listeners safely when the DOM loads completely
window.addEventListener('DOMContentLoaded', () => {
    // Mount click engine activator
    document.getElementById('btnLoadEngine').addEventListener('click', initTestRunner);

    // Mount parameter slider controls
    document.getElementById('waveSelect').addEventListener('change', handleWaveformChange);
    document.getElementById('freqSlider').addEventListener('input', handleFrequencyChange);
    document.getElementById('volSlider').addEventListener('input', handleVolumeChange);

    // Mount action operational nodes
    document.getElementById('btnPlayBuffer').addEventListener('click', triggerStaticBuffer);
    document.getElementById('btnToggleStream').addEventListener('click', toggleLiveStream);
});

