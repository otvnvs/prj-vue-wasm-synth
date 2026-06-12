#include <emscripten.h>
#include "oscillators.h"

// Forward declarations of our modular functions
void generate_sine_wave(float* buffer, int sample_count, float frequency, float sample_rate);
void generate_square_wave(float* buffer, int sample_count, float frequency, float sample_rate);
void generate_sawtooth_wave(float* buffer, int sample_count, float frequency, float sample_rate);

void stream_sine_wave(float* buffer, int sample_count, float frequency, float sample_rate);
void stream_square_wave(float* buffer, int sample_count, float frequency, float sample_rate);
void stream_sawtooth_wave(float* buffer, int sample_count, float frequency, float sample_rate);

// Exposed API Wrappers
EMSCRIPTEN_KEEPALIVE
void generate_wave(int type, float* buffer, int sample_count, float frequency, float sample_rate) {
    if (type == 0) generate_sine_wave(buffer, sample_count, frequency, sample_rate);
    else if (type == 1) generate_square_wave(buffer, sample_count, frequency, sample_rate);
    else if (type == 2) generate_sawtooth_wave(buffer, sample_count, frequency, sample_rate);
}

EMSCRIPTEN_KEEPALIVE
void stream_wave(int type, float* buffer, int sample_count, float frequency, float sample_rate) {
    if (type == 0) stream_sine_wave(buffer, sample_count, frequency, sample_rate);
    else if (type == 1) stream_square_wave(buffer, sample_count, frequency, sample_rate);
    else if (type == 2) stream_sawtooth_wave(buffer, sample_count, frequency, sample_rate);
}

EMSCRIPTEN_KEEPALIVE
void reset_stream_phase() {
    global_phase = 0.0f;
}

