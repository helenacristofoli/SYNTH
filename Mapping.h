/*
 * Mapping.h
 *
 *  Created on: Feb 7, 2026
 *      Author: Helena
 */

#ifndef INC_MAPPING_H_
#define INC_MAPPING_H_

#include "controls.h"
#include <math.h>

// --- Tipos de parámetro para mapeo ---
typedef enum {
    PARAM_FREQ,       // Hz
    PARAM_GAIN,       // 0..1
    PARAM_TIME,       // segundos
    PARAM_Q,          // Resonancia
    PARAM_LFO_DEPTH   // 0..1
} ParamType;

// --- Structs por bloque ---
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

// --- Variables globales de parámetros ---
extern FilterParams filter_params;
extern LFOParams   lfo_params;
extern ADSRParams  adsr_params;

// --- Funciones ---
void Parameter_Init(void);
void Parameter_Update(void);


#endif /* INC_MAPPING_H_ */
