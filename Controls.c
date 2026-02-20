/*
 * Controls.c
 *
 *  Created on: Feb 7, 2026
 *      Author: Helena
 */

#include "controls.h"
#include "Mapping.h"

#define ALPHA 0.1f
#define ADC_MAX 4095.0f
#define DEADBAND 10.0f



volatile uint16_t adc_raw[CTRL_COUNT];
volatile float ctrl_norm[CTRL_COUNT];
volatile float ctrl_smooth[CTRL_COUNT];
static float ctrl_stable[CTRL_COUNT];

static ADC_HandleTypeDef *ctrl_hadc;
static TIM_HandleTypeDef *ctrl_htim;

void Controls_Init(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim)
{
    ctrl_hadc = hadc;
    ctrl_htim = htim;

    for (int i = 0; i < CTRL_COUNT; i++)
        ctrl_smooth[i] = 0.0f;

}

void Controls_Process(void)
{
    for (int i = 0; i < CTRL_COUNT; i++)
    {
        // 1. Filtro exponencial
        ctrl_smooth[i] = ALPHA * adc_raw[i]
                       + (1.0f - ALPHA) * ctrl_smooth[i];

        // 2. Deadband (sobre el valor filtrado)
        float diff = ctrl_smooth[i] - ctrl_stable[i];

        if (diff > DEADBAND || diff < -DEADBAND)
        {
            ctrl_stable[i] = ctrl_smooth[i];
        }

        // 3. Normalización (solo del valor estable)
        ctrl_norm[i] = ctrl_stable[i] / ADC_MAX;
    }

}
