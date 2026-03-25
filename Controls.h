/*
 * Controls.h
 *
 *  Created on: Feb 7, 2026
 *      Author: Helena
 *
 *  Manejo de controles: potenciómetros ADC, botones y registro de desplazamiento
 */

#ifndef INC_CONTROLS_H_
#define INC_CONTROLS_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"

/* ================================================================
 * ADC — Identificadores de controles
 * ================================================================ */
typedef enum
{
    CTRL_RESONANCE = 0,
    CTRL_DECAY,
    CTRL_RATE,
    CTRL_RELEASE,
    CTRL_VOLUME,
    CTRL_CUTOFF,
    CTRL_ATTACK,
    CTRL_SUSTAIN,
    CTRL_DEPTH,

    CTRL_COUNT
} ControlID;

extern volatile uint16_t adc_raw[CTRL_COUNT];
extern volatile float    ctrl_norm[CTRL_COUNT];
extern volatile float    ctrl_smooth[CTRL_COUNT];

/* ================================================================
 * SHIFT REGISTER — Máscaras de LEDs para el 74HC595
 *
 * Mapeo de salidas Q:
 *   Q0 → Resonance  (LFO target)
 *   Q1 → Sine       (onda)
 *   Q2 → Square     (onda)
 *   Q3 → Triangle   (onda)
 *   Q4 → Saw        (onda)
 *   Q5 → Amplitude  (LFO target)
 *   Q6 → Pitch      (LFO target)
 *   Q7 → Cutoff     (LFO target)
 * ================================================================ */
#define SR_LED_SINE       0x02   // QB - pin 1
#define SR_LED_SQUARE     0x01   // QA - pin 15
#define SR_LED_TRIANGLE   0x08   // QD - pin 3
#define SR_LED_SAW        0x04   // QC - pin 2
#define SR_LED_AMPLITUDE  0x10   // QE - pin 4
#define SR_LED_PITCH      0x20   // QF - pin 5
#define SR_LED_CUTOFF     0x40   // QG - pin 6
#define SR_LED_RESONANCE  0x80   // QH - pin 7

/* ================================================================
 * BOTONES — Flags de evento
 * Se activan en la ISR y se consumen en el loop principal
 * ================================================================ */
#define DEBOUNCE_MS   100U

extern volatile uint8_t btn_wave_pressed;
extern volatile uint8_t btn_lfo_pressed;

/* ================================================================
 * Funciones públicas
 * ================================================================ */

// ADC + controles
void Controls_Init(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim, SPI_HandleTypeDef *hspi);
void Controls_Process(void);

// Shift register
void ShiftReg_Init(SPI_HandleTypeDef *hspi);
void ShiftReg_Write(uint8_t data);

// Botones
void Buttons_Init(void);
void Buttons_HandleEXTI(uint16_t GPIO_Pin);

#endif /* INC_CONTROLS_H_ */
