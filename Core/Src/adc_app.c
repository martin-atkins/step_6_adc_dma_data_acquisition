#include "adc_app.h"
#include "adc.h"
#include <string.h>
#include <stdio.h>

#include <math.h>    // for logf

#include "console.h"

#define ADC_DMA_BUF_LEN 32

static uint16_t adc_dma_buf[ADC_DMA_BUF_LEN];
static uint8_t  adc_running = 0;

void adc_app_init(void)
{
    // Nothing required yet
}

void adc_app_start(void)
{
    if (adc_running)
        return;

    HAL_ADC_Start_DMA(
        &hadc1,
        (uint32_t *)adc_dma_buf,
        ADC_DMA_BUF_LEN
    );

    adc_running = 1;
}

void adc_app_stop(void)
{
    if (!adc_running)
        return;

    HAL_ADC_Stop_DMA(&hadc1);
    adc_running = 0;
}

uint16_t adc_app_latest(void)
{
    if (!adc_running)
        return 0;

    // DMA is circular; most recent sample is "last written"
    uint32_t idx =
        ADC_DMA_BUF_LEN - __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);

    if (idx >= ADC_DMA_BUF_LEN)
        idx = 0;

    return adc_dma_buf[idx];
}

uint16_t adc_app_average(void)
{
    if (!adc_running)
        return 0;

    uint32_t sum = 0;

    for (int i = 0; i < ADC_DMA_BUF_LEN; i++)
        sum += adc_dma_buf[i];

    return (uint16_t)(sum / ADC_DMA_BUF_LEN);
}

uint16_t adc_read_once(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint16_t v = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return v;
}

uint16_t adc_read_avg(uint8_t samples)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += adc_read_once();
    }
    return sum / samples;
}

uint32_t adc_to_voltage(uint16_t raw)
{
    return (raw * 3300UL) / 4095;  // ADC 12-bit, Vref 3.3V
}

float adc_to_ntc_resistance(uint16_t raw, float rdiv)
{
    if (raw <= 0)      return 1e9f;   // avoid divide-by-zero
    if (raw >= 4095)   return 1.0f;

    float v = (raw / 4095.0f) * 3.3f;

    return rdiv * v / (3.3f - v);
}

float thermistor_beta_to_celsius(uint16_t raw)
{
    if (raw <= 0 || raw >= 4095)
        return -273.15f;

    const float VREF = 3.3f;
    const float R_PULLUP = 10000.0f;
    const float R0 = 2200.0f;      // 2.2k @ 25C
    const float BETA = 3950.0f;
    const float T0 = 298.15f;      // 25C in Kelvin

    float v = (raw / 4095.0f) * VREF;
    float r_ntc = R_PULLUP * v / (VREF - v);

    float temp_k =
        1.0f / ( (1.0f / T0) + (1.0f / BETA) * logf(r_ntc / R0) );

    float retval = temp_k - 273.15f;
    return retval;
}
