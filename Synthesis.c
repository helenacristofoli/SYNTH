/*
 * Synth.c
 *
 *  Created on: Feb 8, 2026
 *      Author: Helena
 */

#include "Synthesis.h"
#include "Mapping.h"
#include <math.h>

#define PI          3.14159f
#define ENV_TARGET  0.0001f     // -80dB, umbral para considerar silencio

// Variables globales
Voice    voices[MAX_VOICES];
static   WaveType waveType = WAVE_SAW;
LFOState lfo_state = {0.0f, 0.0f};


// ─── Coeficiente multiplicativo ──────────────────────────────────────────────
static float calcCoef(float time)
{
    if (time <= 0.0f) return 0.0f;
    return powf(ENV_TARGET, 1.0f / (time * F_SAMPLE));
}

static void calcFilterCoefs(Voice *v, float cutoff, float Q)
{
    if (cutoff < 20.0f)    cutoff = 20.0f;
    if (cutoff > 20000.0f) cutoff = 20000.0f;
    if (Q < 0.5f)          Q = 0.5f;
    if (Q > 20.0f)         Q = 20.0f;

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

    v->b0 = b0 / a0;
    v->b1 = b1 / a0;
    v->b2 = b2 / a0;
    v->a1 = a1 / a0;
    v->a2 = a2 / a0;
}

// ─── LFO ─────────────────────────────────────────────────────────────────────
void LFO_Tick(void)
{
    lfo_state.phase += lfo_params.rate / F_SAMPLE;
    if (lfo_state.phase >= 1.0f)
        lfo_state.phase -= 1.0f;

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
        voices[i].z1           = 0.0f;
        voices[i].z2           = 0.0f;
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
void Synth_NoteOn(Voice *v, uint8_t note, float dPhase)
{
    v->note         = note;
    v->dPhase       = dPhase;
    v->phase        = 0.0f;
    v->active       = 1;
    v->envState     = ENV_ATTACK;
    v->attackCoef   = 1.0f / (adsr_params.attack  * F_SAMPLE);
    v->decayCoef    = calcCoef(adsr_params.decay);
    v->sustainLevel = adsr_params.sustain;
    v->releaseCoef  = calcCoef(adsr_params.release);
    v->z1           = 0.0f;
    v->z2           = 0.0f;
    calcFilterCoefs(v, filter_params.cutoff, filter_params.resonance);
}

// ─── Note Off ────────────────────────────────────────────────────────────────
void Synth_NoteOff(Voice *v)
{
    if (v->envState != ENV_IDLE)
    {
        v->envState    = ENV_RELEASE;
        v->releaseCoef = calcCoef(adsr_params.release);
    }
}

// ─── Actualización en tiempo real ────────────────────────────────────────────
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

            // Solo actualizar filtro si el LFO no está modulando el cutoff
            // (si lo modula, se recalcula en cada sample dentro de Filter_Apply)
            if (synth_state.lfo != LFO_CUTOFF && synth_state.lfo != LFO_RESONANCE)
            {
                calcFilterCoefs(&voices[i],
                                filter_params.cutoff,
                                filter_params.resonance);
            }
        }
    }
}

// ─── Oscilador ───────────────────────────────────────────────────────────────
float Oscillator_Generate(Voice *v)
{
    float sample  = 0.0f;
    float dPhase  = v->dPhase;

    // LFO_PITCH — modula la frecuencia del oscilador
    if (synth_state.lfo == LFO_PITCH)
    {
        // El LFO desafina hasta ±1 semitono (factor 2^(1/12) ≈ 1.0595)
        float pitch_mod = 1.0f + lfo_params.depth * lfo_state.value * 0.0595f;
        if (pitch_mod < 0.01f) pitch_mod = 0.01f;
        dPhase *= pitch_mod;
    }

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

    v->phase += dPhase;
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

    // LFO_AMPLITUDE — modula el nivel de salida
    if (synth_state.lfo == LFO_AMPLITUDE)
    {
        // El LFO varía la amplitud entre 0 y 1 según depth
        float amp_mod = 1.0f - lfo_params.depth * (lfo_state.value * 0.5f + 0.5f);
        if (amp_mod < 0.0f) amp_mod = 0.0f;
        return sample * v->env * amp_mod;
    }

    return sample * v->env;
}

// ─── Filtro ──────────────────────────────────────────────────────────────────
float Filter_Apply(Voice *v, float sample)
{
    float cutoff_mod    = filter_params.cutoff;
    float resonance_mod = filter_params.resonance;

    switch (synth_state.lfo)
    {
        case LFO_CUTOFF:
            // Modula el cutoff hasta una octava arriba o abajo
            cutoff_mod = filter_params.cutoff
                       + lfo_params.depth
                       * lfo_state.value
                       * filter_params.cutoff;
            break;

        case LFO_RESONANCE:
            // Modula la resonancia entre 0.5 y su valor máximo actual
            resonance_mod = filter_params.resonance
                          + lfo_params.depth
                          * lfo_state.value
                          * filter_params.resonance;
            break;

        default:
            // LFO_AMPLITUDE y LFO_PITCH no afectan el filtro
            break;
    }

    // Protección
    if (cutoff_mod    < 20.0f)    cutoff_mod    = 20.0f;
    if (cutoff_mod    > 20000.0f) cutoff_mod    = 20000.0f;
    if (resonance_mod < 0.5f)     resonance_mod = 0.5f;
    if (resonance_mod > 20.0f)    resonance_mod = 20.0f;

    calcFilterCoefs(v, cutoff_mod, resonance_mod);

    float out = v->b0 * sample + v->z1;
    v->z1     = v->b1 * sample - v->a1 * out + v->z2;
    v->z2     = v->b2 * sample - v->a2 * out;
    return out;
}

// ─── Render ──────────────────────────────────────────────────────────────────
float Synth_RenderVoiceSample(Voice *v)
{
    float sample = Oscillator_Generate(v);
    sample       = ADSR_Apply(v, sample);
    sample       = Filter_Apply(v, sample);
    return sample;
}
