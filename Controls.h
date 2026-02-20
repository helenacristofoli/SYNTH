/*
 * Controls.h
 *
 *  Created on: Feb 7, 2026
 *      Author: Helena
 */

#ifndef INC_CONTROLS_H_
#define INC_CONTROLS_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"

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
extern volatile float ctrl_smooth[CTRL_COUNT];


void Controls_Init(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim);
void Controls_Process(void);

#endif /* INC_CONTROLS_H_ */
