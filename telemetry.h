#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>

extern int16_t gCurrentMinTemp;
extern int16_t gCurrentMaxTemp;

/**
 * @brief Initialize the telemetry module and load previous extremes.
 */
void telemetryInit(void);

/**
 * @brief Update telemetry statistics with a new temperature reading.
 * @param newTemp The new temperature reading in 0.1 deg C.
 */
void telemetryUpdate(int16_t newTemp);

/**
 * @brief Periodic tick function for telemetry processing.
 *
 * This function handles delayed (debounced) flash writes. Batching the updates 
 * and waiting for the extremes to settle significantly reduces flash wear, 
 * prolonging the lifespan of the memory sector compared to writing on every change.
 *
 * @note Must be called periodically (e.g., every second).
 */
void telemetryTick(void);

#endif // TELEMETRY_H
