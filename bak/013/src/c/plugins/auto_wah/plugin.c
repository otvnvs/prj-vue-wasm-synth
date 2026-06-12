// src/c/plugin_autowah.c
#include "plugin.h"
#include <stdlib.h>
#include <math.h>
#include <emscripten.h>

typedef struct {
    float sensitivity; // Envelope filter depth (0.1 to 5.0)
    float resonance;   // Filter sharpness/Q (0.05 to 0.95)
    float mix;         // Dry/Wet blend (0.0 to 1.0)
    
    // Envelope detector tracking state
    float envelope;
    
    // State Variable Filter (SVF) internal memory registers
    float ic1eq;
    float ic2eq;
} AutoWahState;

EMSCRIPTEN_KEEPALIVE
EXPORT void* create_instance() {
    AutoWahState* state = (AutoWahState*)malloc(sizeof(AutoWahState));
    if (!state) return NULL;

    state->sensitivity = 2.0f;
    state->resonance = 0.5f;
    state->mix = 1.0f;
    state->envelope = 0.0f;
    state->ic1eq = 0.0f;
    state->ic2eq = 0.0f;

    return (void*)state;
}

EMSCRIPTEN_KEEPALIVE
EXPORT void set_parameter(void* instance, int param_id, float value) {
    AutoWahState* state = (AutoWahState*)instance;
    if (!state) return;

    switch (param_id) {
        case 0: state->sensitivity = value; break;
        case 1: state->resonance = value; break;
        case 2: state->mix = value; break;
    }
}

EMSCRIPTEN_KEEPALIVE
EXPORT void process(void* instance, float* input, float* output, int sample_count, float sample_rate) {
    AutoWahState* state = (AutoWahState*)instance;
    if (!state) return;

    // Time constants for envelope tracking
    float attack_coef = 1.0f - expf(-1.0f / (0.010f * sample_rate)); // 10ms attack
    float release_coef = 1.0f - expf(-1.0f / (0.150f * sample_rate)); // 150ms release

    // Quality factor dampening calculation derived from resonance parameter
    float damping = 2.0f * (1.0f - state->resonance);

    for (int i = 0; i < sample_count; i++) {
        float dry_sample = input[i];

        // 1. Envelope Detector (Full-wave rectification + asymmetric smoothing)
        float abs_input = fabsf(dry_sample);
        if (abs_input > state->envelope) {
            state->envelope += attack_coef * (abs_input - state->envelope);
        } else {
            state->envelope += release_coef * (abs_input - state->envelope);
        }

        // 2. Map Envelope to Filter Cutoff frequency
        // Clamp normalized modulation between 200Hz base and ~5000Hz peak
        float cutoff_mod = state->envelope * state->sensitivity;
        if (cutoff_mod > 1.0f) cutoff_mod = 1.0f;
        
        float target_freq = 200.0f + (4800.0f * cutoff_mod);
        
        // 3. Andrew Simper's SVF Chamberlin Filter topology equations
        float g = tanf((3.14159265f * target_freq) / sample_rate);
        float k = damping;
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;

        // Process step through the SVF structure
        float v0 = dry_sample;
        float v1 = a1 * state->ic1eq + a2 * (v0 - state->ic2eq);
        float v2 = state->ic2eq + g * v1;

        // Internal register updates for next iteration loop step
        state->ic1eq = 2.0f * v1 - state->ic1eq;
        state->ic2eq = 2.0f * v2 - state->ic2eq;

        // v2 holds the Low-pass filter output stream
        float wet_sample = v2;

        // 4. Mix dry and wet processed nodes
        output[i] = (dry_sample * (1.0f - state->mix)) + (wet_sample * state->mix);
    }
}

EMSCRIPTEN_KEEPALIVE
EXPORT void destroy_instance(void* instance) {
    AutoWahState* state = (AutoWahState*)instance;
    if (!state) return;
    free(state);
}

EMSCRIPTEN_KEEPALIVE
EXPORT const char* get_plugin_manifest() {
    return "{"
        "\"name\": \"Envelope Auto-Wah\","
        "\"id_tag\": \"mono_autowah\","
        "\"type\": \"processor\","
        "\"inputs\": 1,"
        "\"outputs\": 1,"
        "\"parameters\": ["
            "{\"id\": 0, \"name\": \"Sens\", \"min\": 0.1, \"max\": 5.0, \"default\": 2.0},"
            "{\"id\": 1, \"name\": \"Resonance\", \"min\": 0.05, \"max\": 0.95, \"default\": 0.5},"
            "{\"id\": 2, \"name\": \"MixBlend\", \"min\": 0.0, \"max\": 1.0, \"default\": 1.0}"
        "]"
    "}";
}

