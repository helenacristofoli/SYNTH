/*
 * Mapping.c
 *
 *  Created on: Feb 7, 2026
 *      Author: Helena
 *
 *  Mapeo de parámetros y máquina de estados (onda + LFO target)
 */

#include "Mapping.h"
#include "controls.h"
#include "Synthesis.h"

/* ================================================================
 * MAPEO DE PARÁMETROS
 * ================================================================ */
FilterParams filter_params;
LFOParams    lfo_params;
ADSRParams   adsr_params;

static float map_param(float norm, float min, float max, ParamType type)
{
    switch(type)
    {
        case PARAM_FREQ:
        case PARAM_TIME:
            return min * powf(max / min, norm);
        case PARAM_GAIN:
        case PARAM_LFO_DEPTH:
            return min + (max - min) * norm;
        case PARAM_Q:
            return min + (max - min) * norm;
        default:
            return norm;
    }
}

void Parameter_Update(void)
{
    filter_params.cutoff    = map_param(ctrl_norm[CTRL_CUTOFF],
                                        20.0f, 20000.0f, PARAM_FREQ);
    filter_params.resonance = map_param(ctrl_norm[CTRL_RESONANCE],
                                        0.5f, 20.0f, PARAM_FREQ);
    lfo_params.rate         = map_param(ctrl_norm[CTRL_RATE],
                                        0.05f, 20.0f, PARAM_FREQ);
    lfo_params.depth        = map_param(ctrl_norm[CTRL_DEPTH],
                                        0.0f, 1.0f, PARAM_LFO_DEPTH);
    adsr_params.attack      = map_param(ctrl_norm[CTRL_ATTACK],
                                        0.001f, 2.0f, PARAM_TIME);
    adsr_params.decay       = map_param(ctrl_norm[CTRL_DECAY],
                                        0.001f, 2.0f, PARAM_TIME);
    adsr_params.sustain     = map_param(ctrl_norm[CTRL_SUSTAIN],
                                        0.0001f, 1.0f, PARAM_GAIN);
    adsr_params.release     = map_param(ctrl_norm[CTRL_RELEASE],
                                        0.01f, 3.0f, PARAM_TIME);
}



// Tabla de máscaras LED por tipo de onda
static const uint8_t wave_led_mask[4] = {
    0x02,   // WAVE_SINE     → QB
    0x01,   // WAVE_SQUARE   → QA
    0x08,   // WAVE_TRIANGLE → QD
    0x04,   // WAVE_SAW      → QC
};

// Tabla de máscaras LED por LFO target
static const uint8_t lfo_led_mask[LFO_COUNT] = {
    0x10,   // LFO_AMPLITUDE → QE
    0x20,   // LFO_PITCH     → QF
    0x40,   // LFO_CUTOFF    → QG
    0x80,   // LFO_RESONANCE → QH
};

static const char* wave_names[4] = {
    "Sine", "Square", "Triangle", "Saw"
};

static const char* lfo_names[LFO_COUNT] = {
    "Amplitude", "Pitch", "Cutoff", "Resonance"
};

// Estado inicial
SynthState synth_state = {
    .wave = WAVE_SINE,
    .lfo  = LFO_AMPLITUDE
};

void StateManager_Init(void)
{
    synth_state.wave = WAVE_SINE;
    synth_state.lfo  = LFO_AMPLITUDE;
    Synth_SetWaveType(synth_state.wave);
    ShiftReg_Write(StateManager_GetLEDByte());
}

void StateManager_NextWave(void)
{
    synth_state.wave = (WaveType)((synth_state.wave + 1) % 4);
    Synth_SetWaveType(synth_state.wave);
    ShiftReg_Write(StateManager_GetLEDByte());
}

void StateManager_NextLFO(void)
{
    synth_state.lfo = (LFOTarget)((synth_state.lfo + 1) % LFO_COUNT);
    ShiftReg_Write(StateManager_GetLEDByte());
}

uint8_t StateManager_GetLEDByte(void)
{
    return wave_led_mask[synth_state.wave] | lfo_led_mask[synth_state.lfo];
}

const char* StateManager_GetWaveName(void)
{
    return wave_names[synth_state.wave];
}

const char* StateManager_GetLFOName(void)
{
    return lfo_names[synth_state.lfo];
}

/* ================================================================
 * INICIALIZACIÓN GENERAL
 * ================================================================ */
void Parameter_Init(void)
{
    // Valores por defecto de parámetros
    filter_params.cutoff    = 1000.0f;
    filter_params.resonance = 1.0f;
    lfo_params.rate         = 1.0f;
    lfo_params.depth        = 0.5f;
    adsr_params.attack      = 0.01f;
    adsr_params.decay       = 0.1f;
    adsr_params.sustain     = 0.8f;
    adsr_params.release     = 0.5f;

    // Inicializar estado de onda y LEDs
    StateManager_Init();
}
