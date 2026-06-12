#!/bin/bash
emcc generator.c -o waveform.js \
  -s EXPORTED_FUNCTIONS='["_generate_sine_wave", "_generate_square_wave", "_malloc", "_free"]' \
  -s EXPORTED_RUNTIME_METHODS='["HEAPF32"]' \
  -O3

