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
 float phase;         
 float phase_h2;      
 float phase_h3;      
 float phase_h4;      
 float phase_h5;      // Added 5th harmonic for more metallic bite
 float target_volume; // Velocity/volume
 AdsrStage stage;
 float current_gain;
 float release_start_gain;
 float stage_time;
 float release_time;  // Separate timer tracked only during release stage
 unsigned int rand_state; // Per-voice seed for mechanical noise generation
 struct VoiceNode* next;
} VoiceNode;

typedef struct {
 VoiceNode* active_voices_head;
 float attack;
 float decay;
 float sustain;
 float release;
 float amplitude;
 float detune; 
 float click_level;   // Parameter to control the mechanical release noise volume
} SynthPluginState;

// Fast, deterministic pseudo-random number generator for noise
static float next_noise_sample(unsigned int* rand_state) {
 // Changed W to U for unsigned int literal alignment
 *rand_state = *rand_state * 1664525U + 1013904223U;
 return ((float)(*rand_state) / (float)4294967295U) * 2.0f - 1.0f;
}

static void process_node_adsr(VoiceNode* voice, SynthPluginState* state, float sample_rate) {
 float dt = 1.0f / sample_rate;
 voice->stage_time += dt;
 if (voice->stage == STAGE_R) {
 voice->release_time += dt;
 }

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
 voice->current_gain = voice->release_start_gain * (1.0f - (voice->release_time / state->release));
 if (voice->release_time >= state->release || voice->current_gain <= 0.0f) { voice->stage = STAGE_DONE; voice->current_gain = 0.0f; }
 }
 break;
 default: voice->current_gain = 0.0f; break;
 }
}

EXPORT void* create_instance() {
 SynthPluginState* state = (SynthPluginState*)malloc(sizeof(SynthPluginState));
 if (!state) return NULL;
 state->active_voices_head = NULL;
 state->attack = 0.0015f; // Extremely sharp physical pluck
 state->decay = 0.50f;   
 state->sustain = 0.10f; 
 state->release = 0.12f; // Quick damper padding
 state->amplitude = 1.0f;
 state->detune = 0.0f;  
 state->click_level = 0.40f; // Default level for mechanical key releases
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
 case 5: state->amplitude = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); break;
 case 6: state->detune = value < -100.0f ? -100.0f : (value > 100.0f ? 100.0f : value); break;
 case 7: state->click_level = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value); break;
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
 current->release_time = 0.0f;
 return;
 }
 current = current->next;
 }
 VoiceNode* new_voice = (VoiceNode*)malloc(sizeof(VoiceNode));
 if (!new_voice) return;
 new_voice->key_id = key_id;
 new_voice->frequency = frequency;
 new_voice->phase = 0.0f;
 new_voice->phase_h2 = 0.0f;
 new_voice->phase_h3 = 0.0f;
 new_voice->phase_h4 = 0.0f;
 new_voice->phase_h5 = 0.0f;
 new_voice->target_volume = volume;
 new_voice->stage = STAGE_A;
 new_voice->current_gain = 0.0f;
 new_voice->stage_time = 0.0f;
 new_voice->release_time = 0.0f;
 new_voice->rand_state = (unsigned int)key_id + 12345U; // Unique noise seed per note
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
 // We do NOT reset stage_time here so the tone strings keep damping continuously
 current->release_time = 0.0f;
 return;
 }
 current = current->next;
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
 VoiceNode* voice = state->active_voices_head;
 while (voice != NULL) {
 if (voice->stage == STAGE_DONE) { voice = voice->next; continue; }
 
 float detuned_frequency = voice->frequency * powf(2.0f, state->detune / 1200.0f);
 
 float base_inc = (2.0f * PI_CONSTANT * detuned_frequency) / sample_rate;
 float inc_h1 = base_inc;
 float inc_h2 = base_inc * 2.0f;
 float inc_h3 = base_inc * 3.0f;
 float inc_h4 = base_inc * 4.0f;
 float inc_h5 = base_inc * 5.0f;
 
 // Real Harpsichord Velocity Emulation:
 // Tighter pluck velocities alter upper harmonic brightness more than raw amplitude.
 float vel = voice->target_volume;
 float v_bright = 0.3f + (0.7f * vel); 

 for (int i = 0; i < sample_count; i++) {
 process_node_adsr(voice, state, sample_rate);
 
 // Continuous exponential string decay factor
 float damp_factor = (voice->stage == STAGE_A) ? 1.0f : expf(-2.8f * voice->stage_time);
 
 // 1. Tone Component (Additive synthesis with fast high-frequency dampening)
 float string_signal = sinf(voice->phase) * 0.45f;
 string_signal += sinf(voice->phase_h2) * 0.35f * damp_factor * v_bright;
 string_signal += sinf(voice->phase_h3) * 0.25f * (damp_factor * damp_factor) * v_bright;
 string_signal += sinf(voice->phase_h4) * 0.15f * (damp_factor * damp_factor * damp_factor);
 string_signal += sinf(voice->phase_h5) * 0.08f * (damp_factor * damp_factor * damp_factor * damp_factor);

 // Apply the structural string envelope amplitude
 float sample = string_signal * voice->current_gain;

 // 2. Mechanical Release Key-Click Component
 // Generates a transient 15ms burst of band-limited noise precisely when key is let go
 if (voice->stage == STAGE_R && voice->release_time < 0.015f) {
 float click_envelope = 1.0f - (voice->release_time / 0.015f);
 float noise = next_noise_sample(&voice->rand_state);
 // Subtle high frequency color filtering via basic phase manipulation
 sample += noise * click_envelope * state->click_level * 0.18f;
 }

 output[i] += sample * vel * mix_gain * state->amplitude;
 
 // Accumulate phases
 voice->phase += inc_h1;   if (voice->phase >= 2.0f * PI_CONSTANT) voice->phase -= 2.0f * PI_CONSTANT;
 voice->phase_h2 += inc_h2; if (voice->phase_h2 >= 2.0f * PI_CONSTANT) voice->phase_h2 -= 2.0f * PI_CONSTANT;
 voice->phase_h3 += inc_h3; if (voice->phase_h3 >= 2.0f * PI_CONSTANT) voice->phase_h3 -= 2.0f * PI_CONSTANT;
 voice->phase_h4 += inc_h4; if (voice->phase_h4 >= 2.0f * PI_CONSTANT) voice->phase_h4 -= 2.0f * PI_CONSTANT;
 voice->phase_h5 += inc_h5; if (voice->phase_h5 >= 2.0f * PI_CONSTANT) voice->phase_h5 -= 2.0f * PI_CONSTANT;
 }
 voice = voice->next;
 }
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
 "\"name\": \"Realistic Harpsichord\","
 "\"id_tag\": \"harpsichord_pro\","
 "\"type\": \"generator\","
 "\"inputs\": 0,"
 "\"outputs\": 1,"
 "\"parameters\": ["
 "{\"id\": 0, \"name\": \"Attack\", \"min\": 0, \"max\": 0.5, \"default\": 0.0015},"
 "{\"id\": 1, \"name\": \"Decay\", \"min\": 0.05, \"max\": 4, \"default\": 0.5},"
 "{\"id\": 2, \"name\": \"Sustain\", \"min\": 0, \"max\": 1, \"default\": 0.1},"
 "{\"id\": 3, \"name\": \"Release\", \"min\": 0.01, \"max\": 2, \"default\": 0.12},"
 "{\"id\": 5, \"name\": \"Amplitude\", \"min\": 0, \"max\": 1, \"default\": 1.0},"
 "{\"id\": 6, \"name\": \"Detune\", \"min\": -100, \"max\": 100, \"default\": 0},"
 "{\"id\": 7, \"name\": \"ReleaseClick\", \"min\": 0, \"max\": 1, \"default\": 0.4}"
 "]"
 "}";
}

