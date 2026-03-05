/*
 * Controls.c
 *
 *  Created on: Feb 7, 2026
 *      Author: Helena
 *
 *  Manejo de controles: potenciómetros ADC, botones y registro de desplazamiento
 */

#include "controls.h"
#include "Mapping.h"
#include "main.h"   // Para BTN_WAVE_Pin, BTN_LFO_Pin, SR_Latch_Pin y sus puertos

#define ALPHA    0.1f
#define ADC_MAX  4095.0f
#define DEADBAND 10.0f

/* ================================================================
 * ADC
 * ================================================================ */
volatile uint16_t adc_raw[CTRL_COUNT];
volatile float    ctrl_norm[CTRL_COUNT];
volatile float    ctrl_smooth[CTRL_COUNT];
static   float    ctrl_stable[CTRL_COUNT];

static ADC_HandleTypeDef *ctrl_hadc;
static TIM_HandleTypeDef *ctrl_htim;

/* ================================================================
 * SHIFT REGISTER
 * ================================================================ */
static SPI_HandleTypeDef *sr_hspi;

/*
 * ShiftReg_Init
 * Guarda el handle SPI y deja el latch en HIGH (reposo)
 */
void ShiftReg_Init(SPI_HandleTypeDef *hspi)
{
    sr_hspi = hspi;
    HAL_GPIO_WritePin(SR_LATCH_GPIO_Port, SR_LATCH_Pin, GPIO_PIN_SET);
}

/*
 * ShiftReg_Write
 * Envía 1 byte al 74HC595 y genera el pulso de latch
 *
 * Secuencia:
 *   1. Latch LOW  → el 595 acepta datos
 *   2. SPI TX     → desplaza el byte
 *   3. Latch HIGH → transfiere a las salidas Q0-Q7
 */
void ShiftReg_Write(uint8_t data)
{
    HAL_GPIO_WritePin(SR_LATCH_GPIO_Port, SR_LATCH_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(sr_hspi, &data, 1, 10);
    HAL_GPIO_WritePin(SR_LATCH_GPIO_Port, SR_LATCH_Pin, GPIO_PIN_SET);
}

/* ================================================================
 * BOTONES
 * ================================================================ */
volatile uint8_t btn_wave_pressed = 0;
volatile uint8_t btn_lfo_pressed  = 0;

static uint32_t last_wave_tick = 0;
static uint32_t last_lfo_tick  = 0;

/*
 * Buttons_Init
 * Configura PB11 y PB15 como EXTI flanco descendente con pull-up
 * y habilita EXTI15_10_IRQn
 */
void Buttons_Init(void)
{
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/*
 * Buttons_HandleEXTI
 * Llamar desde HAL_GPIO_EXTI_Callback() en main.c
 * Verifica el estado real del pin y aplica debounce
 */
void Buttons_HandleEXTI(uint16_t GPIO_Pin)
{
    uint32_t now = HAL_GetTick();

    if (GPIO_Pin == BTN_WAVE_Pin)
    {
        if (HAL_GPIO_ReadPin(GPIOB, BTN_WAVE_Pin) == GPIO_PIN_RESET)
        {
            if ((now - last_wave_tick) >= DEBOUNCE_MS)
            {
                last_wave_tick   = now;
                btn_wave_pressed = 1;
            }
        }
    }
    else if (GPIO_Pin == BTN_LFO_Pin)
    {
        if (HAL_GPIO_ReadPin(GPIOB, BTN_LFO_Pin) == GPIO_PIN_RESET)
        {
            if ((now - last_lfo_tick) >= DEBOUNCE_MS)
            {
                last_lfo_tick   = now;
                btn_lfo_pressed = 1;
            }
        }
    }
}
/* ================================================================
 * ADC + controles
 * ================================================================ */
void Controls_Init(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim, SPI_HandleTypeDef *hspi)
{
    ctrl_hadc = hadc;
    ctrl_htim = htim;

    for (int i = 0; i < CTRL_COUNT; i++)
        ctrl_smooth[i] = 0.0f;

    // Inicializar shift register y botones
    ShiftReg_Init(hspi);
    Buttons_Init();
}

void Controls_Process(void)
{
    for (int i = 0; i < CTRL_COUNT; i++)
    {
        // 1. Filtro exponencial
        ctrl_smooth[i] = ALPHA * adc_raw[i]
                       + (1.0f - ALPHA) * ctrl_smooth[i];

        // 2. Deadband
        float diff = ctrl_smooth[i] - ctrl_stable[i];
        if (diff > DEADBAND || diff < -DEADBAND)
            ctrl_stable[i] = ctrl_smooth[i];

        // 3. Normalización
        ctrl_norm[i] = ctrl_stable[i] / ADC_MAX;
    }
}
