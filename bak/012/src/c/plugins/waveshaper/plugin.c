// src/c/plugin_distortion.c
#include "plugin.h"
#include <stdlib.h>
#include <math.h>
#include <emscripten.h>

#define PI 3.14159265358979323846f

typedef struct {
    float drive;   // Input amplification factor (1.0 to 20.0)
    float tone;    // Low-pass filter cutoff factor (0.1 to 1.0)
    float mix;     // Dry/Wet blend (0.0 to 1.0)
    
    // Low-pass state memory for the tone control
    float lowpass_state;
} DistortionState;

EMSCRIPTEN_KEEPALIVE
EXPORT void* create_instance() {
    DistortionState* state = (DistortionState*)malloc(sizeof(DistortionState));
    if (!state) return NULL;

    state->drive = 5.0f;
    state->tone = 0.5f;
    state->mix = 1.0f;
    state->lowpass_state = 0.0f;

    return (void*)state;
}

EMSCRIPTEN_KEEPALIVE
EXPORT void set_parameter(void* instance, int param_id, float value) {
    DistortionState* state = (DistortionState*)instance;
    if (!state) return;

    switch (param_id) {
        case 0: state->drive = value; break;
        case 1: state->tone = value; break;
        case 2: state->mix = value; break;
    }
}

EMSCRIPTEN_KEEPALIVE
EXPORT void process(void* instance, float* input, float* output, int sample_count, float sample_rate) {
    DistortionState* state = (DistortionState*)instance;
    if (!state) return;

    // Simple one-pole lowpass filter smoothing coefficient based on tone parameter
    float alpha = state->tone * 0.25f; 

    for (int i = 0; i < sample_count; i++) {
        float dry_sample = input[i];

        // 1. Pre-Gain Stage
        float saturated = dry_sample * state->drive;

        // 2. Waveshaping Soft-Clipping (using Arctangent)
        // Normalised by multiplying by 2/PI so peak remains close to 1.0
        saturated = (2.0f / PI) * atanf(saturated);

        // 3. Post-Saturation Tone Filter (Smooths out harsh, raspy high-frequency fizz)
        state->lowpass_state = state->lowpass_state + alpha * (saturated - state->lowpass_state);
        float wet_sample = state->lowpass_state;

        // 4. Dry/Wet Mix
        output[i] = (dry_sample * (1.0f - state->mix)) + (wet_sample * state->mix);
    }
}

EMSCRIPTEN_KEEPALIVE
EXPORT void destroy_instance(void* instance) {
    DistortionState* state = (DistortionState*)instance;
    if (!state) return;
    free(state);
}

EMSCRIPTEN_KEEPALIVE
EXPORT const char* get_plugin_manifest() {
    return "{"
        "\"name\": \"Saturated Overdrive\","
        "\"id_tag\": \"mono_distortion\","
        "\"type\": \"processor\","
        "\"inputs\": 1,"
        "\"outputs\": 1,"
        "\"parameters\": ["
            "{\"id\": 0, \"name\": \"Drive\", \"min\": 1.0, \"max\": 20.0, \"default\": 5.0},"
            "{\"id\": 1, \"name\": \"Tone\", \"min\": 0.05, \"max\": 1.0, \"default\": 0.5},"
            "{\"id\": 2, \"name\": \"MixBlend\", \"min\": 0.0, \"max\": 1.0, \"default\": 1.0}"
        "]"
    "}";
}

