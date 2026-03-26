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
#define CUTOFF_SMOOTH_ALPHA  0.005f   // antes 0.02f — suavizado más lento
#define COEF_UPDATE_THRESH   20.0f    // antes 2.0f  — recalcula solo si cambia >20Hz

// Variables globales
Voice    voices[MAX_VOICES];
static   WaveType waveType = WAVE_SAW;
LFOState lfo_state = {0.0f, 0.0f};
float sine_table[SINE_TABLE_SIZE];

volatile VoiceEvent event_queue[EVENT_QUEUE_SIZE];
volatile uint8_t    event_queue_head = 0;
volatile uint8_t    event_queue_tail = 0;


// Procesar cola — llamado desde renderAudio(), dentro de la ISR de audio
void Synth_ProcessEventQueue(void)
{
    while (event_queue_head != event_queue_tail)
    {
        VoiceEvent *ev = (VoiceEvent*)&event_queue[event_queue_head];

        if (!ev->isNoteOff)
        {
            // Buscar voz libre o hacer stealing — misma lógica que main.c
            Voice *target = NULL;
            for (int i = 0; i < MAX_VOICES; i++)
            {
                if (!voices[i].active) { target = &voices[i]; break; }
            }
            if (!target) target = StealVoice();
            if (target) Synth_NoteOn(target, ev->note, ev->dPhase);
        }
        else
        {
            for (int i = 0; i < MAX_VOICES; i++)
            {
                if (voices[i].active && voices[i].note == ev->note)
                    Synth_NoteOff(&voices[i]);
            }
        }

        event_queue_head = (event_queue_head + 1) % EVENT_QUEUE_SIZE;
    }
}


Voice* StealVoice(void)
{
    Voice *best = NULL;
    float minEnv = 2.0f;

    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (voices[i].envState == ENV_RELEASE && voices[i].env < minEnv)
        {
            minEnv = voices[i].env;
            best   = &voices[i];
        }
    }
    if (best) return best;

    minEnv = 2.0f;
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (voices[i].envState != ENV_ATTACK && voices[i].env < minEnv)
        {
            minEnv = voices[i].env;
            best   = &voices[i];
        }
    }
    if (best) return best;

    minEnv = 2.0f;
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (voices[i].env < minEnv)
        {
            minEnv = voices[i].env;
            best   = &voices[i];
        }
    }
    return best;
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

    // Usar tabla seno en lugar de sinf()
    uint32_t idx = (uint32_t)(lfo_state.phase * SINE_TABLE_SIZE)
                   & (SINE_TABLE_SIZE - 1);
    lfo_state.value = sine_table[idx];
}

// ─── Init ────────────────────────────────────────────────────────────────────
void Synth_Init(void)
{
    for (int i = 0; i < SINE_TABLE_SIZE; i++)
        sine_table[i] = sinf(2.0f * 3.14159f * i / SINE_TABLE_SIZE);

    for (int i = 0; i < MAX_VOICES; i++)
    {
        voices[i].active         = 0;
        voices[i].phase          = 0.0f;
        voices[i].dPhase         = 0.0f;
        voices[i].note           = 0;
        voices[i].env            = 0.0f;
        voices[i].envState       = ENV_IDLE;
        voices[i].attackCoef     = cached_attackCoef;
        voices[i].decayCoef      = cached_decayCoef;
        voices[i].sustainLevel   = 0.0f;
        voices[i].releaseCoef    = cached_releaseCoef;
        voices[i].z1             = 0.0f;
        voices[i].z2             = 0.0f;
        voices[i].b0             = cached_b0;
        voices[i].b1             = cached_b1;
        voices[i].b2             = cached_b2;
        voices[i].a1             = cached_a1;
        voices[i].a2             = cached_a2;
        voices[i].cutoff_smooth  = filter_params.cutoff;
        voices[i].last_cutoff    = filter_params.cutoff;
        voices[i].last_resonance = filter_params.resonance;
        voices[i].releaseGuard   = 0;
        voices[i].fadeIn         = 1.0f;
    }
}


void Synth_SetWaveType(WaveType type)
{
    waveType = type;
}

// ─── Note On ─────────────────────────────────────────────────────────────────
void Synth_NoteOn(Voice *v, uint8_t note, float dPhase)
{
    EnvState prevState = v->envState;

    v->note           = note;
    v->dPhase         = dPhase;
    v->active         = 1;
    v->env            = 0.0f;
    v->envState       = ENV_ATTACK;
    v->attackCoef     = cached_attackCoef;
    v->decayCoef      = cached_decayCoef;
    v->sustainLevel   = adsr_params.sustain;
    v->releaseCoef    = cached_releaseCoef;
    v->releaseGuard   = 0;
    v->fadeIn         = 0.0f;

    if (prevState == ENV_IDLE)
    {
        v->z1    = 0.0f;
        v->z2    = 0.0f;
        v->phase = 0.0f;
    }

    // Asignar coefs de filtro cacheados — sin cosf/sinf en ISR
    v->b0             = cached_b0;
    v->b1             = cached_b1;
    v->b2             = cached_b2;
    v->a1             = cached_a1;
    v->a2             = cached_a2;
    v->cutoff_smooth  = filter_params.cutoff;
    v->last_cutoff    = filter_params.cutoff;
    v->last_resonance = filter_params.resonance;
}

// ─── Note Off ────────────────────────────────────────────────────────────────
void Synth_NoteOff(Voice *v)
{
    if (v->envState != ENV_IDLE)
    {
        v->envState    = ENV_RELEASE;
        v->releaseCoef = cached_releaseCoef;
        if (v->releaseCoef <= 0.0f || v->releaseCoef >= 1.0f)
            v->releaseCoef = powf(0.0001f, 1.0f / (0.01f * F_SAMPLE));
    }
}

// ─── Actualización en tiempo real ────────────────────────────────────────────
void Synth_UpdateActiveVoices(void)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (voices[i].active)
        {
            voices[i].attackCoef   = cached_attackCoef;
            voices[i].decayCoef    = cached_decayCoef;
            voices[i].sustainLevel = adsr_params.sustain;
            voices[i].releaseCoef  = cached_releaseCoef;
        }
    }
}

// ─── Oscilador ───────────────────────────────────────────────────────────────
float Oscillator_Generate(Voice *v)
{
    float sample = 0.0f;
    float dPhase = v->dPhase;

    if (synth_state.lfo == LFO_PITCH)
    {
        float pitch_mod = 1.0f + lfo_params.depth * lfo_state.value * 0.0595f;
        if (pitch_mod < 0.01f) pitch_mod = 0.01f;
        dPhase *= pitch_mod;
    }

    switch (waveType)
    {
        case WAVE_SINE: {
            // NUEVO: lookup table en lugar de sinf()
            uint32_t idx = (uint32_t)(v->phase * SINE_TABLE_SIZE)
                           & (SINE_TABLE_SIZE - 1);
            sample = sine_table[idx];
            break;
        }
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
            v->env += v->attackCoef * (1.0f - v->env);
            if (v->env >= 0.9999f)
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
            v->releaseGuard++;
            if (v->env <= ENV_TARGET
                || v->env < 0.0001f
                || v->releaseGuard > (uint32_t)(5.0f * F_SAMPLE))
            {
                v->env          = 0.0f;
                v->envState     = ENV_IDLE;
                v->active       = 0;
                v->releaseGuard = 0;
            }
            break;

        case ENV_IDLE:
        default:
            v->env = 0.0f;
            break;
    }

    if (synth_state.lfo == LFO_AMPLITUDE)
    {
        float amp_mod = 1.0f - lfo_params.depth * (lfo_state.value * 0.5f + 0.5f);
        if (amp_mod < 0.0f) amp_mod = 0.0f;
        return sample * v->env * amp_mod;
    }

    return sample * v->env;
}

// ─── Filtro ──────────────────────────────────────────────────────────────────
float Filter_Apply(Voice *v, float sample)
{
    float cutoff_target = filter_params.cutoff;
    float resonance_mod = filter_params.resonance;

    switch (synth_state.lfo)
    {
        case LFO_CUTOFF:
            cutoff_target = filter_params.cutoff *
                            (1.0f + lfo_params.depth * lfo_state.value);
            if (cutoff_target < 20.0f)    cutoff_target = 20.0f;
            if (cutoff_target > 18000.0f) cutoff_target = 18000.0f;
            break;

        case LFO_RESONANCE:
            resonance_mod = filter_params.resonance *
                            (1.0f + lfo_params.depth * lfo_state.value);
            if (resonance_mod < 0.5f)  resonance_mod = 0.5f;
            if (resonance_mod > 20.0f) resonance_mod = 20.0f;
            break;

        default:
            // Sin LFO en filtro — usar coefs cacheados directamente
            // sin ningún recálculo
            v->b0 = cached_b0;
            v->b1 = cached_b1;
            v->b2 = cached_b2;
            v->a1 = cached_a1;
            v->a2 = cached_a2;
            goto apply_biquad;
    }

    // Solo llega aquí si LFO_CUTOFF o LFO_RESONANCE están activos
    v->cutoff_smooth += CUTOFF_SMOOTH_ALPHA *
                        (cutoff_target - v->cutoff_smooth);

    float diff_c = v->cutoff_smooth - v->last_cutoff;
    if (diff_c < 0.0f) diff_c = -diff_c;
    float diff_r = resonance_mod - v->last_resonance;
    if (diff_r < 0.0f) diff_r = -diff_r;

    if (diff_c > COEF_UPDATE_THRESH || diff_r > 0.01f)
    {
        // Este calcFilterCoefs solo se ejecuta cuando el LFO
        // modula el filtro Y el cambio supera el umbral
        calcFilterCoefs(v, v->cutoff_smooth, resonance_mod);
        v->last_cutoff    = v->cutoff_smooth;
        v->last_resonance = resonance_mod;
    }

apply_biquad:
    {
        float out = v->b0 * sample + v->z1;
        v->z1     = v->b1 * sample - v->a1 * out + v->z2;
        v->z2     = v->b2 * sample - v->a2 * out;
        return out;
    }
}

// ─── Render ──────────────────────────────────────────────────────────────────
float Synth_RenderVoiceSample(Voice *v)
{
    float sample = Oscillator_Generate(v);
    sample       = ADSR_Apply(v, sample);
    sample       = Filter_Apply(v, sample);

    // Anti-click: fade-in lineal de FADE_IN_SAMPLES samples
    if (v->fadeIn < 1.0f)
    {
        sample    *= v->fadeIn;
        v->fadeIn += 1.0f / FADE_IN_SAMPLES;
        if (v->fadeIn > 1.0f) v->fadeIn = 1.0f;
    }

    return sample;
}
