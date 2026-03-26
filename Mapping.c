#include "Mapping.h"
#include "controls.h"
#include "Synthesis.h"

FilterParams filter_params;
LFOParams    lfo_params;
ADSRParams   adsr_params;

float cached_attackCoef  = 0.0f;
float cached_decayCoef   = 0.0f;
float cached_releaseCoef = 0.0f;

float cached_b0 = 0.0f;
float cached_b1 = 0.0f;
float cached_b2 = 0.0f;
float cached_a1 = 0.0f;
float cached_a2 = 0.0f;

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

static void recalc_filter_coefs(float cutoff, float Q)
{
    if (cutoff < 20.0f)    cutoff = 20.0f;
    if (cutoff > 20000.0f) cutoff = 20000.0f;
    if (Q < 0.5f)          Q = 0.5f;
    if (Q > 20.0f)         Q = 20.0f;

    float w0    = 2.0f * 3.14159f * cutoff / F_SAMPLE;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float alpha = sinw0 / (2.0f * Q);
    float b0    = (1.0f - cosw0) / 2.0f;
    float b1    =  1.0f - cosw0;
    float b2    = (1.0f - cosw0) / 2.0f;
    float a0    =  1.0f + alpha;
    float a1    = -2.0f * cosw0;
    float a2    =  1.0f - alpha;

    cached_b0 = b0 / a0;
    cached_b1 = b1 / a0;
    cached_b2 = b2 / a0;
    cached_a1 = a1 / a0;
    cached_a2 = a2 / a0;
}

void Parameter_ForceUpdate(void)
{
    cached_attackCoef  = 1.0f - powf(0.0001f,
                             1.0f / (adsr_params.attack  * F_SAMPLE));
    cached_decayCoef   = powf(0.0001f,
                             1.0f / (adsr_params.decay   * F_SAMPLE));
    cached_releaseCoef = powf(0.0001f,
                             1.0f / (adsr_params.release * F_SAMPLE));

    recalc_filter_coefs(filter_params.cutoff, filter_params.resonance);
}

void Parameter_Update(void)
{
    static float prev_norm[CTRL_COUNT] = {0};
    uint8_t changed = 0;

    for (int i = 0; i < CTRL_COUNT; i++)
    {
        float diff = ctrl_norm[i] - prev_norm[i];
        if (diff > 0.005f || diff < -0.005f)
        {
            prev_norm[i] = ctrl_norm[i];
            changed = 1;
        }
    }
    if (!changed) return;

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

    Parameter_ForceUpdate();
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

void Parameter_Init(void)
{
    filter_params.cutoff    = 1000.0f;
    filter_params.resonance = 1.0f;
    lfo_params.rate         = 1.0f;
    lfo_params.depth        = 0.5f;
    adsr_params.attack      = 0.01f;
    adsr_params.decay       = 0.1f;
    adsr_params.sustain     = 0.8f;
    adsr_params.release     = 0.5f;

    StateManager_Init();

    // Garantiza que los cached estén listos antes de Synth_Init()
    Parameter_ForceUpdate();
}
