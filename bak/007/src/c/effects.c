#include "effects.h"
#include <math.h>
#include <stdlib.h>

// Allocate global variables
float tremolo_speed = 4.0f;  // Default 4Hz modulation speed
float tremolo_depth = 0.5f;  // Default 50% modulation intensity
int tremolo_active = 0;      // Default: Bypassed
static float tremolo_lfo_phase = 0.0f;

int echo_active = 0;
float echo_time = 0.3f;       // Default 300ms delay time
float echo_feedback = 0.4f;   // Default 40% repeat regeneration feedback
float echo_mix = 0.35f;       // Default 35% wet echo signal level

// Ring buffer capacity configuration (2 seconds max at 48kHz sample rate)
#define MAX_DELAY_SAMPLES 96000
static float* echo_ring_buffer = NULL;
static int write_ptr = 0;
static float current_smoothed_delay = 0.0f;

// --- NEW: Distortion Coefficients ---
int dist_active = 0;
float dist_drive = 5.0f;       // Multiplies incoming signals to hit clipping limits
float dist_blend = 0.4f;       // Amount of distorted signal mixed into output

// --- NEW: Chorus Modulator Allocations ---
int chorus_active = 0;
float chorus_rate = 1.0f;      // 1 Hz LFO Speed
float chorus_depth = 0.5f;     // Modulates delay depth limits
static float chorus_lfo_phase = 0.0f;
#define CHORUS_RING_SIZE 4096  // Max 85ms buffer at 48kHz (more than enough for 5ms-15ms chorus delay)
static float* chorus_ring_buffer = NULL;
static int chorus_write_ptr = 0;


/*
void init_effects_pipeline() {
    tremolo_lfo_phase = 0.0f;
    
    // Allocate the large audio ring memory heap once on startup
    if (echo_ring_buffer == NULL) {
        echo_ring_buffer = (float*)calloc(MAX_DELAY_SAMPLES, sizeof(float));
    }
    write_ptr = 0;
    current_smoothed_delay = 0.0f;
}
*/
void init_effects_pipeline() {
    tremolo_lfo_phase = 0.0f;
    chorus_lfo_phase = 0.0f;
    chorus_write_ptr = 0;
    write_ptr = 0;
    current_smoothed_delay = 0.0f;

    if (echo_ring_buffer == NULL)  echo_ring_buffer = (float*)calloc(MAX_DELAY_SAMPLES, sizeof(float));
    if (chorus_ring_buffer == NULL) chorus_ring_buffer = (float*)calloc(CHORUS_RING_SIZE, sizeof(float));
}


// 1. The Tremolo Effect Worker Loop
void apply_tremolo_effect(float* buffer, int sample_count, float sample_rate) {
    if (!tremolo_active) return;

    // Calculate how much the LFO phase moves forward per single sample frame
    float phase_increment = (2.0f * PI * tremolo_speed) / sample_rate;

    for (int i = 0; i < sample_count; i++) {
        // Compute LFO oscillator position mapped from -1.0->1.0 to 0.0->1.0
        float lfo_sin = sinf(tremolo_lfo_phase);
        float normalized_lfo = (lfo_sin + 1.0f) * 0.5f;

        // Calculate the target volume attenuation multiplier based on effect depth
        float gain_modulator = 1.0f - (tremolo_depth * normalized_lfo);

        // Modify the polyphonic audio sample frame inline (destructive mix)
        buffer[i] *= gain_modulator;

        // Increment the LFO phase tracking independently
        tremolo_lfo_phase += phase_increment;
        if (tremolo_lfo_phase >= 2.0f * PI) {
            tremolo_lfo_phase -= 2.0f * PI;
        }
    }
}

// Tape Echo DSP Algorithm Processor
void apply_echo_effect(float* buffer, int sample_count, float sample_rate) {
    if (echo_ring_buffer == NULL) return;
    
    // Fallback if effect is entirely bypassed: we still record dry audio into the ring
    // so that when you toggle it on, it already has an active tail history.
    float feedback_scalar = echo_active ? echo_feedback : 0.0f;
    float mix_scalar = echo_active ? echo_mix : 0.0f;

    for (int i = 0; i < sample_count; i++) {
        float dry_sample = buffer[i];

        // 1. Calculate the ideal target sample offset based on desired seconds
        float target_delay_samples = echo_time * sample_rate;
        if (target_delay_samples >= MAX_DELAY_SAMPLES - 1) {
            target_delay_samples = MAX_DELAY_SAMPLES - 2;
        }

        // Initialize smoothed tracking value on the first pass
        if (current_smoothed_delay == 0.0f) {
            current_smoothed_delay = target_delay_samples;
        }

        // 2. Real-time Tape Glide Smoothing Math:
        // Instead of snapping instantly to a new delay time (which causes clicks),
        // we slowly glide toward it by a tiny fraction (0.01%) every sample frame.
        // This stretching of the virtual tape creates an authentic pitch-bending warp.
        current_smoothed_delay += (target_delay_samples - current_smoothed_delay) * 0.001f;

        // 3. Compute fractional playback index pointers using wrapped circular constraints
        float read_ptr_exact = (float)write_ptr - current_smoothed_delay;
        while (read_ptr_exact < 0.0f) read_ptr_exact += (float)MAX_DELAY_SAMPLES;

        // 4. Perform Linear Interpolation to extract a pristine fractional sample
        int index_left = (int)read_ptr_exact;
        int index_right = (index_left + 1) % MAX_DELAY_SAMPLES;
        float fractional_part = read_ptr_exact - (float)index_left;

        float sample_left = echo_ring_buffer[index_left];
        float sample_right = echo_ring_buffer[index_right];
        
        // The interpolated delayed sound value (WET sample)
        float wet_sample = sample_left + fractional_part * (sample_right - sample_left);

        // 5. Destructive Inline Combination:
        // Combine dry and wet audio on the active output tracking channel buffer
        buffer[i] = (dry_sample * (1.0f - mix_scalar)) + (wet_sample * mix_scalar);


        // 6. Write back into tape memory loop combining feedback decay structures
        echo_ring_buffer[write_ptr] = dry_sample + (wet_sample * feedback_scalar);

        // Advance circular index
        write_ptr = (write_ptr + 1) % MAX_DELAY_SAMPLES;
    }
}


// Effect Node 3: Non-Linear Hard/Soft Distortion Clipper
void apply_distortion_effect(float* buffer, int sample_count) {
    if (!dist_active) return;

    for (int i = 0; i < sample_count; i++) {
        float dry = buffer[i];
        
        // Boost input signal via Drive scalar
        float driven = dry * dist_drive;

        // Apply a smooth non-linear hyperbolic tangent (atanf) distortion shape curve.
        // This rounds off peaks musically, resembling tube saturation.
        float wet = atanf(driven) / (PI / 2.0f); // Normalizes output level peaks back to roughly 1.0

        // Mix dry signal with wet saturated audio profile
        buffer[i] = (dry * (1.0f - dist_blend)) + (wet * dist_blend);
    }
}

// Effect Node 4: Vintage Analog Chorus Modulator
// Modulates a very short 10ms delay line sample-by-sample using a slow sine LFO [INDEX]
void apply_chorus_effect(float* buffer, int sample_count, float sample_rate) {
    if (chorus_ring_buffer == NULL) return;

    float mix_scalar = chorus_active ? 0.45f : 0.0f; // Standard 45% wet blend for chorus cancellation phasing
    float phase_inc = (2.0f * PI * chorus_rate) / sample_rate;

    for (int i = 0; i < sample_count; i++) {
        float dry = buffer[i];

        // Store the incoming current audio sample frame inside the short chorus memory loop
        chorus_ring_buffer[chorus_write_ptr] = dry;

        // Compute LFO position normalized to 0.0 -> 1.0
        float lfo = (sinf(chorus_lfo_phase) + 1.0f) * 0.5f;

        // Map LFO onto a 5ms to 25ms changing delay window frame range
        float delay_ms = 5.0f + (lfo * chorus_depth * 20.0f);
        float delay_samples = (delay_ms / 1000.0f) * sample_rate;

        // Read pointer index calculation
        float r_ptr = (float)chorus_write_ptr - delay_samples;
        while (r_ptr < 0.0f) r_ptr += (float)CHORUS_RING_SIZE;

        // Precise Sample-Accurate Linear Interpolation [INDEX]
        int idx_floor = (int)r_ptr;
        int idx_ceil = (idx_floor + 1) % CHORUS_RING_SIZE;
        float frac = r_ptr - (float)idx_floor;

        float wet = chorus_ring_buffer[idx_floor] + frac * (chorus_ring_buffer[idx_ceil] - chorus_ring_buffer[idx_floor]);

        // Destructively mix chorused voice data back onto master array track
        buffer[i] = (dry * (1.0f - mix_scalar)) + (wet * mix_scalar);

        // Advance indices
        chorus_write_ptr = (chorus_write_ptr + 1) % CHORUS_RING_SIZE;
        chorus_lfo_phase += phase_inc;
        if (chorus_lfo_phase >= 2.0f * PI) chorus_lfo_phase -= 2.0f * PI;
    }
}

// 2. The Central Effects Chaining Router
// To expand your engine later with Echo or Reverb, you simply append their 
// processing loops right underneath tremolo inside this function.
// Master Chaining Router Execution Sequential Pipeline
void process_effects_chain(float* buffer, int sample_count, float sample_rate) {
    apply_tremolo_effect(buffer, sample_count, sample_rate);
    apply_distortion_effect(buffer, sample_count);          // Layer 2: Saturation
    apply_chorus_effect(buffer, sample_count, sample_rate);  // Layer 3: Spatial Modulation
    apply_echo_effect(buffer, sample_count, sample_rate);    // Layer 4: Tape Delay repeats

}


