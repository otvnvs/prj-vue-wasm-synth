#include "plugin.h"
#include <stdlib.h>
#include <math.h>
#include <emscripten.h>

#ifndef PI_CONSTANT
#define PI_CONSTANT 3.14159265358979323846f
#endif

typedef enum { STAGE_A, STAGE_D, STAGE_S, STAGE_R, STAGE_DONE } AdsrStage;

typedef struct VoiceNode {
    int key_id;
    float frequency;
    float phase_1;             // Oscillator 1 phase
    float phase_2;             // Oscillator 2 (Detuned) phase
    float target_volume;
    AdsrStage stage;
    float current_gain;
    float release_start_gain;
    float stage_time;
    float filter_state;        // Simple low-pass filter memory
    struct VoiceNode* next;
} VoiceNode;

typedef struct {
    VoiceNode* active_voices_head;
    float attack;
    float decay;
    float sustain;
    float release;
    float morph;               // Slider: 0.0 (Sine) -> 0.5 (Triangle) -> 1.0 (Square)
    float detune;              // Slider: 0.0 (Perfect Pitch) -> 1.0 (Heavy Chorus)
    float cutoff;              // Slider: 0.0 (Muffled) -> 1.0 (Fully Open)
    float amplitude;
} SynthPluginState;

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

EXPORT void* create_instance() {
    SynthPluginState* state = (SynthPluginState*)malloc(sizeof(SynthPluginState));
    if (!state) return NULL;
    state->active_voices_head = NULL;
    state->attack = 0.30f;       // Ambient default values
    state->decay = 0.50f;
    state->sustain = 0.80f;
    state->release = 1.50f;
    state->morph = 0.0f;         // Start pure sine
    state->detune = 0.15f;       // Sweet-spot unison default
    state->cutoff = 0.70f;       // Slightly warm top end
    state->amplitude = 0.8f;
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
        case 4: state->morph = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); break;
        case 5: state->detune = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); break;
        case 6: state->cutoff = value < 0.01f ? 0.01f : (value > 1.0f ? 1.0f : value); break;
        case 7: state->amplitude = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); break;
    }
}

EXPORT void trigger_note_on(void* instance, int key_id, float frequency, float volume) {
    SynthPluginState* state = (SynthPluginState*)instance;
    if (!state) return;
    
    VoiceNode* current = state->active_voices_head;
    while (current != NULL) {
        if (current->key_id == key_id) {
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
    new_voice->frequency = frequency;
    new_voice->phase_1 = 0.0f;
    new_voice->phase_2 = 0.0f;
    new_voice->target_volume = volume;
    new_voice->stage = STAGE_A;
    new_voice->current_gain = 0.0f;
    new_voice->stage_time = 0.0f;
    new_voice->filter_state = 0.0f;
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

// Inline helper to calculate morphed waveform geometry continuously
static inline float generate_morphic_wave(float phase, float morph) {
    float sine = sinf(phase);
    
    // Triangle generation from phase
    float tri = (phase < PI_CONSTANT) ? (2.0f * (phase / PI_CONSTANT) - 1.0f) : (3.0f - 2.0f * (phase / PI_CONSTANT));
    
    // Square generation from phase
    float sqr = (sine >= 0.0f) ? 1.0f : -1.0f;
    
    if (morph < 0.5f) {
        float t = morph * 2.0f; // Scale to 0.0 -> 1.0
        return (1.0f - t) * sine + t * tri;
    } else {
        float t = (morph - 0.5f) * 2.0f; // Scale to 0.0 -> 1.0
        return (1.0f - t) * tri + t * sqr;
    }
}

EXPORT void process(void* instance, float* input, float* output, int sample_count, float sample_rate) {
    SynthPluginState* state = (SynthPluginState*)instance;
    if (!state) return;
    
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
    
    // Unison detuning math (scales with the detune slider up to ~15 cents)
    float detune_factor = 1.004f * state->detune; 
    //float freq_1 = voice_freq = voice->frequency;//ockert
    
    VoiceNode* voice = state->active_voices_head;
    while (voice != NULL) {
        if (voice->stage == STAGE_DONE) { voice = voice->next; continue; }
        
        float f1 = voice->frequency;
        float f2 = voice->frequency * (1.0f + (0.015f * state->detune)); // detune voice 2 up
        
        float phase_inc_1 = (2.0f * PI_CONSTANT * f1) / sample_rate;
        float phase_inc_2 = (2.0f * PI_CONSTANT * f2) / sample_rate;
        
        // Dynamic pole coefficient calculation for the lowpass filter
        float filter_coef = state->cutoff * state->cutoff; 
        
        for (int i = 0; i < sample_count; i++) {
            // Mix two detuned osc nodes
            float osc1 = generate_morphic_wave(voice->phase_1, state->morph);
            float osc2 = generate_morphic_wave(voice->phase_2, state->morph);
            float raw_sample = (osc1 + osc2) * 0.5f;
            
            process_node_adsr(voice, state, sample_rate);
            
            // Run a One-Pole low-pass filter
            voice->filter_state = voice->filter_state + filter_coef * (raw_sample - voice->filter_state);
            
            output[i] += voice->filter_state * voice->target_volume * voice->current_gain * mix_gain * state->amplitude;
            
            voice->phase_1 += phase_inc_1;
            if (voice->phase_1 >= 2.0f * PI_CONSTANT) voice->phase_1 -= 2.0f * PI_CONSTANT;
            
            voice->phase_2 += phase_inc_2;
            if (voice->phase_2 >= 2.0f * PI_CONSTANT) voice->phase_2 -= 2.0f * PI_CONSTANT;
        }
        voice = voice->next;
    }
    
    // Clean up expired nodes
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
    "\"name\": \"Organ Wave Blender\","
    "\"id_tag\": \"organ_blender\","
    "\"type\": \"generator\","
    "\"inputs\": 0,"
    "\"outputs\": 1,"
    "\"parameters\": ["
    "{\"id\": 0, \"name\": \"Attack\", \"min\": 0, \"max\": 2, \"default\": 0.05},"
    "{\"id\": 1, \"name\": \"Decay\", \"min\": 0, \"max\": 2, \"default\": 0.15},"
    "{\"id\": 2, \"name\": \"Sustain\", \"min\": 0, \"max\": 1, \"default\": 0.6},"
    "{\"id\": 3, \"name\": \"Release\", \"min\": 0, \"max\": 8, \"default\": 0.4},"
    "{\"id\": 4, \"name\": \"Sine Mix\", \"min\": 0, \"max\": 1, \"default\": 0.7},"
    "{\"id\": 5, \"name\": \"Square Mix\", \"min\": 0, \"max\": 1, \"default\": 0.2},"
    "{\"id\": 6, \"name\": \"Saw Mix\", \"min\": 0, \"max\": 1, \"default\": 0.1},"
    "{\"id\": 7, \"name\": \"Master Volume\", \"min\": 0, \"max\": 1, \"default\": 0.8}"
    "]"
    "}";
}

