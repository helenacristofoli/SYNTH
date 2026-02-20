/*
 * Synth.c
 *
 *  Created on: Feb 8, 2026
 *      Author: Helena
 */

#include "Synthesis.h"
#include "Mapping.h"
#include <math.h>

//defines
#define PI 3.14159f
#define ENV_TARGET 0.0001f     // -80dB, umbral para considerar silencio

// Variables globales
Voice voices[MAX_VOICES];
static WaveType waveType = WAVE_SAW; // onda por defecto
LFOState lfo_state = {0.0f, 0.0f};


// ─── Coeficiente multiplicativo ──────────────────────────────────────────────
// Calcula cuánto multiplicar por sample para bajar de 1.0 a ENV_TARGET
// en 'time' segundos. Usado para Decay y Release.
static float calcCoef(float time)
{
    if (time <= 0.0f) return 0.0f;
    return powf(ENV_TARGET, 1.0f / (time * F_SAMPLE));
}


static void calcFilterCoefs(Voice *v, float cutoff, float Q)
{
    // Protección contra valores inestables
    if (cutoff < 20.0f)    cutoff = 20.0f;
    if (cutoff > 20000.0f) cutoff = 20000.0f;
    if (Q < 0.5f)          Q = 0.5f;
    if (Q > 20.0f)         Q = 20.0f;

    // Fórmula biquad pasa-bajos estándar (Robert Bristow-Johnson)
    float w0    = 2.0f * PI * cutoff / F_SAMPLE;
    float cosw0 = cosf(w0);
    float sinw0 = sinf(w0);
    float alpha = sinw0 / (2.0f * Q);

    float b0 =  (1.0f - cosw0) / 2.0f;
    float b1 =   1.0f - cosw0;
    float b2 =  (1.0f - cosw0) / 2.0f;
    float a0 =   1.0f + alpha;
    float a1 = -2.0f * cosw0;
    float a2 =   1.0f - alpha;

    // Normalizar por a0
    v->b0 =  b0 / a0;
    v->b1 =  b1 / a0;
    v->b2 =  b2 / a0;
    v->a1 =  a1 / a0;
    v->a2 =  a2 / a0;
}

void LFO_Tick(void)
{
    // Avanzar fase
    lfo_state.phase += lfo_params.rate / F_SAMPLE;
    if (lfo_state.phase >= 1.0f)
        lfo_state.phase -= 1.0f;

    // Calcular valor sine
    lfo_state.value = sinf(2.0f * PI * lfo_state.phase);
}

// ─── Init ────────────────────────────────────────────────────────────────────
void Synth_Init(void)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        voices[i].active       = 0;
        voices[i].phase        = 0.0f;
        voices[i].dPhase       = 0.0f;
        voices[i].note         = 0;
        voices[i].env          = 0.0f;
        voices[i].envState     = ENV_IDLE;
        voices[i].attackCoef   = 0.0f;
        voices[i].decayCoef    = 0.0f;
        voices[i].sustainLevel = 0.0f;
        voices[i].releaseCoef  = 0.0f;

        // Filtro
        voices[i].z1 = 0.0f;
        voices[i].z2 = 0.0f;
        calcFilterCoefs(&voices[i],
                        filter_params.cutoff,
                        filter_params.resonance);
    }
    waveType = WAVE_SQUARE;
}

void Synth_SetWaveType(WaveType type)
{
    waveType = type;
}

// ─── Note On ─────────────────────────────────────────────────────────────────
// Continúa el envelope desde donde está (no reinicia env)
void Synth_NoteOn(Voice *v, uint8_t note, float dPhase)
{
    v->note    = note;
    v->dPhase  = dPhase;
    v->phase   = 0.0f;
    v->active  = 1;
    v->envState     = ENV_ATTACK;
    v->attackCoef   = 1.0f / (adsr_params.attack  * F_SAMPLE);
    v->decayCoef    = calcCoef(adsr_params.decay);
    v->sustainLevel = adsr_params.sustain;
    v->releaseCoef  = calcCoef(adsr_params.release);

    // Resetear estado del filtro y calcular coeficientes
    v->z1 = 0.0f;
    v->z2 = 0.0f;
    calcFilterCoefs(v, filter_params.cutoff, filter_params.resonance);
}

// ─── Note Off ────────────────────────────────────────────────────────────────
// Pasa a Release inmediatamente desde donde esté el envelope
void Synth_NoteOff(Voice *v)
{
    if (v->envState != ENV_IDLE)
    {
        v->envState    = ENV_RELEASE;
        // Recalcula Release con el parámetro actual al momento de soltar
        v->releaseCoef = calcCoef(adsr_params.release);
    }
}

// ─── Actualización en tiempo real ────────────────────────────────────────────
// Llamar desde el callback del TIM6 después de Parameter_Update()
void Synth_UpdateActiveVoices(void)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (voices[i].active)
        {
            voices[i].decayCoef    = calcCoef(adsr_params.decay);
            voices[i].sustainLevel = adsr_params.sustain;
            voices[i].releaseCoef  = calcCoef(adsr_params.release);

            if (voices[i].envState == ENV_ATTACK)
                voices[i].attackCoef = 1.0f / (adsr_params.attack * F_SAMPLE);

            // Actualizar filtro en tiempo real
            calcFilterCoefs(&voices[i],
                            filter_params.cutoff,
                            filter_params.resonance);
        }
    }
}

// ─── Oscilador ───────────────────────────────────────────────────────────────
float Oscillator_Generate(Voice *v)
{
    float sample = 0.0f;

    switch (waveType)
    {
        case WAVE_SINE:
            sample = sinf(2.0f * PI * v->phase);
            break;
        case WAVE_SQUARE:
            sample = (v->phase < 0.5f) ? 1.0f : -1.0f;
            break;
        case WAVE_TRIANGLE:
            sample = 4.0f * fabsf(v->phase - 0.5f) - 1.0f;
            break;
        case WAVE_SAW:
            sample = 2.0f * v->phase - 1.0f;
            break;
    }

    v->phase += v->dPhase;
    if (v->phase >= 1.0f) v->phase -= 1.0f;

    return sample;
}

// ─── ADSR ────────────────────────────────────────────────────────────────────
float ADSR_Apply(Voice *v, float sample)
{
    switch (v->envState)
    {
        case ENV_ATTACK:
            v->env += v->attackCoef;
            if (v->env >= 1.0f)
            {
                v->env      = 1.0f;
                v->envState = ENV_DECAY;
            }
            break;

        case ENV_DECAY:
            v->env *= v->decayCoef;
            if (v->env <= v->sustainLevel)
            {
                v->env      = v->sustainLevel;
                v->envState = ENV_SUSTAIN;
            }
            break;

        case ENV_SUSTAIN:
            v->env = v->sustainLevel;
            break;

        case ENV_RELEASE:
            v->env *= v->releaseCoef;
            if (v->env <= ENV_TARGET)
            {
                v->env      = 0.0f;
                v->envState = ENV_IDLE;
                v->active   = 0;
            }
            break;

        case ENV_IDLE:
        default:
            v->env = 0.0f;
            break;
    }

    return sample * v->env;
}


float Filter_Apply(Voice *v, float sample)
{
    // Cutoff modulado por LFO
    // El LFO puede mover el cutoff hasta una octava arriba o abajo
    float cutoff_mod = filter_params.cutoff
                     + lfo_params.depth
                     * lfo_state.value
                     * filter_params.cutoff; // rango relativo al cutoff base

    // Protección
    if (cutoff_mod < 20.0f)    cutoff_mod = 20.0f;
    if (cutoff_mod > 20000.0f) cutoff_mod = 20000.0f;

    // Recalcular coeficientes con cutoff modulado
    calcFilterCoefs(v, cutoff_mod, filter_params.resonance);

    // Aplicar filtro
    float out = v->b0 * sample + v->z1;
    v->z1     = v->b1 * sample - v->a1 * out + v->z2;
    v->z2     = v->b2 * sample - v->a2 * out;
    return out;
}




// ─── Render ──────────────────────────────────────────────────────────────────
float Synth_RenderVoiceSample(Voice *v)
{
    float sample = Oscillator_Generate(v);
    sample = ADSR_Apply(v, sample);
    sample = Filter_Apply(v, sample);
    return sample;
}
