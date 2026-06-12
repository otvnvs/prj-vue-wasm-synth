// src/c/plugin_synth.c
#include "plugin.h"
#include <stdlib.h>
#include <math.h>
#include <emscripten.h>

// Structural internal state tracking definitions
typedef enum { STAGE_A, STAGE_D, STAGE_S, STAGE_R, STAGE_DONE } AdsrStage;

typedef struct VoiceNode {
    int key_id;
    int type; // 0=sine, 1=custom, 2=saw
    float frequency;
    float phase;
    float target_volume;
    AdsrStage stage;
    float current_gain;
    float release_start_gain;
    float stage_time;
    struct VoiceNode* next;
} VoiceNode;

// Private state struct for this plugin node instance
typedef struct {
    VoiceNode* active_voices_head;
    float attack;
    float decay;
    float sustain;
    float release;
    int active_wave_type;
} SynthPluginState;

// --- Helper Functions Local to this Module ---
static void process_node_adsr(VoiceNode* voice, SynthPluginState* state, float sample_rate) {
    float dt = 1.0f / sample_rate;
    voice->stage_time += dt;

    switch (voice->stage) {
        case STAGE_A:
            if (state->attack <= 0.0f) { voice->stage = STAGE_D; voice->stage_time = 0.0f; voice->current_gain = 1.0f; }
            else {
                voice->current_gain = voice->stage_time / state->attack;
                if (voice->stage_time >= state->attack) { voice->stage = STAGE_D; voice->stage_time = 0.0f; voice->current_gain = 1.0f; }
            }
            break;
        case STAGE_D:
            if (state->decay <= 0.0f) { voice->stage = STAGE_S; voice->stage_time = 0.0f; voice->current_gain = state->sustain; }
            else {
                voice->current_gain = 1.0f - ((1.0f - state->sustain) * (voice->stage_time / state->decay));
                if (voice->stage_time >= state->decay) { voice->stage = STAGE_S; voice->stage_time = 0.0f; voice->current_gain = state->sustain; }
            }
            break;
        case STAGE_S:
            voice->current_gain = state->sustain;
            break;
        case STAGE_R:
            if (state->release <= 0.0f) { voice->stage = STAGE_DONE; voice->current_gain = 0.0f; }
            else {
                voice->current_gain = voice->release_start_gain * (1.0f - (voice->stage_time / state->release));
                if (voice->stage_time >= state->release || voice->current_gain <= 0.0f) { voice->stage = STAGE_DONE; voice->current_gain = 0.0f; }
            }
            break;
        default: voice->current_gain = 0.0f; break;
    }
}

// --- MANDATORY UNIFIED INTERFACE LIFECYCLE HOOKS ---

EXPORT void* create_instance() {
    SynthPluginState* state = (SynthPluginState*)malloc(sizeof(SynthPluginState));
    if (!state) return NULL;
    
    state->active_voices_head = NULL;
    state->attack = 0.05f;
    state->decay = 0.15f;
    state->sustain = 0.60f;
    state->release = 0.40f;
    state->active_wave_type = 0; // Sine
    return (void*)state;
}

EXPORT void set_parameter(void* instance, int param_id, float value) {
    SynthPluginState* state = (SynthPluginState*)instance;
    if (!state) return;

    switch (param_id) {
        case 0: state->attack = value; break;
        case 1: state->decay = value; break;
        case 2: state->sustain = value; break;
        case 3: state->release = value; break;
        case 4: state->active_wave_type = (int)value; break;
    }
}

// Custom control endpoints mapped inside the instance context
EXPORT void trigger_note_on(void* instance, int key_id, float frequency, float volume) {
    SynthPluginState* state = (SynthPluginState*)instance;
    if (!state) return;

    VoiceNode* current = state->active_voices_head;
    while (current != NULL) {
        if (current->key_id == key_id) {
            current->type = state->active_wave_type;
            current->frequency = frequency;
            current->target_volume = volume;
            current->stage = STAGE_A;
            current->stage_time = 0.0f;
            return;
        }
        current = current->next;
    }

    VoiceNode* new_voice = (VoiceNode*)malloc(sizeof(VoiceNode));
    if (!new_voice) return;

    new_voice->key_id = key_id;
    new_voice->type = state->active_wave_type;
    new_voice->frequency = frequency;
    new_voice->phase = 0.0f;
    new_voice->target_volume = volume;
    new_voice->stage = STAGE_A;
    new_voice->current_gain = 0.0f;
    new_voice->stage_time = 0.0f;

    new_voice->next = state->active_voices_head;
    state->active_voices_head = new_voice;
}

EXPORT void trigger_note_off(void* instance, int key_id) {
    SynthPluginState* state = (SynthPluginState*)instance;
    if (!state) return;

    VoiceNode* current = state->active_voices_head;
    while (current != NULL) {
        if (current->key_id == key_id && current->stage != STAGE_R && current->stage != STAGE_DONE) {
            current->release_start_gain = current->current_gain;
            current->stage = STAGE_R;
            current->stage_time = 0.0f;
            return;
        }
        current = current->next;
    }
}

EXPORT void process(void* instance, float* input, float* output, int sample_count, float sample_rate) {
    SynthPluginState* state = (SynthPluginState*)instance;
    if (!state) return;

    // Clear the output stream block buffer first (Generator node ignores incoming 'input' array)
    for (int i = 0; i < sample_count; i++) {
        output[i] = 0.0f;
    }

    int active_count = 0;
    VoiceNode* count_cursor = state->active_voices_head;
    while (count_cursor != NULL) {
        if (count_cursor->stage != STAGE_DONE) active_count++;
        count_cursor = count_cursor->next;
    }

    if (active_count == 0) return;
    float mix_gain = 1.0f / sqrtf((float)active_count);

    VoiceNode* voice = state->active_voices_head;
    while (voice != NULL) {
        if (voice->stage == STAGE_DONE) { voice = voice->next; continue; }

        float phase_increment = (2.0f * PI_CONSTANT * voice->frequency) / sample_rate;

        for (int i = 0; i < sample_count; i++) {
            float sample = 0.0f;
            
            if (voice->type == 0) { // Sine
                sample = sinf(voice->phase);
            } else if (voice->type == 1) { // Square approximation
                sample = sinf(voice->phase) >= 0.0f ? 1.0f : -1.0f;
            } else if (voice->type == 2) { // Sawtooth
                sample = (voice->phase / PI_CONSTANT) - 1.0f;
            }

            process_node_adsr(voice, state, sample_rate);
            output[i] += sample * voice->target_volume * voice->current_gain * mix_gain;

            voice->phase += phase_increment;
            if (voice->phase >= 2.0f * PI_CONSTANT) voice->phase -= 2.0f * PI_CONSTANT;
        }
        voice = voice->next;
    }

    // Node Garbage Collection Housekeeping
    VoiceNode* current = state->active_voices_head;
    VoiceNode* previous = NULL;
    while (current != NULL) {
        if (current->stage == STAGE_DONE) {
            VoiceNode* to_delete = current;
            if (previous == NULL) { state->active_voices_head = current->next; current = state->active_voices_head; }
            else { previous->next = current->next; current = current->next; }
            free(to_delete);
        } else { previous = current; current = current->next; }
    }
}

EXPORT void destroy_instance(void* instance) {
    SynthPluginState* state = (SynthPluginState*)instance;
    if (!state) return;

    VoiceNode* current = state->active_voices_head;
    while (current != NULL) {
        VoiceNode* next = current->next;
        free(current);
        current = next;
    }
    free(state);
}
EMSCRIPTEN_KEEPALIVE
EXPORT const char* get_plugin_manifest() {
    return "{"
        "\"name\": \"Polyphonic Synth\","
        "\"id_tag\": \"synth_core\","
        "\"type\": \"generator\","
        "\"inputs\": 0,"
        "\"outputs\": 1,"
        "\"parameters\": ["
            "{\"id\": 0, \"name\": \"Attack\", \"min\": 0, \"max\": 2, \"default\": 0.05},"
            "{\"id\": 1, \"name\": \"Decay\", \"min\": 0, \"max\": 2, \"default\": 0.15},"
            "{\"id\": 2, \"name\": \"Sustain\", \"min\": 0, \"max\": 1, \"default\": 0.6},"
            "{\"id\": 3, \"name\": \"Release\", \"min\": 0, \"max\": 2, \"default\": 0.4},"
            "{\"id\": 4, \"name\": \"WaveformType\", \"min\": 0, \"max\": 2, \"default\": 0}"
        "]"
    "}";
}
