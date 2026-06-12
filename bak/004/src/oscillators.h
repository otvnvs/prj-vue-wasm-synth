#ifndef OSCILLATORS_H
#define OSCILLATORS_H

#define PI 3.14159265358979323846f

// Shared global phase tracking across chunks
extern float global_phase;

// Utility to keep phase bounded between 0 and 2*PI
void update_phase(float phase_increment);

#endif

