#include <emscripten.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>//for printf

#define PI 3.14159265358979323846f

// 1. Definition of Envelope Stages
typedef enum {
    STAGE_ATTACK,
    STAGE_DECAY,
    STAGE_SUSTAIN,
    STAGE_RELEASE,
    STAGE_FINISHED
} EnvelopeStage;

// 2. Definition of an independent dynamic Voice node with ADSR Tracking
typedef struct VoiceNode {
    int key_id;             // Musical key identification
    int type;               // 0=sine, 1=square, 2=sawtooth
    float frequency;
    float phase;
    float target_volume;    // Base user-requested volume
    
    // ADSR State variables
    EnvelopeStage stage;
    float current_gain;     // Actively computed multiplier (0.0 to 1.0)
    float release_start_gain; // The gain value exactly when note_off was called
    float stage_time;       // Accumulated time spent inside the current stage (in seconds)

    struct VoiceNode* next;
} VoiceNode;

// Head pointer for active voices
static VoiceNode* active_voices_head = NULL;

// ADSR Timing Settings (in seconds)
static const float ATTACK_TIME  = 0.05f; // Quick fade in (50ms)
static const float DECAY_TIME   = 0.15f; // Drop down to sustain level (150ms)
static const float SUSTAIN_LEVEL = 0.60f; // Hold note at 60% amplitude
static const float RELEASE_TIME = 0.40f; // Smooth trailing tail out (400ms)

// Helper to advance individual voice envelope states sample-by-sample
void process_voice_adsr(VoiceNode* voice, float sample_rate) {
    //printf("void process_voice_adsr(VoiceNode* voice, float sample_rate)\n");
    float dt = 1.0f / sample_rate; // Time elapsed per sample frame
    voice->stage_time += dt;

    switch (voice->stage) {
        case STAGE_ATTACK:
            // Linearly ramp up to 1.0 peak amplitude
            voice->current_gain = voice->stage_time / ATTACK_TIME;
            if (voice->stage_time >= ATTACK_TIME) {
                voice->stage = STAGE_DECAY;
                voice->stage_time = 0.0f;
                voice->current_gain = 1.0f;
            }
            break;

        case STAGE_DECAY:
            // Linearly ramp down from 1.0 to our SUSTAIN_LEVEL target
            voice->current_gain = 1.0f - ((1.0f - SUSTAIN_LEVEL) * (voice->stage_time / DECAY_TIME));
            if (voice->stage_time >= DECAY_TIME) {
                voice->stage = STAGE_SUSTAIN;
                voice->stage_time = 0.0f;
                voice->current_gain = SUSTAIN_LEVEL;
            }
            break;

        case STAGE_SUSTAIN:
            // Stand firm at fixed volume level indefinitely until note_off is tripped
            voice->current_gain = SUSTAIN_LEVEL;
            break;

        case STAGE_RELEASE:
            // Linearly decline from whatever volume the note was at down to 0.0 absolute mute
            voice->current_gain = voice->release_start_gain * (1.0f - (voice->stage_time / RELEASE_TIME));
            if (voice->stage_time >= RELEASE_TIME || voice->current_gain <= 0.0f) {
                voice->stage = STAGE_FINISHED;
                voice->current_gain = 0.0f;
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

// 6. Feature 1 Backward Compatibility Layer
EMSCRIPTEN_KEEPALIVE
void generate_wave(int type, float* buffer, int sample_count, float frequency, float sample_rate, float volume) {
    //printf("void generate_wave(int type, float* buffer, int sample_count, float frequency, float sample_rate, float volume)\n");
    VoiceNode* live_backup_head = active_voices_head;
    active_voices_head = NULL;

    // Trigger note_on which hooks up our Attack, Decay, Sustain states
    note_on(999, type, frequency, volume);

    // Because a static buffer note represents a snapshot, we run the mixer continuously.
    // To simulate a complete natural key strike cycle in a 2-second buffer:
    // We let it play under sustain for 1.5 seconds, then fire note_off to render the 0.4-second release tail.
    int release_sample_trigger = (int)(sample_rate * 1.5f);

    float phase_increment = (2.0f * PI * frequency) / sample_rate;
    
    // Evaluate sample by sample
    for (int i = 0; i < sample_count; i++) {
        if (i == release_sample_trigger) {
            note_off(999);
        }
        
        // Emulate streaming blocks of 1 frame length via our master polyphonic mixer engine
        stream_mix_polyphonic(&buffer[i], 1, sample_rate);
    }

    // Purge lingering structures
    VoiceNode* temp = active_voices_head;
    while (temp != NULL) {
        VoiceNode* next = temp->next;
        free(temp);
        temp = next;
    }

    active_voices_head = live_backup_head;
}

