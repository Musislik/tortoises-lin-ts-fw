#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stdbool.h>

#define TEMP_INVALID_VALUE INT16_MAX

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
 * @brief Calculate the temperature in 0.1 deg C from a raw ADC reading.
 *
 * Internally performs a two-point calibration linear conversion using fixed-point
 * UQ16.16 arithmetic based on stored flash parameters, then applies the configured 
 * software filter (Moving Average or EMA).
 *
 * @param rawAdc The raw ADC measurement.
 * @return The calculated temperature in 0.1 deg C units, or INT16_MAX on error.
 */
int16_t calcTemperature(const uint32_t rawAdc);

/**
 * @brief Set the configuration parameters for the filter module.
 *
 * Updates the filter mode, offset, and gain. If any of the new 
 * parameters differ from the current ones, the internal filter state and 
 * lock mechanism will be reset via filterInit().
 *
 * @param swFilterConfig The software filter configuration mode.
 * @param offsetMv The voltage offset at 0 degrees C in mV in Q16.16 format.
 * @param gainSens The gain sensitivity in UQ16.16 format (e.g. 0x000A0000 for 10.0 mV/degC).
 */
void setFilterConfig(const FilterSwMode_t swFilterConfig, const int32_t offsetMv, const uint32_t gainSens);

#endif // FILTER_H
