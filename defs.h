#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

// Defines for ADC
#define ADC12_0_INST ADC0
#define ADC12_0_ADCMEM_0 DL_ADC12_MEM_IDX_0

#define ADC_VREF_MV 3300
#define ADC_MAX_VAL 4095
#define TEMP_OFFSET_MV 500

// Defines for LIN
#define LIN_RX_BUFFER_LEN 32
#define LIN_TX_BUFFER_LEN 32
#define LIN_SYNC_BYTE (0x55)

// Macros
#define LinDataExpected() ((linRxState == LIN_RX_STATE_AWAITING) || (linRxState == LIN_RX_STATE_PID))

// LIN state
typedef enum {
    LIN_STATE_INIT = 0,
    LIN_STATE_IDLE = 1,
    LIN_STATE_RX = 2,
    LIN_STATE_TX = 3,
    LIN_STATE_FAULT = 4
} linState_t;

// LIN RX state
typedef enum {
    LIN_RX_STATE_INIT = 0,
    LIN_RX_STATE_IDLE = 1,
    LIN_RX_STATE_AWAITING = 2,
    LIN_RX_STATE_PID = 3,
    LIN_RX_STATE_RX_DATA = 4,
    LIN_RX_STATE_FAULT = 5
} linRxState_t;

#endif // DEFS_H
