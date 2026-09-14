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
 *        Called every second to handle delayed flash writes.
 */
void telemetryTick(void);

#endif // TELEMETRY_H
