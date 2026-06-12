#!/bin/bash
emcc generator.c -o waveform.js \
  -s EXPORTED_FUNCTIONS='["_generate_sine_wave", "_generate_square_wave", "_stream_sine_wave", "_reset_stream_phase", "_malloc", "_free"]' \
  -s EXPORTED_RUNTIME_METHODS='["HEAPF32"]' \
  -s STANDALONE_WASM=1 \
  --no-entry \
  -O3
