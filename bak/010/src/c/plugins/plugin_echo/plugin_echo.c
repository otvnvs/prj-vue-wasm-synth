// src/c/plugin_echo.c
#include "plugin.h"
#include <stdlib.h>
#include <math.h>
#include <emscripten.h>

#define MAX_ECHO_SAMPLES 96000

typedef struct {
    float* ring_buffer;
    int write_ptr;
    float smoothed_delay;
    float time;
    float feedback;
    float mix;
} EchoPluginState;

EXPORT void* create_instance() {
    EchoPluginState* state = (EchoPluginState*)malloc(sizeof(EchoPluginState));
    if (!state) return NULL;

    state->ring_buffer = (float*)calloc(MAX_ECHO_SAMPLES, sizeof(float));
    state->write_ptr = 0;
    state->smoothed_delay = 0.0f;
    state->time = 0.3f;
    state->feedback = 0.4f;
    state->mix = 0.35f;
    return (void*)state;
}

EXPORT void set_parameter(void* instance, int param_id, float value) {
    EchoPluginState* state = (EchoPluginState*)instance;
    if (!state) return;

    switch (param_id) {
        case 0: state->time = value; break;
        case 1: state->feedback = value; break;
        case 2: state->mix = value; break;
    }
}

EXPORT void process(void* instance, float* input, float* output, int sample_count, float sample_rate) {
    EchoPluginState* state = (EchoPluginState*)instance;
    if (!state || !state->ring_buffer) return;

    for (int i = 0; i < sample_count; i++) {
        float dry_sample = input[i];

        float target_samples = state->time * sample_rate;
        if (target_samples >= MAX_ECHO_SAMPLES - 1) target_samples = MAX_ECHO_SAMPLES - 2;
        if (state->smoothed_delay == 0.0f) state->smoothed_delay = target_samples;

        // Apply tape speed glide interpolation
        state->smoothed_delay += (target_samples - state->smoothed_delay) * 0.001f;

        float read_ptr = (float)state->write_ptr - state->smoothed_delay;
        while (read_ptr < 0.0f) read_ptr += (float)MAX_ECHO_SAMPLES;

        int idx_floor = (int)read_ptr;
        int idx_ceil = (idx_floor + 1) % MAX_ECHO_SAMPLES;
        float frac = read_ptr - (float)idx_floor;

        float wet_sample = state->ring_buffer[idx_floor] + frac * (state->ring_buffer[idx_ceil] - state->ring_buffer[idx_floor]);

        // Write the processed output straight into the output buffer pointer array
        output[i] = (dry_sample * (1.0f - state->mix)) + (wet_sample * state->mix);

        // Store back in ring history memory
        state->ring_buffer[state->write_ptr] = dry_sample + (wet_sample * state->feedback);
        state->write_ptr = (state->write_ptr + 1) % MAX_ECHO_SAMPLES;
    }
}

EXPORT void destroy_instance(void* instance) {
    EchoPluginState* state = (EchoPluginState*)instance;
    if (!state) return;

    if (state->ring_buffer) free(state->ring_buffer);
    free(state);
}

// ... Append to the bottom of src/c/plugins/plugin_echo/plugin_echo.c
EMSCRIPTEN_KEEPALIVE
EXPORT const char* get_plugin_manifest() {
    return "{"
        "\"name\": \"Tape Echo\","
        "\"type\": \"effect\","
        "\"parameters\": ["
            "{\"id\": 0, \"name\": \"EchoTime\", \"min\": 0.05, \"max\": 1.5, \"default\": 0.3},"
            "{\"id\": 1, \"name\": \"Feedback\", \"min\": 0, \"max\": 0.95, \"default\": 0.4},"
            "{\"id\": 2, \"name\": \"MixBlend\", \"min\": 0, \"max\": 1, \"default\": 0.35}"
        "]"
    "}";
}

