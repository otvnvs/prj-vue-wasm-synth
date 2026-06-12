#include "ftable.h"
#include <math.h>

float ftable_sine[FTABLE_SIZE];
float ftable_custom[FTABLE_SIZE]; // Our programmable target memory slice
float ftable_saw[FTABLE_SIZE];

// The core compiler remains exactly the same as your previous step...
void compile_harmonic_table(float* table, float* harmonic_weights, int harmonic_count) {
    for (int i = 0; i < FTABLE_SIZE; i++) table[i] = 0.0f;

    for (int i = 0; i < FTABLE_SIZE; i++) {
        float phase = (float)i / (float)FTABLE_SIZE;
        for (int h = 0; h < harmonic_count; h++) {
            float weight = harmonic_weights[h];
            if (weight == 0.0f) continue;
            int harmonic_number = h + 1;
            table[i] += sinf(2.0f * PI * (float)harmonic_number * phase) * weight;
        }
    }

    float max_val = 0.0f;
    for (int i = 0; i < FTABLE_SIZE; i++) {
        float abs_val = fabsf(table[i]);
        if (abs_val > max_val) max_val = abs_val;
    }
    if (max_val > 0.0f) {
        for (int i = 0; i < FTABLE_SIZE; i++) table[i] /= max_val;
    }
}

// Set up base defaults on boot
void compile_function_tables() {
    float sine_weights[1] = { 1.0f };
    compile_harmonic_table(ftable_sine, sine_weights, 1);

    // Initial default layout for our custom table (a warm triangle-like shape)
    float initial_custom_weights[4] = { 1.0f, 0.0f, 0.33f, 0.0f };
    compile_harmonic_table(ftable_custom, initial_custom_weights, 4);

    float saw_weights[15];
    for (int h = 0; h < 15; h++) saw_weights[h] = 1.0f / (float)(h + 1);
    compile_harmonic_table(ftable_saw, saw_weights, 15);
}

// NEW: Triggered by JavaScript to program the table dynamically
void set_custom_harmonics(float* weights, int count) {
    compile_harmonic_table(ftable_custom, weights, count);
}

