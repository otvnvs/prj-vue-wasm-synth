#ifndef EFFECTS_H
#define EFFECTS_H

#define PI 3.14159265358979323846f

// Tremolo
extern float tremolo_speed;  // Frequency of modulation (Hz)
extern float tremolo_depth;  // Strength of modulation (0.0 to 1.0)
extern int tremolo_active;   // Toggle switch (0 = Bypass, 1 = On)

// Tape Echo
extern int echo_active;       // Toggle switch (0 = Bypass, 1 = Active)
extern float echo_time;       // Delay time in seconds (e.g. 0.3s)
extern float echo_feedback;   // Repeat decay strength (0.0 to 0.95)
extern float echo_mix;        // Wet/Dry mix balancing scalar (0.0 to 1.0)

extern int dist_active;       // 0 = Bypass, 1 = Active
extern float dist_drive;      // Input amplification (1.0 to 20.0)
extern float dist_blend;      // Wet/Dry mix ratio (0.0 to 1.0)

extern int chorus_active;
extern float chorus_rate;
extern float chorus_depth;


// Initialise global effect LFO state tracking variables
void init_effects_pipeline();

// The master chain wrapper that receives the raw polyphonic mix 
// and sequentially modifies the sample frames inline
void process_effects_chain(float* buffer, int sample_count, float sample_rate);

void set_echo_parameters(int active, float time, float feedback, float mix);


#endif

