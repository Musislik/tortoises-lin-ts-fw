#ifndef DEFS_H
#define DEFS_H

#include "ti_msp_dl_config.h"

// Defines for ADC
#define ADC12_0_INST                                                        ADC0
//#define ADC12_0_INST_IRQHandler                                  ADC0_IRQHandler
//#define ADC12_0_INST_INT_IRQN                                    (ADC0_INT_IRQn)
#define ADC12_0_ADCMEM_0                                      DL_ADC12_MEM_IDX_0
//#define ADC12_0_ADCMEM_0_REF                     DL_ADC12_REFERENCE_VOLTAGE_VDDA
//#define ADC12_0_ADCMEM_0_REF_VOLTAGE_V                                       3.3
//#define GPIO_ADC12_0_C6_PORT                                               GPIOA
//#define GPIO_ADC12_0_C6_PIN                                       DL_GPIO_PIN_20
//#define GPIO_ADC12_0_IOMUX_C6                                    (IOMUX_PINCM21)
//#define GPIO_ADC12_0_IOMUX_C6_FUNC                (IOMUX_PINCM21_PF_UNCONNECTED)

// Defines for LIN
#define LIN_RX_BUFFER_LEN 32
#define LIN_TX_BUFFER_LEN 32
#define LIN_SYNC_BYTE (0x55)


// Device PIDs
#define LIN_SEND_TEMP_PID 0x0A // 10 decimal, misto puvodniho 0x10 (16)

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
    LIN_RX_STATE_FAULT = 4

} linRxState_t;



// Define for flash operations
//#define FLASH_USER_START_ADDR (0x00001C00)
//#define CALIBRATION_MAGIC_WORD 0x2626

// Functions
//void ADC_init(void);
//void saveTempCal(int16_t value);
//int16_t loadTempCal(void);

#endif
