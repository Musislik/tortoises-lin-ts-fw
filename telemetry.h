#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>

extern int16_t gCurrentMinTemp;
extern int16_t gCurrentMaxTemp;

void telemetryInit(void);
void telemetryUpdate(int16_t newTemp);
void telemetryTick(void); // Called every second

#endif // TELEMETRY_H
