#include "oscillators.h"

void generate_sawtooth_wave(float* buffer, int sample_count, float frequency, float sample_rate) {
    float phase = 0.0f;
    float phase_increment = (2.0f * PI * frequency) / sample_rate;
    for (int i = 0; i < sample_count; i++) {
        // Map 0 -> 2*PI down onto -1.0 -> 1.0
        buffer[i] = (phase / PI) - 1.0f;
        phase += phase_increment;
        if (phase >= 2.0f * PI) phase -= 2.0f * PI;
    }
}

void stream_sawtooth_wave(float* buffer, int sample_count, float frequency, float sample_rate) {
    float phase_increment = (2.0f * PI * frequency) / sample_rate;
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = (global_phase / PI) - 1.0f;
        update_phase(phase_increment);
    }
}

