#ifndef FTABLE_H
#define FTABLE_H

#define FTABLE_SIZE 2048
#define PI 3.14159265358979323846f

extern float ftable_sine[FTABLE_SIZE];
extern float ftable_custom[FTABLE_SIZE]; // CHANGED: Renamed from ftable_organ to ftable_custom
extern float ftable_saw[FTABLE_SIZE];

void compile_function_tables();
void compile_harmonic_table(float* table, float* harmonic_weights, int harmonic_count);

// NEW: Public API entry point to rewrite the table on the fly
void set_custom_harmonics(float* weights, int count);

#endif

