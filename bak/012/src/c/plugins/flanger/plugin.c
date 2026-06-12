// src/c/plugin_chorus_mono.c
#include "plugin.h"
#include <stdlib.h>
#include <math.h>
#include <emscripten.h>

#define MAX_DELAY_SAMPLES 4800  // 0.1 seconds at 48kHz is plenty for chorus/flange
#define PI 3.14159265358979323846f

typedef struct {
    float* ring_buffer;
    int write_ptr;
    
    // Parameters
    float rate;      // LFO frequency in Hz (0.1 to 10.0)
    float depth;     // LFO modulation depth (0.0 to 1.0)
    float feedback;  // Feedback amount (-0.95 to 0.95)
    float mix;       // Dry/Wet blend (0.0 to 1.0)
    
    // Internal LFO Phase
    float lfo_phase;
} ChorusMonoState;

EMSCRIPTEN_KEEPALIVE
EXPORT void* create_instance() {
    ChorusMonoState* state = (ChorusMonoState*)malloc(sizeof(ChorusMonoState));
    if (!state) return NULL;

    state->ring_buffer = (float*)calloc(MAX_DELAY_SAMPLES, sizeof(float));
    state->write_ptr = 0;
    
    // Default safe Chorus settings
    state->rate = 1.5f;       
    state->depth = 0.5f;      
    state->feedback = 0.0f;   // Set to 0.0 for classic chorus, increase for flanger
    state->mix = 0.5f;        
    state->lfo_phase = 0.0f;

    return (void*)state;
}

EMSCRIPTEN_KEEPALIVE
EXPORT void set_parameter(void* instance, int param_id, float value) {
    ChorusMonoState* state = (ChorusMonoState*)instance;
    if (!state) return;

    switch (param_id) {
        case 0: state->rate = value; break;
        case 1: state->depth = value; break;
        case 2: state->feedback = value; break;
        case 3: state->mix = value; break;
    }
}

EMSCRIPTEN_KEEPALIVE
EXPORT void process(void* instance, float* input, float* output, int sample_count, float sample_rate) {
    ChorusMonoState* state = (ChorusMonoState*)instance;
    if (!state || !state->ring_buffer) return;

    // Baseline delay defaults: Chorus (~15ms), Flanger requires shorter (~2ms)
    // If feedback is high, we assume flanger mode and drop the delay base time
    float base_delay_ms = (fabsf(state->feedback) > 0.1f) ? 2.0f : 15.0f;
    float base_delay_samples = (base_delay_ms / 1000.0f) * sample_rate;
    
    // Maximum modulation amplitude in samples
    float max_mod_samples = (base_delay_ms * 0.7f / 1000.0f) * sample_rate; 

    // Phase increment per sample for the LFO loop
    float phase_inc = (2.0f * PI * state->rate) / sample_rate;

    for (int i = 0; i < sample_count; i++) {
        float dry_sample = input[i];

        // Read single LFO value
        float lfo = sinf(state->lfo_phase);

        // Calculate dynamic, moving delay time
        float delay_samples = base_delay_samples + (lfo * max_mod_samples * state->depth);

        // Calculate read pointer position in ring buffer
        float read_ptr = (float)state->write_ptr - delay_samples;
        while (read_ptr < 0.0f) read_ptr += (float)MAX_DELAY_SAMPLES;

        // Linear interpolation for fractional samples
        int idx_floor = (int)read_ptr;
        int idx_ceil = (idx_floor + 1) % MAX_DELAY_SAMPLES;
        float frac = read_ptr - (float)idx_floor;

        float wet_sample = state->ring_buffer[idx_floor] + frac * (state->ring_buffer[idx_ceil] - state->ring_buffer[idx_floor]);

        // Write processed mix back straight into output pointer array
        output[i] = (dry_sample * (1.0f - state->mix)) + (wet_sample * state->mix);

        // Store back in ring history memory with optional feedback path
        state->ring_buffer[state->write_ptr] = dry_sample + (wet_sample * state->feedback);
        
        // Advance buffer indices and LFO cycles
        state->write_ptr = (state->write_ptr + 1) % MAX_DELAY_SAMPLES;
        state->lfo_phase += phase_inc;
        if (state->lfo_phase >= 2.0f * PI) {
            state->lfo_phase -= 2.0f * PI;
        }
    }
}

EMSCRIPTEN_KEEPALIVE
EXPORT void destroy_instance(void* instance) {
    ChorusMonoState* state = (ChorusMonoState*)instance;
    if (!state) return;

    if (state->ring_buffer) free(state->ring_buffer);
    free(state);
}

EMSCRIPTEN_KEEPALIVE
EXPORT const char* get_plugin_manifest() {
    return "{"
        "\"name\": \"Mono Chorus/Flanger\","
        "\"id_tag\": \"mono_chorus\","
        "\"type\": \"processor\","
        "\"inputs\": 1,"
        "\"outputs\": 1,"
        "\"parameters\": ["
            "{\"id\": 0, \"name\": \"Rate\", \"min\": 0.1, \"max\": 10.0, \"default\": 1.5},"
            "{\"id\": 1, \"name\": \"Depth\", \"min\": 0.0, \"max\": 1.0, \"default\": 0.5},"
            "{\"id\": 2, \"name\": \"Feedback\", \"min\": -0.95, \"max\": 0.95, \"default\": 0.0},"
            "{\"id\": 3, \"name\": \"MixBlend\", \"min\": 0.0, \"max\": 1.0, \"default\": 0.5}"
        "]"
    "}";
}
