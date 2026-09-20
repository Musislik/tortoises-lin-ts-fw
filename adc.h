#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

// Defines for ADC
#define ADC12_0_INST ADC0
#define ADC12_0_ADCMEM_0 DL_ADC12_MEM_IDX_0

#define ADC_VREF_MV 3300
#define ADC_MAX_VAL 4095

/**
 * @brief Converts a raw ADC reading to voltage in millivolts.
 * @param raw The raw ADC value.
 * @return The corresponding voltage in millivolts.
 */
static inline float adcRawToMv(uint32_t raw) {
    return (float)((float)raw * ADC_VREF_MV) / ADC_MAX_VAL;
}

/**
 * @brief Initialize the ADC peripheral.
 *
 * This function prepares the hardware for initial temperature readings and applies the
 * initial configurations. It ensures the analog circuitry is fully powered and settled 
 * before sampling begins.
 */
void adcInit(void);

/**
 * @brief Reconfigure the ADC hardware based on settings.
 *
 * This function dynamically adapts the ADC sampling time and hardware averaging 
 * parameters during runtime. This allows the system to balance measurement 
 * precision and noise immunity based on varying external environments without 
 * resetting the device.
 *
 * @param hwConfig Configuration byte for hardware averaging and sample time.
 */
void adcReconfigure(uint8_t hwConfig);

#endif
