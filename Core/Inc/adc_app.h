/*
 * adc_app.h
 *
 *  Created on: Jan 22, 2026
 *      Author: matkins
 */

#ifndef INC_ADC_APP_H_
#define INC_ADC_APP_H_

#include <stdint.h>

void     adc_app_init(void);
void     adc_app_start(void);
void     adc_app_stop(void);

uint16_t adc_app_latest(void);
uint16_t adc_app_average(void);
uint16_t adc_read_once(void);
uint16_t adc_read_avg(uint8_t samples);
uint32_t adc_to_voltage(uint16_t raw);

float adc_to_ntc_resistance(uint16_t raw, float rdiv);
float thermistor_beta_to_celsius(uint16_t raw);

#endif /* INC_ADC_APP_H_ */
