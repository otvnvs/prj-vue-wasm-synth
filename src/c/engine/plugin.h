#ifndef PLUGIN_H
#define PLUGIN_H

#define EXPORT __attribute__((visibility("default")))
#define SAMPLE_RATE_DEFAULT 44100.0f
#define PI_CONSTANT 3.14159265358979323846f

typedef void (*AudioProcessCallback)(void* instance_state, float* input, float* output, int sample_count, float sample_rate);

/*
 * unified json schematic discovery
 * describes the plugin's configuration profile
 */
EXPORT const char* get_plugin_manifest();

#endif

