#include <emscripten.h>
#include <math.h>
#include <stdio.h>

#define PI 3.14159265358979323846
#ifdef __cplusplus
extern "C" {
#endif


// Generates a sine wave buffer
EMSCRIPTEN_KEEPALIVE
void generate_sine_wave(float* buffer, int sample_count, float frequency, float sample_rate) {
	printf("generate_sine_wave:begin\n");
    for (int i = 0; i < sample_count; i++) {
        buffer[i] = sinf(2.0f * PI * frequency * ((float)i / sample_rate));
    }
	printf("generate_sine_wave:end\n");
}

// Generates a square wave buffer
EMSCRIPTEN_KEEPALIVE
void generate_square_wave(float* buffer, int sample_count, float frequency, float sample_rate) {
	printf("generate_square_wave:begin\n");
    for (int i = 0; i < sample_count; i++) {
        float value = sinf(2.0f * PI * frequency * ((float)i / sample_rate));
        buffer[i] = (value >= 0.0f) ? 1.0f : -1.0f;
    }
	printf("generate_square_wave:end\n");
}

#ifdef __cplusplus
}
#endif
