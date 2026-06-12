#include "oscillators.h"
#include <math.h>

void generate_sine_wave(float* buffer, int sample_count, float frequency, float sample_rate, float volume) {
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = sinf(2.0f * PI * frequency * ((float)i / sample_rate)) * volume;
    }
}

void stream_sine_wave(float* buffer, int sample_count, float frequency, float sample_rate, float volume) {
    float phase_increment = (2.0f * PI * frequency) / sample_rate;
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = sinf(global_phase) * volume;
        update_phase(phase_increment);
    }
}
