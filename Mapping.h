/*
 * Mapping.h
 *
 *  Created on: Feb 7, 2026
 *      Author: Helena
 *
 *  Mapeo de parámetros y máquina de estados (onda + LFO target)
 */

#ifndef INC_MAPPING_H_
#define INC_MAPPING_H_

#include "controls.h"
#include "Synthesis.h"
#include <math.h>

/* ================================================================
 * MAPEO DE PARÁMETROS
 * ================================================================ */
typedef enum {
    PARAM_FREQ,
    PARAM_GAIN,
    PARAM_TIME,
    PARAM_Q,
    PARAM_LFO_DEPTH
} ParamType;

typedef struct {
    float cutoff;
    float resonance;
} FilterParams;

typedef struct {
    float rate;
    float depth;
} LFOParams;

typedef struct {
    float attack;
    float decay;
    float sustain;
    float release;
} ADSRParams;

extern FilterParams filter_params;
extern LFOParams    lfo_params;
extern ADSRParams   adsr_params;

/* ================================================================
 * STATE MANAGER — Tipo de onda y parámetro objetivo del LFO
 * ================================================================ */
typedef enum {
    LFO_CUTOFF = 0,
    LFO_RESONANCE,
    LFO_AMPLITUDE,
    LFO_PITCH,
    LFO_COUNT
} LFOTarget;

typedef struct {
    WaveType  wave;
    LFOTarget lfo;
} SynthState;

extern SynthState synth_state;

/* ================================================================
 * Funciones públicas
 * ================================================================ */

// Mapeo de parámetros
void Parameter_Init(void);
void Parameter_Update(void);

// State manager
void           StateManager_Init(void);
void           StateManager_NextWave(void);
void           StateManager_NextLFO(void);
uint8_t        StateManager_GetLEDByte(void);
const char*    StateManager_GetWaveName(void);
const char*    StateManager_GetLFOName(void);

#endif /* INC_MAPPING_H_ */
