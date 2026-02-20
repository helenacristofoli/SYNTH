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
#define MAX_VOICES 8
#define F_SAMPLE   48000.0f

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



typedef struct {
    uint8_t  note;
    uint8_t  active;

    // Oscilador
    float phase;
    float dPhase;

    // Envelope
    float    env;
    EnvState envState;
    float    attackCoef;
    float    decayCoef;
    float    sustainLevel;
    float    releaseCoef;

    // Filtro biquad
    float z1, z2;           // variables de estado
    float b0, b1, b2;       // coeficientes feedforward
    float a1, a2;           // coeficientes feedback

} Voice;

// Estado global del LFO
typedef struct {
    float phase;       // 0.0 → 1.0
    float value;       // salida actual -1.0 → 1.0
} LFOState;

extern LFOState lfo_state;

// Variables globales accesibles externamente
extern Voice voices[MAX_VOICES];





// Funciones públicas
void  Synth_Init(void);
void  Synth_SetWaveType(WaveType type);
void  Synth_NoteOn(Voice *v, uint8_t note, float dPhase);
void  Synth_NoteOff(Voice *v);
void  Synth_UpdateActiveVoices(void);
float Synth_RenderVoiceSample(Voice *v);

// Funciones de bloques internos de síntesis
float Oscillator_Generate(Voice *v);
float ADSR_Apply(Voice *v, float sample);
float Filter_Apply(Voice *v, float sample);
void LFO_Tick(void);

#endif /* INC_SYNTH_H_ */
