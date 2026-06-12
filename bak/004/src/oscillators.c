#include "oscillators.h"

float global_phase = 0.0f;

void update_phase(float phase_increment) {
    global_phase += phase_increment;
    if (global_phase >= 2.0f * PI) {
        global_phase -= 2.0f * PI;
    }
}

