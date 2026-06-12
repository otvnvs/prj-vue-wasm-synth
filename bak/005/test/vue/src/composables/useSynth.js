// src/vue/src/composables/useSynth.js
import { ref, onBeforeUnmount } from 'vue';
import { WebAudioSynth } from '../../../../dist/synth-engine.js';

// Resolve library files from your /dist directory using Vite asset rules
import wasmUrl from '../../../../dist/waveform.wasm?url';
import processorUrl from '../../../../dist/stream-processor.js?url';

export function useSynth() {
  const synth = ref(null);
  const isReady = ref(false);
  const isStreaming = ref(false);
  const frequency = ref(440);
  const volume = ref(0.5);
  const waveform = ref('sine');

  /**
   * Safe asynchronous engine constructor wrapper
   */
  const init = async () => {
    if (synth.value) return;

    try {
      synth.value = new WebAudioSynth({ wasmUrl, processorUrl });
      await synth.value.init();
      
      // Sync initial component state values down to the core
      synth.value.setWaveform(waveform.value);
      synth.value.setFrequency(frequency.value);
      synth.value.setVolume(volume.value);
      
      isReady.value = true;
      console.log('Synth Composable mounted and linked smoothly.');
    } catch (err) {
      console.error('Failed to initialize synth inside composable:', err);
    }
  };

  const setWaveform = (type) => {
    waveform.value = type;
    if (synth.value) synth.value.setWaveform(type);
  };

  const setFrequency = (hz) => {
    frequency.value = parseFloat(hz);
    if (synth.value) synth.value.setFrequency(hz);
  };

  const setVolume = (val) => {
    volume.value = parseFloat(val);
    if (synth.value) synth.value.setVolume(val);
  };

  const playBuffer = async (duration = 2.0) => {
    if (!synth.value) return;
    await synth.value.playBuffer(duration);
  };

  const toggleStream = async () => {
    if (!synth.value) return;

    if (isStreaming.value) {
      synth.value.stopStream();
      isStreaming.value = false;
    } else {
      await synth.value.startStream();
      isStreaming.value = true;
    }
  };

  // Automated cleaning hook to close contexts and prevent memory leaks
  onBeforeUnmount(() => {
    if (synth.value) {
      synth.value.destroy();
      synth.value = null;
    }
  });

  return {
    isReady,
    isStreaming,
    frequency,
    volume,
    waveform,
    init,
    setWaveform,
    setFrequency,
    setVolume,
    playBuffer,
    toggleStream
  };
}

