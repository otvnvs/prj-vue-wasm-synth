// src/c/plugin_filter.c
#include "plugin.h"
#include <stdlib.h>
#include <math.h>
#include <emscripten.h>

typedef struct {
    // State capacitor registers tracking past feedback positions
    float ic1eq;
    float ic2eq;
    // Sliders variable bindings
    float cutoff;    // Frequency in Hz (e.g. 500Hz)
    float resonance; // Resonance Q factor (0.1 to 10.0)
} FilterPluginState;

EXPORT void* create_instance() {
    FilterPluginState* state = (FilterPluginState*)malloc(sizeof(FilterPluginState));
    if (!state) return NULL;

    state->ic1eq = 0.0f;
    state->ic2eq = 0.0f;
    state->cutoff = 800.0f;     // Default warm cutoff frequency
    state->resonance = 1.0f;    // Neutral Q dampening point
    return (void*)state;
}

EXPORT void set_parameter(void* instance, int param_id, float value) {
    FilterPluginState* state = (FilterPluginState*)instance;
    if (!state) return;

    switch (param_id) {
        case 0: state->cutoff = value; break;
        case 1: state->resonance = value; break;
    }
}

/**
 * Sample-Accurate State Variable Filter (SVF) DSP Processing Loop
 */
EXPORT void process(void* instance, float* input, float* output, int sample_count, float sample_rate) {
    FilterPluginState* state = (FilterPluginState*)instance;
    if (!state) return;

    // Bounds checking to keep the algorithm stable
    float co = state->cutoff;
    if (co < 20.0f) co = 20.0f;
    if (co > (sample_rate * 0.45f)) co = sample_rate * 0.45f;

    float q = state->resonance;
    if (q < 0.1f) q = 0.1f;

    // Calculate SVF filter tracking coefficients (Andrew Simper's design layout)
    float g = tanf(PI_CONSTANT * co / sample_rate);
    float k = 1.0f / q;
    float a1 = 1.0f / (1.0f + g * (g + k));
    float a2 = g * a1;
    float a3 = g * a2;

    for (int i = 0; i < sample_count; i++) {
        float v0 = input[i];

        // SVF node state equation mixing evaluations
        float v1 = a1 * state->ic1eq + a2 * (v0 - state->ic2eq);
        float v2 = state->ic2eq + g * v1 + a3 * (v0 - state->ic1eq);

        // Low-Pass output calculation extraction
        float low_pass_sample = v2;

        // Advance internal capacitor tracking loops
        state->ic1eq = 2.0f * v1 - state->ic1eq;
        state->ic2eq = 2.0f * v2 - state->ic2eq;

        // Stream the processed frame straight out to output memory bus
        output[i] = low_pass_sample;
    }
}

EXPORT void destroy_instance(void* instance) {
    FilterPluginState* state = (FilterPluginState*)instance;
    if (!state) return;
    free(state);
}


// --- NEW SINGLE MANIFEST DISCOVERY HOOK ---
EMSCRIPTEN_KEEPALIVE
// Inside src/c/plugins/plugin_filter/plugin_filter.c

EXPORT const char* get_plugin_manifest() {
    return "{"
        "\"name\": \"Resonant Filter\","
        "\"id_tag\": \"resonant_filter\","
        "\"type\": \"processor\","
        "\"inputs\": 1,"
        "\"outputs\": 1,"
        "\"parameters\": ["
            "{\"id\": 0, \"name\": \"Cutoff\", \"min\": 100, \"max\": 4000, \"default\": 800},"
            "{\"id\": 1, \"name\": \"Resonance\", \"min\": 0.5, \"max\": 8, \"default\": 1}"
        "]"
    "}";
}

