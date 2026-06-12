// test/vue/src/composables/useSynth.js
import { ref, onBeforeUnmount } from 'vue';
import { WebAudioSynth } from '../../../../dist/synth-engine.js';

import wasmUrl from '../../../../dist/waveform.wasm?url';
import processorUrl from '../../../../dist/stream-processor.js?url';

export function useSynth() {
  const synth = ref(null);
  const isReady = ref(false);
  const isStreaming = ref(false);
  const globalVolume = ref(0.5);
  const activeWaveform = ref('sine');

  // adsr
  const attack = ref(0.05);
  const decay = ref(0.15);
  const sustain = ref(0.60);
  const release = ref(0.40);

  // tremolo
  const tremoloActive = ref(false);
  const tremoloSpeed = ref(4.0);
  const tremoloDepth = ref(0.5);

  // echo
  const echoActive = ref(false);
  const echoTime = ref(0.3);
  const echoFeedback = ref(0.4);
  const echoMix = ref(0.35);

  //ftable
  const drawbars = ref([1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]); // 8 Harmonics

  //distortion
  const distActive = ref(false); const distDrive = ref(5.0); const distBlend = ref(0.4);

  //chorus
  const chorusActive = ref(false); const chorusRate = ref(1.0); const chorusDepth = ref(0.5);


  
  const waveMap = { 'sine': 0, 'square': 1, 'sawtooth': 2 };

  const init = async () => {
    if (synth.value) return;
    try {
      synth.value = new WebAudioSynth({ wasmUrl, processorUrl });
      await synth.value.init();
      isReady.value = true;
      // Send down initial defaults
      updateAdsr();
    } catch (err) {
      console.error('Failed initialization:', err);
    }
  };

  const startStream = async () => {
    if (!synth.value) return;
    await synth.value.startStream();
    isStreaming.value = true;
    updateAdsr(); // Sync sliders right after startup
  };

  // NEW: Dispatches real-time slider metrics down to WASM
  const updateAdsr = () => {
    if (synth.value && synth.value.streamNode) {
      synth.value.streamNode.port.postMessage({
        type: 'SET_ADSR',
        attack: attack.value,
        decay: decay.value,
        sustain: sustain.value,
        release: release.value
      });
    }
    // Also store it on our main instance for playBuffer evaluations
    if (synth.value && synth.value.mainWasmInstance) {
        synth.value.mainWasmInstance.set_adsr_parameters(attack.value, decay.value, sustain.value, release.value);
    }
  };

   const updateTremolo = () => {
     if (synth.value && synth.value.streamNode) {
       synth.value.streamNode.port.postMessage({
         type: 'SET_TREMOLO',
         active: tremoloActive.value ? 1 : 0,
         speed: tremoloSpeed.value,
         depth: tremoloDepth.value
       });
     }
   };

   const updateEcho = () => {
     if (synth.value && synth.value.streamNode) {
       synth.value.streamNode.port.postMessage({
         type: 'SET_ECHO',
         active: echoActive.value ? 1 : 0,
         time: parseFloat(echoTime.value),
         feedback: parseFloat(echoFeedback.value),
         mix: parseFloat(echoMix.value)
       });
     }
   };

const updateCustomHarmonics = () => {
  if (synth.value && synth.value.streamNode) {
    // Pass raw primitive numeric values down to the worker thread
    synth.value.streamNode.port.postMessage({
      type: 'SET_CUSTOM_HARMONICS',
      weights: drawbars.value.map(val => parseFloat(val))
    });
  }
};

const updateDistortion = () => {
  if (synth.value?.streamNode) {
    synth.value.streamNode.port.postMessage({ type: 'SET_DISTORTION', active: distActive.value ? 1 : 0, drive: distDrive.value, blend: distBlend.value });
  }
};

const updateChorus = () => {
  if (synth.value?.streamNode) {
    synth.value.streamNode.port.postMessage({ type: 'SET_CHORUS', active: chorusActive.value ? 1 : 0, rate: chorusRate.value, depth: chorusDepth.value });
  }
};


  const noteOn = (keyId, frequency) => {
    if (synth.value && synth.value.streamNode) {
      const typeId = waveMap[activeWaveform.value];
      synth.value.streamNode.port.postMessage({
        type: 'NOTE_ON',
        keyId,
        waveType: typeId,
        frequency: parseFloat(frequency),
        volume: parseFloat(globalVolume.value)
      });
    }
  };

  const noteOff = (keyId) => {
    if (synth.value && synth.value.streamNode) {
      synth.value.streamNode.port.postMessage({ type: 'NOTE_OFF', keyId });
    }
  };

  onBeforeUnmount(() => {
    if (synth.value) synth.value.destroy();
  });

  return {
    isReady,
    isStreaming,
    globalVolume,
    activeWaveform,
    attack,
    decay,
    sustain,
    release,
    init,
    startStream,
    noteOn,
    noteOff,
    updateAdsr,
    tremoloActive,
    tremoloSpeed,
    tremoloDepth,
    updateTremolo,
    echoActive,
    echoTime,
    echoFeedback,
    echoMix,
    updateEcho,
    drawbars,
    updateCustomHarmonics ,
    distActive,
	distDrive,
	distBlend,
	updateDistortion,
	chorusActive,
	chorusRate,
	chorusDepth,
	updateChorus

  };
}

