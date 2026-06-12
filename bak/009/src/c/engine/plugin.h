// src/c/plugin.h
#ifndef PLUGIN_H
#define PLUGIN_H

#define EXPORT __attribute__((visibility("default")))
#define SAMPLE_RATE_DEFAULT 44100.0f
#define PI_CONSTANT 3.14159265358979323846f

/**
 * The standard layout signature for every audio processor node.
 * It reads from an arbitrary input buffer and writes directly 
 * into an arbitrary output buffer.
 */
typedef void (*AudioProcessCallback)(void* instance_state, float* input, float* output, int sample_count, float sample_rate);

#endif

