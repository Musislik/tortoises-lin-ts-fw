/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ti/driverlib/dl_uart_extend.h"
#include "ti_msp_dl_config.h"

// Defines for LIN
#define LIN_RX_BUFFER_LEN 32
#define LIN_TX_BUFFER_LEN 32
#define LIN_SYNC_BYTE (0x55)

#define TIMER_1S_MS               1000
#define TIMER_100MS_MS            100
#define CHECKSUM_OVERFLOW_THRESHOLD 256
#define CHECKSUM_OVERFLOW_SUBTRACT  255
#define DELAY_SWD_CYCLES            240000000
#define DELAY_STANDARD_CYCLES       240000

#define LIN_TX_DATA_TEMP_LEN        3
#define LIN_TX_DATA_CONFIG_LEN      20
#define LIN_RX_DATA_CONFIG_LEN      16
#define LIN_RX_CONFIG_PAYLOAD_LEN   15

/**
 * @brief Checks if LIN is in a state expecting data.
 */
#define LIN_DATA_EXPECTED() ((sLinRxState == LIN_RX_STATE_AWAITING) || (sLinRxState == LIN_RX_STATE_PID))

/**
 * @brief Represents the global LIN state machine states.
 */
typedef enum {
    LIN_STATE_INIT = 0,
    LIN_STATE_IDLE = 1,
    LIN_STATE_RX = 2,
    LIN_STATE_TX = 3,
    LIN_STATE_FAULT = 4
} LinState_t;

/**
 * @brief Represents the LIN RX specific state machine states.
 */
typedef enum {
    LIN_RX_STATE_INIT = 0,
    LIN_RX_STATE_IDLE = 1,
    LIN_RX_STATE_AWAITING = 2,
    LIN_RX_STATE_PID = 3,
    LIN_RX_STATE_RX_DATA = 4,
    LIN_RX_STATE_FAULT = 5
} LinRxState_t;
#include "adc.h"
#include "config.h"
#include "filter.h"
#include "telemetry.h"

// Variables for LIN TX/RX
static volatile uint8_t txBuffer[LIN_TX_BUFFER_LEN] = {0};
static volatile uint32_t txBufferIx = 0;
static volatile uint32_t txBufferLen = 0;

static volatile uint8_t rxBuffer[LIN_RX_BUFFER_LEN] = {0};
static volatile uint32_t rxBufferIx = 0;
static volatile uint32_t expectedRxLen = 0;
static volatile uint8_t activeRxPid = 0;

static volatile LinRxState_t sLinRxState = LIN_RX_STATE_INIT;

// TIMER for telemetry and ADC timing
static volatile uint32_t timer1msCounter = 0;
static volatile uint32_t timer1sCounter = 0;
static volatile uint32_t timerAdcCounter = 0;
static volatile bool gFlagTimer1s = false;
static volatile bool gFlagStartAdc = false;
static volatile bool gFlagAdcReady = false;
static volatile uint32_t gRawAdc = 0;

// Pending config save from ISR
volatile bool gPendingConfigSave = false;
ConfigBlock_t gPendingConfig;

// Latest measured temperature available to LIN ISR
volatile int16_t gLatestTemperature = 0;

void TIMER_SYS_INST_IRQHandler(void) {
    switch (DL_TimerG_getPendingInterrupt(TIMER_SYS_INST)) {
        case DL_TIMER_IIDX_ZERO:
            timer1msCounter++;
            timer1sCounter++;
            timerAdcCounter++;

            if (timer1sCounter >= TIMER_1S_MS) {
                timer1sCounter = 0;
                gFlagTimer1s = true;
            }

            if (timerAdcCounter >= TIMER_100MS_MS) {
                timerAdcCounter = 0;
                gFlagStartAdc = true;
            }
            break;
        default:
            break;
    }
}

static uint8_t calcChecksum(uint8_t pid, const volatile uint8_t *buffer, uint8_t length) {
    uint16_t sum = pid;

    for (uint8_t i = 0; i < length; i++) {
        sum += buffer[i];
        if (sum >= CHECKSUM_OVERFLOW_THRESHOLD) {
            sum -= CHECKSUM_OVERFLOW_SUBTRACT;
        }
    }
    return (uint8_t)(~sum);
}

static void LIN_resetRX(LinRxState_t newState) {
    while (!DL_UART_isRXFIFOEmpty(LIN_INST)) {
        DL_UART_Extend_receiveData(LIN_INST);
    }

    DL_UART_Extend_clearInterruptStatus(LIN_INST, 
        DL_UART_EXTEND_INTERRUPT_RX | 
        DL_UART_EXTEND_INTERRUPT_OVERRUN_ERROR | 
        DL_UART_EXTEND_INTERRUPT_FRAMING_ERROR | 
        DL_UART_EXTEND_INTERRUPT_RX_TIMEOUT_ERROR | 
        DL_UART_EXTEND_INTERRUPT_PARITY_ERROR);

    rxBufferIx = 0;
    expectedRxLen = 0;
    activeRxPid = 0;
    sLinRxState = newState;
}

static void initHardware(void) {
    SYSCFG_DL_init();
    
    delay_cycles(DELAY_SWD_CYCLES/2); // 10s delay for SWD
    delay_cycles(DELAY_STANDARD_CYCLES);
    /*
    // Init GPIO - LIN enable pin
    DL_GPIO_initDigitalOutput(IOMUX_PINCM20);
    DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_19);
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_19);
    DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_19); // Enable LIN transceiver

    delay_cycles(DELAY_STANDARD_CYCLES);
    */
    configInit();
    filterInit();
    telemetryInit();
    
    adcInit(); // Init ADC so it transfers result to memory
    
    // LIN UART Interrupt settings
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_RX);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_FRAMING_ERROR);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_RX_TIMEOUT_ERROR);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_OVERRUN_ERROR);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_LIN_COUNTER_OVERFLOW);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_LINC0_MATCH);

    delay_cycles(DELAY_STANDARD_CYCLES);

    NVIC_ClearPendingIRQ(LIN_INST_INT_IRQN);
    NVIC_EnableIRQ(LIN_INST_INT_IRQN);
    
    // Enable Sys Timer Interrupt
    NVIC_ClearPendingIRQ(TIMER_SYS_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_SYS_INST_INT_IRQN);
    
    // Enable ADC Interrupt
    NVIC_ClearPendingIRQ(ADC0_INT_IRQn);
    NVIC_EnableIRQ(ADC0_INT_IRQn);

    LIN_resetRX(LIN_RX_STATE_IDLE);

    __enable_irq();

    delay_cycles(DELAY_STANDARD_CYCLES);
}

void ADC0_IRQHandler(void) {
    switch (DL_ADC12_getPendingInterrupt(ADC12_0_INST)) {
        case DL_ADC12_IIDX_MEM0_RESULT_LOADED:
            gRawAdc = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
            gFlagAdcReady = true;
            break;
        default:
            break;
    }
}

int main(void) {
    initHardware();

    // Trigger initial conversion
    gFlagStartAdc = true;

    while (1) {
        if (gPendingConfigSave) {
            // Flash writes are deferred to the main loop because executing them inside the ISR 
            // would block the CPU for too long, potentially causing LIN RX overruns or missing ADC events.
            __disable_irq();
            ConfigBlock_t newCfg = gPendingConfig;
            gPendingConfigSave = false;
            __enable_irq();

            if (!configSaveUser(&newCfg)) {
                // Flash write failed, could implement retry mechanism here
            } else {
                adcReconfigure(newCfg.filterHwAdc);
                filterInit(); // Reset software filter
            }
            
            // Trigger a conversion after reconfiguring ADC
            gFlagStartAdc = true;
        }

        if (gFlagStartAdc) {
            gFlagStartAdc = false;
            
            DL_ADC12_enableConversions(ADC12_0_INST);
            DL_ADC12_startConversion(ADC12_0_INST);
        }

        if (gFlagAdcReady) {
            gFlagAdcReady = false;
            
            uint32_t filteredAdc = filterProcess(gRawAdc, gActiveConfig.filterSwMode);
            int32_t temp = calcTemperature(filteredAdc, gActiveConfig.offsetMv, gActiveConfig.gainSens);
            
            // Atomic update of global temp for LIN ISR
            __disable_irq();
            gLatestTemperature = (int16_t)temp;
            __enable_irq();

            // Run telemetry (will safely write Flash if needed)
            telemetryUpdate((int16_t)temp);
        }

        if (gFlagTimer1s) {
            gFlagTimer1s = false;
            telemetryTick();
        }

        // Sleep safely (wake up on any interrupt).
        // This puts the CPU into a low-power state, significantly reducing power consumption 
        // while idling between ADC conversions or LIN bus activity.
        __WFI();
    }
}

void LIN_INST_IRQHandler(void) {
    uint32_t pendingFlags = DL_UART_Extend_getEnabledInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LINC0_MATCH);

    // Break detection
    if ((pendingFlags & DL_UART_INTERRUPT_LINC0_MATCH) == DL_UART_INTERRUPT_LINC0_MATCH) {
        // A break field signals the start of a new LIN frame. We unconditionally reset the 
        // state machine to resynchronize, discarding any incomplete/corrupted previous frames.
        DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LINC0_MATCH);
        LIN_resetRX(LIN_RX_STATE_AWAITING);
        return;
    }

    pendingFlags = DL_UART_Extend_getEnabledInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW);

    // LIN counter overflow
    if ((pendingFlags & DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW) == DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW) {
        DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW);
        DL_UART_Extend_setLINCounterValue(LIN_INST, 0);
        LIN_resetRX(LIN_RX_STATE_IDLE);
        return;
    }

    switch (DL_UART_Extend_getPendingInterrupt(LIN_INST)) {
        case DL_UART_EXTEND_IIDX_RX: {
            uint8_t rxByte = DL_UART_Extend_receiveData(LIN_INST);

            switch (sLinRxState) {
                case LIN_RX_STATE_AWAITING:
                    if (rxByte == LIN_SYNC_BYTE) {
                        sLinRxState = LIN_RX_STATE_PID;
                    } else {
                        sLinRxState = LIN_RX_STATE_IDLE;
                    }
                    break;

                case LIN_RX_STATE_PID:
                    if (rxByte == gActiveConfig.pidGetTemp) {
                        sLinRxState = LIN_RX_STATE_IDLE;

                        // Fast reply using latest prepared temp.
                        // By caching the ADC result in the main loop and using it here, we decouple 
                        // the communication layer from the slow ADC sampling process, guaranteeing 
                        // an immediate response within the tight LIN timing constraints.
                        int16_t temp = gLatestTemperature;

                        // Little-endian
                        txBuffer[0] = (uint8_t)(temp & 0xFF);
                        txBuffer[1] = (uint8_t)((temp >> 8) & 0xFF);
                        txBuffer[2] = calcChecksum(rxByte, txBuffer, 2);

                        txBufferIx = 1;
                        txBufferLen = LIN_TX_DATA_TEMP_LEN;

                        DL_UART_Extend_transmitData(LIN_INST, txBuffer[0]);
                        DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_TX);
                    } else if (rxByte == gActiveConfig.pidGetConfig) {
                        sLinRxState = LIN_RX_STATE_IDLE;
                        
                        // Populate 15 bytes of config + 4 bytes of factory SN
                        txBuffer[0] = (uint8_t)(gActiveConfig.logicalNodeId & 0xFF);
                        txBuffer[1] = (uint8_t)((gActiveConfig.logicalNodeId >> 8) & 0xFF);
                        txBuffer[2] = (uint8_t)((gActiveConfig.logicalNodeId >> 16) & 0xFF);
                        txBuffer[3] = (uint8_t)((gActiveConfig.logicalNodeId >> 24) & 0xFF);
                        txBuffer[4] = (uint8_t)(gActiveConfig.offsetMv & 0xFF);
                        txBuffer[5] = (uint8_t)((gActiveConfig.offsetMv >> 8) & 0xFF);
                        
                        uint32_t gainBits;
                        memcpy(&gainBits, &gActiveConfig.gainSens, sizeof(gainBits));
                        txBuffer[6] = (uint8_t)(gainBits & 0xFF);
                        txBuffer[7] = (uint8_t)((gainBits >> 8) & 0xFF);
                        txBuffer[8] = (uint8_t)((gainBits >> 16) & 0xFF);
                        txBuffer[9] = (uint8_t)((gainBits >> 24) & 0xFF);
                        
                        txBuffer[10] = gActiveConfig.pidGetTemp;
                        txBuffer[11] = gActiveConfig.pidGetConfig;
                        txBuffer[12] = gActiveConfig.pidSetConfig;
                        txBuffer[13] = gActiveConfig.filterHwAdc;
                        txBuffer[14] = gActiveConfig.filterSwMode;
                        
                        txBuffer[15] = (uint8_t)(gActiveFactory.factorySn & 0xFF);
                        txBuffer[16] = (uint8_t)((gActiveFactory.factorySn >> 8) & 0xFF);
                        txBuffer[17] = (uint8_t)((gActiveFactory.factorySn >> 16) & 0xFF);
                        txBuffer[18] = (uint8_t)((gActiveFactory.factorySn >> 24) & 0xFF);
                        
                        txBuffer[19] = calcChecksum(rxByte, txBuffer, 19);
                        
                        txBufferIx = 1;
                        txBufferLen = LIN_TX_DATA_CONFIG_LEN;

                        DL_UART_Extend_transmitData(LIN_INST, txBuffer[0]);
                        DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_TX);
                    } else if (rxByte == gActiveConfig.pidSetConfig) {
                        // We need to receive 13 bytes + checksum
                        sLinRxState = LIN_RX_STATE_RX_DATA;
                        rxBufferIx = 0;
                        expectedRxLen = LIN_RX_DATA_CONFIG_LEN; // 13 data + 1 cs
                        activeRxPid = rxByte;
                    } else {
                        // Unrecognized PID: effectively filters out irrelevant bus traffic targeting 
                        // other nodes by dropping back to IDLE until the next Break.
                        sLinRxState = LIN_RX_STATE_IDLE;
                    }
                    break;
                    
                case LIN_RX_STATE_RX_DATA:
                    rxBuffer[rxBufferIx++] = rxByte;
                    if (rxBufferIx >= expectedRxLen) {
                        sLinRxState = LIN_RX_STATE_IDLE;
                        
                        // Validate checksum
                        uint8_t cs = calcChecksum(activeRxPid, rxBuffer, LIN_RX_CONFIG_PAYLOAD_LEN);
                        if (cs == rxBuffer[LIN_RX_CONFIG_PAYLOAD_LEN]) {
                            // Valid frame, parse it
                            ConfigBlock_t newConfig = gActiveConfig;
                            newConfig.logicalNodeId = ((uint32_t)rxBuffer[3] << 24) | ((uint32_t)rxBuffer[2] << 16) | ((uint32_t)rxBuffer[1] << 8) | rxBuffer[0];
                            newConfig.offsetMv = ((uint16_t)rxBuffer[5] << 8) | rxBuffer[4];
                            
                            uint32_t gainBits = ((uint32_t)rxBuffer[9] << 24) | ((uint32_t)rxBuffer[8] << 16) | ((uint32_t)rxBuffer[7] << 8) | rxBuffer[6];
                            memcpy(&newConfig.gainSens, &gainBits, sizeof(newConfig.gainSens));
                            
                            newConfig.pidGetTemp = rxBuffer[10];
                            newConfig.pidGetConfig = rxBuffer[11];
                            newConfig.pidSetConfig = rxBuffer[12];
                            newConfig.filterHwAdc = rxBuffer[13];
                            newConfig.filterSwMode = rxBuffer[14];
                            
                            // Signal main loop to save to flash
                            gPendingConfig = newConfig;
                            gPendingConfigSave = true;
                        }
                    }
                    break;

                default:
                    break;
            }
            break;
        }
        case DL_UART_EXTEND_IIDX_TX: {
            if (txBufferIx < txBufferLen) {
                DL_UART_Extend_transmitData(LIN_INST, txBuffer[txBufferIx]);
                txBufferIx++;
            } else {
                DL_UART_disableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_TX);
                LIN_resetRX(LIN_RX_STATE_IDLE);
                txBufferLen = 0;
            }
            break;
        }
        case DL_UART_EXTEND_IIDX_OVERRUN_ERROR:
        case DL_UART_EXTEND_IIDX_FRAMING_ERROR:
        case DL_UART_EXTEND_IIDX_RX_TIMEOUT_ERROR:
        case DL_UART_EXTEND_IIDX_PARITY_ERROR: {
            LIN_resetRX(LIN_RX_STATE_IDLE);
            break;
        }
        default: {
            uint32_t pendingFlags = DL_UART_Extend_getEnabledInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LINC0_MATCH);
            // TODO: implement error status in config or something to report LIN master unrecognized IRQ!
            break;
        }
    }
}
