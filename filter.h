#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stdbool.h>

void filterInit(void);
uint32_t filterProcess(uint32_t raw_adc, uint8_t sw_filter_config);
int32_t calcTemperature(uint32_t raw_adc, uint16_t offset_mV, uint16_t gain_sens);

#endif // FILTER_H
