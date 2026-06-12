#include <emscripten.h>
#include <stdlib.h>
#include <math.h>
#include "effects.h"
#include "ftable.h"

#define PI 3.14159265358979323846f

typedef enum {
    STAGE_ATTACK,
    STAGE_DECAY,
    STAGE_SUSTAIN,
    STAGE_RELEASE,
    STAGE_FINISHED
} EnvelopeStage;

typedef struct VoiceNode {
    int key_id;
    int type;
    float frequency;
    float phase;
    float target_volume;
    
    EnvelopeStage stage;
    float current_gain;
    float release_start_gain;
    float stage_time;

    struct VoiceNode* next;
} VoiceNode;

static VoiceNode* active_voices_head = NULL;

// NEW: Global mutable ADSR parameters (with defaults)
static float adsr_attack  = 0.05f; 
static float adsr_decay   = 0.15f; 
static float adsr_sustain = 0.60f; 
static float adsr_release = 0.40f; 

EMSCRIPTEN_KEEPALIVE
void set_adsr_parameters(float attack, float decay, float sustain, float release) {
    adsr_attack  = attack;
    adsr_decay   = decay;
    adsr_sustain = sustain;
    adsr_release = release;
}

EMSCRIPTEN_KEEPALIVE
void set_tremolo_parameters(int active, float speed, float depth) {
    tremolo_active = active;
    tremolo_speed = speed;
    tremolo_depth = depth;
}


EMSCRIPTEN_KEEPALIVE
void set_echo_parameters(int active, float time, float feedback, float mix) {
    echo_active = active;
    echo_time = time;
    echo_feedback = feedback;
    echo_mix = mix;
}

EMSCRIPTEN_KEEPALIVE
void set_custom_harmonics_api(float* weights, int count) {
    set_custom_harmonics(weights, count);
}

EMSCRIPTEN_KEEPALIVE
void set_distortion_parameters_api(int active, float drive, float blend) { dist_active = active; dist_drive = drive; dist_blend = blend; }

EMSCRIPTEN_KEEPALIVE
void set_chorus_parameters_api(int active, float rate, float depth) { chorus_active = active; chorus_rate = rate; chorus_depth = depth; }



void process_voice_adsr(VoiceNode* voice, float sample_rate) {
    float dt = 1.0f / sample_rate;
    voice->stage_time += dt;

    switch (voice->stage) {
        case STAGE_ATTACK:
            // Prevent division by zero if Attack is set to 0ms
            if (adsr_attack <= 0.0f) {
                voice->stage = STAGE_DECAY;
                voice->stage_time = 0.0f;
                voice->current_gain = 1.0f;
            } else {
                voice->current_gain = voice->stage_time / adsr_attack;
                if (voice->stage_time >= adsr_attack) {
                    voice->stage = STAGE_DECAY;
                    voice->stage_time = 0.0f;
                    voice->current_gain = 1.0f;
                }
            }
            break;

        case STAGE_DECAY:
            if (adsr_decay <= 0.0f) {
                voice->stage = STAGE_SUSTAIN;
                voice->stage_time = 0.0f;
                voice->current_gain = adsr_sustain;
            } else {
                voice->current_gain = 1.0f - ((1.0f - adsr_sustain) * (voice->stage_time / adsr_decay));
                if (voice->stage_time >= adsr_decay) {
                    voice->stage = STAGE_SUSTAIN;
                    voice->stage_time = 0.0f;
                    voice->current_gain = adsr_sustain;
                }
            }
            break;

        case STAGE_SUSTAIN:
            voice->current_gain = adsr_sustain;
            break;

        case STAGE_RELEASE:
            if (adsr_release <= 0.0f) {
                voice->stage = STAGE_FINISHED;
                voice->current_gain = 0.0f;
            } else {
                voice->current_gain = voice->release_start_gain * (1.0f - (voice->stage_time / adsr_release));
                if (voice->stage_time >= adsr_release || voice->current_gain <= 0.0f) {
                    voice->stage = STAGE_FINISHED;
                    voice->current_gain = 0.0f;
                }
            }
            break;

        default:
            voice->current_gain = 0.0f;
            break;
    }
}

// 3. Trigger Node Insertion (Note On)
EMSCRIPTEN_KEEPALIVE
void note_on(int key_id, int type, float frequency, float volume) {
    //printf("void note_on(int key_id, int type, float frequency, float volume)\n");
    VoiceNode* current = active_voices_head;
    while (current != NULL) {
        // If note is already playing (or actively releasing), re-trigger it from Attack
        if (current->key_id == key_id) {
            current->type = type;
            current->frequency = frequency;
            current->target_volume = volume;
            current->stage = STAGE_ATTACK;
            current->stage_time = 0.0f;
            return;
        }
        current = current->next;
    }

    // Allocate an isolated memory block for the new note voice
    VoiceNode* new_voice = (VoiceNode*)malloc(sizeof(VoiceNode));
    if (new_voice == NULL) return;

    new_voice->key_id = key_id;
    new_voice->type = type;
    new_voice->frequency = frequency;
    new_voice->phase = 0.0f;
    new_voice->target_volume = volume;
    
    // Initialise Envelope at zero, ramping into Attack stage
    new_voice->stage = STAGE_ATTACK;
    new_voice->current_gain = 0.0f;
    new_voice->stage_time = 0.0f;

    new_voice->next = active_voices_head;
    active_voices_head = new_voice;
}

// 4. Trigger Node Modification (Note Off)
// Instead of freeing memory instantly, we shift the note into the Release stage.
EMSCRIPTEN_KEEPALIVE
void note_off(int key_id) {
    //printf("void note_off(int key_id)\n");
    VoiceNode* current = active_voices_head;
    while (current != NULL) {
        // Only target notes that are not already releasing or finished
        if (current->key_id == key_id && current->stage != STAGE_RELEASE && current->stage != STAGE_FINISHED) {
            current->release_start_gain = current->current_gain;
            current->stage = STAGE_RELEASE;
            current->stage_time = 0.0f;
            return;
        }
        current = current->next;
    }
}

// 5. Advanced Polyphonic Mixer Loop (Includes Node Housekeeping)
/*
EMSCRIPTEN_KEEPALIVE
void stream_mix_polyphonic(float* buffer, int sample_count, float sample_rate) {
    //printf("void stream_mix_polyphonic(float* buffer, int sample_count, float sample_rate)\n");
    // Reset buffer track chunk outputs
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = 0.0f;
    }

    // Pass 1: Count active non-finished voices to calculate automated mix levels
    int active_count = 0;
    VoiceNode* count_cursor = active_voices_head;
    while (count_cursor != NULL) {
        if (count_cursor->stage != STAGE_FINISHED) {
            active_count++;
        }
        count_cursor = count_cursor->next;
    }

    if (active_count == 0) return;

    float mix_gain = 1.0f / sqrtf((float)active_count);
    float phase_increment;

    // Pass 2: Process synthesis math and apply ADSR gain modulators
    VoiceNode* voice = active_voices_head;
    while (voice != NULL) {
        if (voice->stage == STAGE_FINISHED) {
            voice = voice->next;
            continue;
        }

        phase_increment = (2.0f * PI * voice->frequency) / sample_rate;

        for (int i = 0; i < sample_count; i++) {
            float sample = 0.0f;

            if (voice->type == 0) {       // Sine
                sample = sinf(voice->phase);
            } else if (voice->type == 1) { // Square
                sample = sinf(voice->phase) >= 0.0f ? 1.0f : -1.0f;
            } else if (voice->type == 2) { // Sawtooth
                sample = (voice->phase / PI) - 1.0f;
            }

            // Progress ADSR envelope sample-by-sample for smooth transitions
            process_voice_adsr(voice, sample_rate);

            // Mix into master stream adding the runtime ADSR volume tracking multiplier
            buffer[i] += sample * voice->target_volume * voice->current_gain * mix_gain;

            // Increment oscillator phase tracking
            voice->phase += phase_increment;
            if (voice->phase >= 2.0f * PI) voice->phase -= 2.0f * PI;
        }

        voice = voice->next;
    }

    // Pass 3: Garbage collection cleanup loop
    // Safely unlinks and calls free() on notes that reached the STAGE_FINISHED state.
    VoiceNode* current = active_voices_head;
    VoiceNode* previous = NULL;

    while (current != NULL) {
        if (current->stage == STAGE_FINISHED) {
            VoiceNode* to_delete = current;
            
            if (previous == NULL) {
                active_voices_head = current->next;
                current = active_voices_head;
            } else {
                previous->next = current->next;
                current = current->next;
            }
            
            free(to_delete);
        } else {
            previous = current;
            current = current->next;
        }
    }
}
*/
/*
EMSCRIPTEN_KEEPALIVE
void stream_mix_polyphonic(float* buffer, int sample_count, float sample_rate) {
    // 1. Wipe and clear the target chunk buffer output
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = 0.0f;
    }

    int active_count = 0;
    VoiceNode* count_cursor = active_voices_head;
    while (count_cursor != NULL) {
        if (count_cursor->stage != STAGE_FINISHED) active_count++;
        count_cursor = count_cursor->next;
    }

    if (active_count == 0) return;

    float mix_gain = 1.0f / sqrtf((float)active_count);
    VoiceNode* voice = active_voices_head;

    // 2. Generate raw polyphonic wave mixtures (DRY audio stage)
    while (voice != NULL) {
        if (voice->stage == STAGE_FINISHED) {
            voice = voice->next;
            continue;
        }
        float phase_increment = (2.0f * PI * voice->frequency) / sample_rate;

        for (int i = 0; i < sample_count; i++) {
            float sample = 0.0f;
            if (voice->type == 0)      sample = sinf(voice->phase);
            else if (voice->type == 1) sample = sinf(voice->phase) >= 0.0f ? 1.0f : -1.0f;
            else if (voice->type == 2) sample = (voice->phase / PI) - 1.0f;

            process_voice_adsr(voice, sample_rate);
            buffer[i] += sample * voice->target_volume * voice->current_gain * mix_gain;

            voice->phase += phase_increment;
            if (voice->phase >= 2.0f * PI) voice->phase -= 2.0f * PI;
        }
        voice = voice->next;
    }

    // 3. PIPELINE INTEGRATION CRITICAL LINE:
    // Route the combined polyphonic audio through our cascading effects chain
    process_effects_chain(buffer, sample_count, sample_rate);

    // 4. Run standard active voice node cleanup
    VoiceNode* current = active_voices_head;
    VoiceNode* previous = NULL;
    while (current != NULL) {
        if (current->stage == STAGE_FINISHED) {
            VoiceNode* to_delete = current;
            if (previous == NULL) { active_voices_head = current->next; current = active_voices_head; }
            else { previous->next = current->next; current = current->next; }
            free(to_delete);
        } else { previous = current; current = current->next; }
    }
}
*/
/*
EMSCRIPTEN_KEEPALIVE
void stream_mix_polyphonic(float* buffer, int sample_count, float sample_rate) {
    // 1. Always wipe and reset the outgoing buffer chunk first
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = 0.0f;
    }

    // 2. Count active non-finished voice threads
    int active_count = 0;
    VoiceNode* count_cursor = active_voices_head;
    while (count_cursor != NULL) {
        if (count_cursor->stage != STAGE_FINISHED) {
            active_count++;
        }
        count_cursor = count_cursor->next;
    }

    // REMOVED: "if (active_count == 0) return;" 
    // We let the execution fall through so the echo engine can process its tails!

    // 3. Only run oscillator accumulation loops IF voices are actually active
    if (active_count > 0) {
        float mix_gain = 1.0f / sqrtf((float)active_count);
        VoiceNode* voice = active_voices_head;

        while (voice != NULL) {
            if (voice->stage == STAGE_FINISHED) {
                voice = voice->next;
                continue;
            }

            float phase_increment = (2.0f * PI * voice->frequency) / sample_rate;

            for (int i = 0; i < sample_count; i++) {
                float sample = 0.0f;
                if (voice->type == 0)       sample = sinf(voice->phase);
                else if (voice->type == 1) sample = sinf(voice->phase) >= 0.0f ? 1.0f : -1.0f;
                else if (voice->type == 2) sample = (voice->phase / PI) - 1.0f;

                process_voice_adsr(voice, sample_rate);
                
                // Sum the dry synth signals together
                buffer[i] += sample * voice->target_volume * voice->current_gain * mix_gain;

                voice->phase += phase_increment;
                if (voice->phase >= 2.0f * PI) voice->phase -= 2.0f * PI;
            }
            voice = voice->next;
        }
    }

    // 4. MASTER EFFECTS RUN CONTINUOUSLY
    // Even if buffer is full of pure zeros (silence), the effects chain receives it,
    // mixes in any stored echoes from the tape memory loop, and passes it out.
    process_effects_chain(buffer, sample_count, sample_rate);

    // 5. Run standard garbage collection on finished voice nodes
    if (active_voices_head != NULL) {
        VoiceNode* current = active_voices_head;
        VoiceNode* previous = NULL;
        while (current != NULL) {
            if (current->stage == STAGE_FINISHED) {
                VoiceNode* to_delete = current;
                if (previous == NULL) {
                    active_voices_head = current->next;
                    current = active_voices_head;
                } else {
                    previous->next = current->next;
                    current = current->next;
                }
                free(to_delete);
            } else {
                previous = current;
                current = current->next;
            }
        }
    }
}
*/
/*
EMSCRIPTEN_KEEPALIVE
void stream_mix_polyphonic(float* buffer, int sample_count, float sample_rate) {
    // 1. Always wipe and reset the outgoing buffer chunk first
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = 0.0f;
    }

    // 2. Count active non-finished voice threads
    int active_count = 0;
    VoiceNode* count_cursor = active_voices_head;
    while (count_cursor != NULL) {
        if (count_cursor->stage != STAGE_FINISHED) {
            active_count++;
        }
        count_cursor = count_cursor->next;
    }

    // REMOVED: "if (active_count == 0) return;" 
    // We let the execution fall through so the echo engine can process its tails!

    // 3. Only run oscillator accumulation loops IF voices are actually active
    if (active_count > 0) {
        float mix_gain = 1.0f / sqrtf((float)active_count);
        VoiceNode* voice = active_voices_head;

        while (voice != NULL) {
            if (voice->stage == STAGE_FINISHED) {
                voice = voice->next;
                continue;
            }

            float phase_increment = (2.0f * PI * voice->frequency) / sample_rate;

            for (int i = 0; i < sample_count; i++) {
                float sample = 0.0f;
                
                // 1. Map the 0 -> 2*PI phase angle onto a 0.0 -> 2047.0 table index frame
                // (voice->phase / (2.0f * PI)) normalises it to 0.0 -> 1.0
                float ftable_index = (voice->phase / (2.0f * PI)) * (float)FTABLE_SIZE;
                if (ftable_index >= (float)FTABLE_SIZE) ftable_index = (float)FTABLE_SIZE - 1.0f;
                if (ftable_index < 0.0f) ftable_index = 0.0f;

                // 2. Linear Interpolation Lookup
                int index_floor = (int)ftable_index;
                int index_ceil = (index_floor + 1) % FTABLE_SIZE;
                float fractional_part = ftable_index - (float)index_floor;

                // 3. Dynamic Waveform Array Router Selector
                float* active_table = ftable_sine;
                if (voice->type == 1)      active_table = ftable_organ;
                else if (voice->type == 2) active_table = ftable_saw;

                // Grab and interpolate sample frames
                float sample_a = active_table[index_floor];
                float sample_b = active_table[index_ceil];
                sample = sample_a + fractional_part * (sample_b - sample_a);

                process_voice_adsr(voice, sample_rate);

                // Sum the dry synth signals together
                buffer[i] += sample * voice->target_volume * voice->current_gain * mix_gain;

                voice->phase += phase_increment;
                if (voice->phase >= 2.0f * PI) voice->phase -= 2.0f * PI;
            }
            voice = voice->next;
        }
    }


    // 4. MASTER EFFECTS RUN CONTINUOUSLY
    // Even if buffer is full of pure zeros (silence), the effects chain receives it,
    // mixes in any stored echoes from the tape memory loop, and passes it out.
    process_effects_chain(buffer, sample_count, sample_rate);

    // 5. Run standard garbage collection on finished voice nodes
    if (active_voices_head != NULL) {
        VoiceNode* current = active_voices_head;
        VoiceNode* previous = NULL;
        while (current != NULL) {
            if (current->stage == STAGE_FINISHED) {
                VoiceNode* to_delete = current;
                if (previous == NULL) {
                    active_voices_head = current->next;
                    current = active_voices_head;
                } else {
                    previous->next = current->next;
                    current = current->next;
                }
                free(to_delete);
            } else {
                previous = current;
                current = current->next;
            }
        }
    }
}
*/
EMSCRIPTEN_KEEPALIVE
void stream_mix_polyphonic(float* buffer, int sample_count, float sample_rate) {
    // 1. Always wipe and reset the outgoing buffer chunk first
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = 0.0f;
    }

    // 2. Count active non-finished voice threads
    int active_count = 0;
    VoiceNode* count_cursor = active_voices_head;
    while (count_cursor != NULL) {
        if (count_cursor->stage != STAGE_FINISHED) {
            active_count++;
        }
        count_cursor = count_cursor->next;
    }

    // REMOVED: "if (active_count == 0) return;" 
    // We let the execution fall through so the echo engine can process its tails!

    // 3. Only run oscillator accumulation loops IF voices are actually active
    if (active_count > 0) {
        float mix_gain = 1.0f / sqrtf((float)active_count);
        VoiceNode* voice = active_voices_head;

        while (voice != NULL) {
            if (voice->stage == STAGE_FINISHED) {
                voice = voice->next;
                continue;
            }

            // Calculate how many index steps to skip forward per sample frame
            float table_phase_increment = (voice->frequency * (float)FTABLE_SIZE) / sample_rate;

            for (int i = 0; i < sample_count; i++) {
                float sample = 0.0f;

                // 1. Directly interpret running phase as a linear table address pointer
                int index_floor = (int)voice->phase;
                int index_ceil = (index_floor + 1) % FTABLE_SIZE;
                float fractional_part = voice->phase - (float)index_floor;

                // 2. Route to target F-Table
                float* active_table = ftable_sine;
                if (voice->type == 1)      active_table = ftable_custom;
                else if (voice->type == 2) active_table = ftable_saw;

                // 3. Extract sample frames
                float sample_a = active_table[index_floor];
                float sample_b = active_table[index_ceil];
                sample = sample_a + fractional_part * (sample_b - sample_a);

                process_voice_adsr(voice, sample_rate);

                // Sum the dry synth signals together
                buffer[i] += sample * voice->target_volume * voice->current_gain * mix_gain;

                // 4. Update index positions using table length wraps
                voice->phase += table_phase_increment;
                while (voice->phase >= (float)FTABLE_SIZE) {
                    voice->phase -= (float)FTABLE_SIZE;
                }
            }
            voice = voice->next;
        }
    }


    // 4. MASTER EFFECTS RUN CONTINUOUSLY
    // Even if buffer is full of pure zeros (silence), the effects chain receives it,
    // mixes in any stored echoes from the tape memory loop, and passes it out.
    process_effects_chain(buffer, sample_count, sample_rate);

    // 5. Run standard garbage collection on finished voice nodes
    if (active_voices_head != NULL) {
        VoiceNode* current = active_voices_head;
        VoiceNode* previous = NULL;
        while (current != NULL) {
            if (current->stage == STAGE_FINISHED) {
                VoiceNode* to_delete = current;
                if (previous == NULL) {
                    active_voices_head = current->next;
                    current = active_voices_head;
                } else {
                    previous->next = current->next;
                    current = current->next;
                }
                free(to_delete);
            } else {
                previous = current;
                current = current->next;
            }
        }
    }
}



// Update your generate_wave function to use your new parameters
EMSCRIPTEN_KEEPALIVE
void generate_wave(int type, float* buffer, int sample_count, float frequency, float sample_rate, float volume) {
    VoiceNode* live_backup_head = active_voices_head;
    active_voices_head = NULL;

    note_on(999, type, frequency, volume);

    // Calculate when to release based on half the buffer size
    int release_sample_trigger = sample_count / 2;
    
    for (int i = 0; i < sample_count; i++) {
        if (i == release_sample_trigger) {
            note_off(999);
        }
        stream_mix_polyphonic(&buffer[i], 1, sample_rate);
    }

    VoiceNode* temp = active_voices_head;
    while (temp != NULL) {
        VoiceNode* next = temp->next;
        free(temp);
        temp = next;
    }
    active_voices_head = live_backup_head;
}

EMSCRIPTEN_KEEPALIVE
void init_wasm_synth_core() {
    // This safely invokes our tape delay buffer allocation 
    init_effects_pipeline(); 
    compile_function_tables(); 

}
