/*
 * Mapping.c
 *
 *  Created on: Feb 7, 2026
 *      Author: Helena
 */

#include "Mapping.h"

// --- Variables globales ---
FilterParams filter_params;
LFOParams   lfo_params;
ADSRParams  adsr_params;

// --- Función de mapeo genérica ---
static float map_param(float norm, float min, float max, ParamType type)
{
    switch(type)
    {
        case PARAM_FREQ:
        case PARAM_TIME:
            // Escala log/exponencial para frecuencia y tiempos (más natural al oído)
            return min * powf(max / min, norm);
        case PARAM_GAIN:
        case PARAM_LFO_DEPTH:
            // Escala lineal para amplitud y LFO depth
            return min + (max - min) * norm;
        case PARAM_Q:
            // Escala lineal para Q
            return min + (max - min) * norm;
        default:
            return norm;
    }
}

// --- Inicialización de parámetros ---
void Parameter_Init(void)
{
    // Valores iniciales (puedes cambiar según tus necesidades)
    filter_params.cutoff    = 1000.0f;
    filter_params.resonance = 1.0f;

    lfo_params.rate  = 1.0f;
    lfo_params.depth = 0.5f;

    adsr_params.attack  = 0.01f;
    adsr_params.decay   = 0.1f;
    adsr_params.sustain = 0.8f;
    adsr_params.release = 0.5f;
}

// --- Actualización de parámetros desde controles ---
void Parameter_Update(void)
{
    // Filtro
    filter_params.cutoff    = map_param(ctrl_norm[CTRL_CUTOFF],
                                        20.0f, 20000.0f, PARAM_FREQ);
    filter_params.resonance = map_param(ctrl_norm[CTRL_RESONANCE],
                                        0.5f, 20.0f, PARAM_FREQ);   // exponencial

    // LFO
    lfo_params.rate  = map_param(ctrl_norm[CTRL_RATE],
                                 0.05f, 20.0f, PARAM_FREQ);         // rango ampliado
    lfo_params.depth = map_param(ctrl_norm[CTRL_DEPTH],
                                 0.0f, 1.0f,   PARAM_LFO_DEPTH);

    // ADSR
    adsr_params.attack  = map_param(ctrl_norm[CTRL_ATTACK],
                                    0.001f, 5.0f, PARAM_TIME);      // desde 1ms
    adsr_params.decay   = map_param(ctrl_norm[CTRL_DECAY],
                                    0.001f, 5.0f, PARAM_TIME);      // desde 1ms
    adsr_params.sustain = map_param(ctrl_norm[CTRL_SUSTAIN],
                                    0.0001f,   1.0f, PARAM_GAIN);      // sin cambio
    adsr_params.release = map_param(ctrl_norm[CTRL_RELEASE],
                                    0.01f,  8.0f, PARAM_TIME);      // hasta 8s
}

