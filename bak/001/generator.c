#include <emscripten.h>
#include <math.h>

#define PI 3.14159265358979323846

// --- KEPT EXISTING FUNCTIONS ---
EMSCRIPTEN_KEEPALIVE
void generate_sine_wave(float* buffer, int sample_count, float frequency, float sample_rate) {
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = sinf(2.0f * PI * frequency * ((float)i / sample_rate));
    }
}

EMSCRIPTEN_KEEPALIVE
void generate_square_wave(float* buffer, int sample_count, float frequency, float sample_rate) {
    for (int i = 0; i < sample_count; i++) {
        float value = sinf(2.0f * PI * frequency * ((float)i / sample_rate));
        buffer[i] = (value >= 0.0f) ? 1.0f : -1.0f;
    }
}

// --- NEW LIVE STREAMING FUNCTIONS ---
// Tracks the cumulative phase angle so the wave doesn't click or pop between chunks
static float global_phase = 0.0f;

EMSCRIPTEN_KEEPALIVE
void stream_sine_wave(float* buffer, int sample_count, float frequency, float sample_rate) {
    // Calculate phase increment per sample frame
    float phase_increment = (2.0f * PI * frequency) / sample_rate;
    
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = sinf(global_phase);
        global_phase += phase_increment;
        
        // Keep phase bounded to prevent precision loss over time
        if (global_phase >= 2.0f * PI) {
            global_phase -= 2.0f * PI;
        }
    }
}

EMSCRIPTEN_KEEPALIVE
void reset_stream_phase() {
    global_phase = 0.0f;
}

