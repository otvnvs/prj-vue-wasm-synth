// src/c/engine/plugin.h
#ifndef PLUGIN_H
#define PLUGIN_H

#define EXPORT __attribute__((visibility("default")))
#define SAMPLE_RATE_DEFAULT 44100.0f
#define PI_CONSTANT 3.14159265358979323846f

typedef void (*AudioProcessCallback)(void* instance_state, float* input, float* output, int sample_count, float sample_rate);

// --- NEW UNIFIED JSON SCHEMATIC DISCOVERY CONTRACT ---
/**
 * Returns a direct read-only memory pointer address to a JSON string literal 
 * that completely describes the plugin's configuration profile.
 */
EXPORT const char* get_plugin_manifest();

#endif

