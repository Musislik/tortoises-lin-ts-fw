#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the filter state.
 */
void filterInit(void);

/**
 * @brief Process a raw ADC value through the configured software filter.
 * @param rawAdc The raw ADC reading.
 * @param swFilterConfig The software filter configuration mode.
 * @return The filtered ADC value.
 */
uint32_t filterProcess(uint32_t rawAdc, uint8_t swFilterConfig);

/**
 * @brief Calculate the temperature in 0.1 deg C from a filtered ADC value.
 * @param rawAdc The filtered ADC reading.
 * @param offsetMv The voltage offset at 0 degrees C.
 * @param gainSens The calibration gain sensitivity.
 * @return The calculated temperature in 0.1 deg C units.
 */
int32_t calcTemperature(uint32_t rawAdc, uint16_t offsetMv, uint16_t gainSens);

#endif // FILTER_H
