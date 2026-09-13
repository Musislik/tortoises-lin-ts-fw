#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

// Defines for ADC
#define ADC12_0_INST ADC0
#define ADC12_0_ADCMEM_0 DL_ADC12_MEM_IDX_0

void adcInit(void);
void adcReconfigure(uint8_t hwConfig);

#endif
