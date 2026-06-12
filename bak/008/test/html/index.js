// test/html/test-runner.js
import { WebAudioSynth } from '../../dist/synth-engine.js';

let synth = null;
let isStreaming = false;

// Keep track of a dummy key ID for the single static buffer button
const BUFFER_TEST_KEY_ID = 999;

/**
 * 1. Engine Life-Cycle Initialization
 */
function initTestRunner() {
    try {
        // Instantiate the object-oriented wrapper class with target paths
        synth = new WebAudioSynth({
            wasmUrl: '../../dist/waveform.wasm',
            processorUrl: '../../dist/stream-processor.js'
        });

        // Initialize user audio permissions layer
        synth.init();

        // Update UI status badges
        document.getElementById('status').innerText = "Status: Synth Instance Active & Ready!";
        
        // Unlock panel interactive fields
        document.getElementById('btnPlayBuffer').disabled = false;
        document.getElementById('btnToggleStream').disabled = false;
        document.getElementById('waveSelect').disabled = false;
        document.getElementById('freqSlider').disabled = false;
        document.getElementById('volSlider').disabled = false;

        console.log("WebAudioSynth multi-voice testing runner successfully mounted.");
    } catch (err) {
        console.error("Initialization failure:", err);
        document.getElementById('status').innerText = "Status: Instantiation Failed!";
    }
}

/**
 * 2. Continuous Parameters Synchronization
 * In the new polyphonic pipeline, moving the sliders while streaming live 
 * updates the running note on the fly by overriding its frequency/volume.
 */
function handleFrequencyChange(e) {
    const val = e.target.value;
    document.getElementById('freqVal').innerText = val;
    
    if (synth) {
        synth.setFrequency(val);
        // If live streaming is active, dynamically update our test key note
        if (isStreaming && synth.streamNode) {
            const typeName = document.getElementById('waveSelect').value;
            const waveMap = { 'sine': 0, 'square': 1, 'sawtooth': 2 };
            
            synth.streamNode.port.postMessage({
                type: 'NOTE_ON',
                keyId: BUFFER_TEST_KEY_ID,
                waveType: waveMap[typeName],
                frequency: parseFloat(val),
                volume: parseFloat(document.getElementById('volSlider').value)
            });
        }
    }
}

function handleVolumeChange(e) {
    const val = e.target.value;
    document.getElementById('volVal').innerText = Math.round(val * 100);
    
    if (synth) {
        synth.setVolume(val);
        // If live streaming is active, dynamically update our test key note volume
        if (isStreaming && synth.streamNode) {
            const typeName = document.getElementById('waveSelect').value;
            const waveMap = { 'sine': 0, 'square': 1, 'sawtooth': 2 };
            
            synth.streamNode.port.postMessage({
                type: 'NOTE_ON',
                keyId: BUFFER_TEST_KEY_ID,
                waveType: waveMap[typeName],
                frequency: parseFloat(document.getElementById('freqSlider').value),
                volume: parseFloat(val)
            });
        }
    }
}

function handleWaveformChange(e) {
    const val = e.target.value;
    if (synth) {
        synth.setWaveform(val);
        // If live streaming is active, dynamically swap the running note waveform
        if (isStreaming && synth.streamNode) {
            const waveMap = { 'sine': 0, 'square': 1, 'sawtooth': 2 };
            synth.streamNode.port.postMessage({
                type: 'NOTE_ON',
                keyId: BUFFER_TEST_KEY_ID,
                waveType: waveMap[val],
                frequency: parseFloat(document.getElementById('freqSlider').value),
                volume: parseFloat(document.getElementById('volSlider').value)
            });
        }
    }
}

/**
 * 3. Execution Pipeline Routines
 */
async function triggerStaticBuffer() {
    if (synth) {
        console.log("Evaluating static 2-second clip using polyphonic layout...");
        // Call the asynchronous self-instantiating method inside the library
        await synth.playBuffer(2.0);
    }
}

function toggleLiveStream() {
    if (!synth) return;

    const streamStatusEl = document.getElementById('streamStatus');
    const typeName = document.getElementById('waveSelect').value;
    const waveMap = { 'sine': 0, 'square': 1, 'sawtooth': 2 };

    if (isStreaming) {
        // Send a note-off signal to silence the running note before closing the stream
        if (synth.streamNode) {
            synth.streamNode.port.postMessage({ type: 'NOTE_OFF', keyId: BUFFER_TEST_KEY_ID });
        }
        synth.stopStream();
        isStreaming = false;
        streamStatusEl.innerText = "Live Stream: Offline";
    } else {
        // Open the continuous audio thread pipeline
        synth.startStream().then(() => {
            isStreaming = true;
            streamStatusEl.innerText = "Live Stream: Streaming continuously!";
            
            // Trigger a single continuous running note using our dummy key ID
            synth.streamNode.port.postMessage({
                type: 'NOTE_ON',
                keyId: BUFFER_TEST_KEY_ID,
                waveType: waveMap[typeName],
                frequency: parseFloat(document.getElementById('freqSlider').value),
                volume: parseFloat(document.getElementById('volSlider').value)
            });
        });
    }
}

/**
 * 4. DOM Context Event Mounting
 */
window.addEventListener('DOMContentLoaded', () => {
    // Mount lifecycle loader button
    document.getElementById('btnLoadEngine').addEventListener('click', initTestRunner);

    // Mount runtime parametric controls
    document.getElementById('waveSelect').addEventListener('change', handleWaveformChange);
    document.getElementById('freqSlider').addEventListener('input', handleFrequencyChange);
    document.getElementById('volSlider').addEventListener('input', handleVolumeChange);

    // Mount operational execution buttons
    document.getElementById('btnPlayBuffer').addEventListener('click', triggerStaticBuffer);
    document.getElementById('btnToggleStream').addEventListener('click', toggleLiveStream);
});

