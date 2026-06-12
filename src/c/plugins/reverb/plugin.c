#include "plugin.h"
#include <stdlib.h>
#include <math.h>
#include <emscripten.h>

// 8 Parallel Comb Filters and 4 All-Pass Filters in series for dense, smooth tail diffusion
#define NUM_COMBS 8
#define NUM_ALLPASS 4

// Standard optimal prime-number buffer tunings for maximum dispersion at 44.1kHz/48kHz
static const int COMB_TUNINGS[NUM_COMBS] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
static const int ALLPASS_TUNINGS[NUM_ALLPASS] = { 556, 441, 341, 225 };
static const float ALLPASS_GAIN = 0.5f;

typedef struct {
    float* buffer;
    int size;
    int write_ptr;
} DelayLine;

typedef struct {
    DelayLine combs[NUM_COMBS];
    DelayLine allpass[NUM_ALLPASS];
    float room_size;     // Controls decay length
    float dampening;     // High-frequency absorption scale
    float mix;           // Dry/Wet ratio
    float width;         // Stereo/Structure scale factor
    float comb_filter_store[NUM_COMBS]; 
} ReverbPluginState;

static void init_delay_line(DelayLine* dl, int size) {
    dl->size = size;
    dl->buffer = (float*)calloc(size, sizeof(float));
    dl->write_ptr = 0;
}

EXPORT void* create_instance() {
    ReverbPluginState* state = (ReverbPluginState*)malloc(sizeof(ReverbPluginState));
    if (!state) return NULL;
    
    for (int i = 0; i < NUM_COMBS; i++) {
        init_delay_line(&state->combs[i], COMB_TUNINGS[i]);
        state->comb_filter_store[i] = 0.0f;
    }
    for (int i = 0; i < NUM_ALLPASS; i++) {
        init_delay_line(&state->allpass[i], ALLPASS_TUNINGS[i]);
    }
    
    state->room_size = 0.80f; 
    state->dampening = 0.25f; 
    state->mix = 0.30f;       
    state->width = 1.0f;
    
    return (void*)state;
}

EXPORT void set_parameter(void* instance, int param_id, float value) {
    ReverbPluginState* state = (ReverbPluginState*)instance;
    if (!state) return;
    switch (param_id) {
        case 0: state->room_size = value < 0.0f ? 0.0f : (value > 0.98f ? 0.98f : value); break; 
        case 1: state->dampening = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); break;
        case 2: state->mix = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); break;
        case 3: state->width = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); break;
    }
}

EXPORT void process(void* instance, float* input, float* output, int sample_count, float sample_rate) {
    ReverbPluginState* state = (ReverbPluginState*)instance;
    if (!state) return;

    // Cache parameters locally for faster loop processing execution
    float r_size = state->room_size;
    float damp = state->dampening;
    float inv_damp = 1.0f - damp;
    float mix_wet = state->mix;
    float mix_dry = 1.0f - mix_wet;

    for (int i = 0; i < sample_count; i++) {
        float dry_sample = input[i];
        float comb_accumulator = 0.0f;
        
        // 1. Process 8 Low-Pass Feedback Comb Filters in Parallel
        for (int c = 0; c < NUM_COMBS; c++) {
            DelayLine* dl = &state->combs[c];
            float out_sample = dl->buffer[dl->write_ptr];
            
            // Internal Low-Pass element smoothing
            state->comb_filter_store[c] = (out_sample * inv_damp) + (state->comb_filter_store[c] * damp);
            
            // Feed back the smoothed damp path scaled by room size parameter safely
            dl->buffer[dl->write_ptr] = dry_sample + (state->comb_filter_store[c] * r_size);
            
            dl->write_ptr = (dl->write_ptr + 1);
            if (dl->write_ptr >= dl->size) dl->write_ptr = 0;
            
            comb_accumulator += out_sample;
        }
        
        // Normalize the parallel energy sum to maintain excellent headroom scaling
        float current_sample = comb_accumulator * 0.125f * state->width;
        
        // 2. Process 4 Series All-Pass Networks for maximum echo diffusion blurring
        for (int a = 0; a < NUM_ALLPASS; a++) {
            DelayLine* dl = &state->allpass[a];
            float delayed = dl->buffer[dl->write_ptr];
            
            float ap_input = current_sample + (delayed * ALLPASS_GAIN);
            dl->buffer[dl->write_ptr] = ap_input;
            
            current_sample = -ap_input + delayed;
            
            dl->write_ptr = (dl->write_ptr + 1);
            if (dl->write_ptr >= dl->size) dl->write_ptr = 0;
        }
        
        // Direct output matrix mix blending assignment
        output[i] = (dry_sample * mix_dry) + (current_sample * mix_wet);
    }
}

EXPORT void destroy_instance(void* instance) {
    ReverbPluginState* state = (ReverbPluginState*)instance;
    if (!state) return;
    
    for (int i = 0; i < NUM_COMBS; i++) {
        if (state->combs[i].buffer) free(state->combs[i].buffer);
    }
    for (int i = 0; i < NUM_ALLPASS; i++) {
        if (state->allpass[i].buffer) free(state->allpass[i].buffer);
    }
    free(state);
}

EMSCRIPTEN_KEEPALIVE
EXPORT const char* get_plugin_manifest() {
    return "{"
    "\"name\": \"Studio Reverb\","
    "\"id_tag\": \"studio_reverb\","
    "\"type\": \"processor\","
    "\"inputs\": 1,"
    "\"outputs\": 1,"
    "\"parameters\": ["
    "{\"id\": 0, \"name\": \"DecayTime\", \"min\": 0.1, \"max\": 0.98, \"default\": 0.80},"
    "{\"id\": 1, \"name\": \"Dampening\", \"min\": 0.0, \"max\": 1.0, \"default\": 0.25},"
    "{\"id\": 2, \"name\": \"MixBlend\", \"min\": 0.0, \"max\": 1.0, \"default\": 0.30},"
    "{\"id\": 3, \"name\": \"SpaceWidth\", \"min\": 0.0, \"max\": 1.0, \"default\": 1.0}"
    "]"
    "}";
}
