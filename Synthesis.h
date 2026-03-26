/*
 * Synth.h
 *
 *  Created on: Feb 8, 2026
 *      Author: Helena
 */

#ifndef INC_SYNTH_H_
#define INC_SYNTH_H_

#include <stdint.h>

// Número máximo de voces
#define MAX_VOICES       8
#define F_SAMPLE         48000.0f
#define FADE_IN_SAMPLES  48
#define EVENT_QUEUE_SIZE 16
#define SINE_TABLE_SIZE  1024

// ─── Enums ───────────────────────────────────────────────────────────────────
typedef enum {
    WAVE_SINE = 0,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_SAW
} WaveType;

typedef enum {
    ENV_IDLE = 0,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} EnvState;

// ─── Structs ─────────────────────────────────────────────────────────────────
typedef struct {
    uint8_t  note;
    uint8_t  active;
    float    phase;
    float    dPhase;
    float    env;
    EnvState envState;
    float    attackCoef;
    float    decayCoef;
    float    sustainLevel;
    float    releaseCoef;
    float    z1, z2;
    float    b0, b1, b2;
    float    a1, a2;
    float    cutoff_smooth;
    float    last_cutoff;
    float    last_resonance;
    uint32_t releaseGuard;
    float    fadeIn;
} Voice;

typedef struct {
    float phase;
    float value;
} LFOState;

typedef struct {
    uint8_t active;
    uint8_t note;
    float   dPhase;
    uint8_t isNoteOff;
} VoiceEvent;

// ─── Variables globales ───────────────────────────────────────────────────────
extern Voice    voices[MAX_VOICES];
extern LFOState lfo_state;
extern float    sine_table[SINE_TABLE_SIZE];

extern volatile VoiceEvent event_queue[EVENT_QUEUE_SIZE];
extern volatile uint8_t    event_queue_head;
extern volatile uint8_t    event_queue_tail;

// ─── Funciones públicas ───────────────────────────────────────────────────────
void   Synth_Init(void);
void   Synth_SetWaveType(WaveType type);
void   Synth_NoteOn(Voice *v, uint8_t note, float dPhase);
void   Synth_NoteOff(Voice *v);
void   Synth_UpdateActiveVoices(void);
void   Synth_ProcessEventQueue(void);
float  Synth_RenderVoiceSample(Voice *v);
Voice* StealVoice(void);

float Oscillator_Generate(Voice *v);
float ADSR_Apply(Voice *v, float sample);
float Filter_Apply(Voice *v, float sample);
void  LFO_Tick(void);

#endif /* INC_SYNTH_H_ */
