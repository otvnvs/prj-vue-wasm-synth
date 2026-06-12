#include "oscillators.h"
#include <math.h>

void generate_square_wave(float* buffer, int sample_count, float frequency, float sample_rate, float volume) {
    for (int i = 0; i < sample_count; i++) {
        float value = sinf(2.0f * PI * frequency * ((float)i / sample_rate));
        buffer[i] = (value >= 0.0f) ? 1.0f : -1.0f;
	buffer[i] *= volume;
    }
}

void stream_square_wave(float* buffer, int sample_count, float frequency, float sample_rate, float volume) {
    float phase_increment = (2.0f * PI * frequency) / sample_rate;
    for (int i = 0; i < sample_count; i++) {
        float value = sinf(global_phase);
        buffer[i] = (value >= 0.0f) ? 1.0f : -1.0f;
	buffer[i] *= volume;
        update_phase(phase_increment);
    }
}

