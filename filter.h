#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stdbool.h>

#define TEMP_INVALID_VALUE 0xFFFF

/**
 * @brief Software filtering modes for ADC measurements.
 */
typedef enum {
    FILTER_SW_MODE_PASSTHROUGH = 0x00,
    FILTER_SW_MODE_MA_4        = 0x01,
    FILTER_SW_MODE_MA_8        = 0x02,
    FILTER_SW_MODE_MA_16       = 0x03,
    FILTER_SW_MODE_EMA_1       = 0x04,
    FILTER_SW_MODE_EMA_2       = 0x05,
    FILTER_SW_MODE_EMA_3       = 0x06
} FilterSwMode_t;

/**
 * @brief Initialize the filter state.
 *
 * Resets the ring buffers and tracking variables. This must be called after 
 * boot or whenever the filter configuration changes to prevent stale data 
 * from corrupting the new filter output.
 */
void filterInit(void);

/**
 * @brief Calculate the temperature in 0.1 deg C from a raw ADC value.
 *
 * Internally applies the configured software filter (Moving Average or EMA),
 * then performs a two-point calibration linear conversion based on stored flash parameters.
 *
 * @param rawAdc The raw ADC reading.
 * @param offsetMv The voltage offset at 0 degrees C in mV.
 * @param gainSens The gain sensitivity in mv/degC.
 * @return The calculated temperature in 0.1 deg C units, or INT32_MAX on error.
 */
int32_t calcTemperature(uint32_t rawAdc, uint16_t offsetMv, float gainSens);

/**
 * @brief Set the active software filter configuration mode.
 *
 * @param swFilterConfig The software filter configuration mode.
 */
void setSwFilterConfig(FilterSwMode_t swFilterConfig);

#endif // FILTER_H
