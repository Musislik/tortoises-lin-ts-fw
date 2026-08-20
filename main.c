/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ti_msp_dl_config.h"
#include "defs.h"
#include "adc.h"

// Variables for Calibration
volatile int16_t tempCal = 0;
volatile bool flagSaveTempCal = false;

// Variables for LIN TX
uint8_t txBuffer[LIN_TX_BUFFER_LEN] = {0};
volatile uint32_t txBufferIx = 0;
volatile uint32_t txBufferLen = 0;

// External functions
extern int16_t loadTempCal(void);
extern void saveTempCal(int16_t);
extern void adcInit(void);

int32_t calcTempx10(uint32_t adc_raw)
{
    int32_t voltage_mV = ((int32_t)adc_raw * ADC_VREF_MV) / ADC_MAX_VAL;
    int16_t temp_C_x10 = (int16_t)(voltage_mV - TEMP_OFFSET_MV); // 5.6 degC = 56
    
    return temp_C_x10 - tempCal;
}

uint8_t calcChecksum(uint8_t pid, const uint8_t *buffer, uint8_t length)
{
    uint16_t sum = pid;

    for (uint8_t i = 0; i < length; i++)
    {
        sum += buffer[i];
        if (sum >= 256)
        {
            sum -= 255;
        }
    }
    return (uint8_t)(~sum);
}

void initHardware(void)
{
    SYSCFG_DL_init();
    delay_cycles(240000000); // 10s delay for SWD
    delay_cycles(240000);

    // Init GPIO - LIN enable pin
    DL_GPIO_initDigitalOutput(IOMUX_PINCM20);
    DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_19);
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_19);
    DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_19); // Enable LIN transceiver

    delay_cycles(240000);

    adcInit(); // Init ADC so it transfers result to memory
    
    // Load calibration from flash
    tempCal = loadTempCal();
    
    // LIN UART Interrupt settings
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_RX);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_FRAMING_ERROR);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_RX_TIMEOUT_ERROR);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_OVERRUN_ERROR);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_LIN_COUNTER_OVERFLOW);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_LINC0_MATCH);

    delay_cycles(240000);

    NVIC_ClearPendingIRQ(LIN_INST_INT_IRQN);
    NVIC_EnableIRQ(LIN_INST_INT_IRQN);
    __enable_irq();

    delay_cycles(240000);
}

int main(void)
{
    initHardware();

    while (1) {
        if (flagSaveTempCal) {
            flagSaveTempCal = false;
            saveTempCal(tempCal);
        }

        // Sleep safely (wake up on any interrupt)
        __disable_irq();
        if (!flagSaveTempCal) {
            __WFI();
        }
        __enable_irq();
    }
}

volatile linRxState_t linRxState = LIN_RX_STATE_INIT;

void LIN_INST_IRQHandler(void)
{
    uint32_t pendingFlags = DL_UART_Extend_getEnabledInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LINC0_MATCH | DL_UART_INTERRUPT_RX);

    // Break detection
    if ((pendingFlags & DL_UART_INTERRUPT_LINC0_MATCH) == DL_UART_INTERRUPT_LINC0_MATCH)
    {
        DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LINC0_MATCH);
        linRxState = LIN_RX_STATE_AWAITING;
        return;
    }

    pendingFlags = DL_UART_Extend_getEnabledInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW);

    // LIN counter overflow
    if ((pendingFlags & DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW) == DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW)
    {
        DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW);
        return;
    }

    switch (DL_UART_Extend_getPendingInterrupt(LIN_INST))
    {
        case DL_UART_EXTEND_IIDX_RX: // Data received
        {
            uint8_t rxByte = DL_UART_Extend_receiveData(LIN_INST);

            switch (linRxState) {
                case LIN_RX_STATE_AWAITING:
                    if (rxByte == LIN_SYNC_BYTE) {
                        linRxState = LIN_RX_STATE_PID;
                    } else {
                        linRxState = LIN_RX_STATE_IDLE;
                    }
                    break;

                case LIN_RX_STATE_PID:
                    if (rxByte == LIN_SEND_TEMP_PID)
                    {
                        linRxState = LIN_RX_STATE_IDLE;

                        uint32_t adcTempVal = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
                        int32_t temp = calcTempx10(adcTempVal);

                        txBuffer[0] = (uint8_t)((temp >> 8) & 0xFF);
                        txBuffer[1] = (uint8_t)(temp & 0xFF);
                        txBuffer[2] = calcChecksum(LIN_SEND_TEMP_PID, txBuffer, 2);

                        txBufferIx = 1;
                        txBufferLen = 3;

                        // Start transmission
                        DL_UART_Extend_transmitData(LIN_INST, txBuffer[0]);
                        DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_TX);
                    }
                    else if (rxByte == LIN_CALIBRATE_PID)
                    {
                        linRxState = LIN_RX_STATE_IDLE;

                        uint32_t adcTempVal = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
                        int32_t temp = calcTempx10(adcTempVal);

                        // Assuming calibration is done at exactly 0°C.
                        // calcTempx10 computes the error (measured vs 0), we add it to the offset.
                        tempCal += temp;
                        flagSaveTempCal = true;
                    }
                    else
                    {
                        linRxState = LIN_RX_STATE_IDLE;
                    }
                    break;

                default:
                    break;
            }
            break;
        }
        case DL_UART_EXTEND_IIDX_TX:
        {
            if (txBufferIx < txBufferLen)
            {
                DL_UART_Extend_transmitData(LIN_INST, txBuffer[txBufferIx++]);
            }
            else
            {
                DL_UART_disableInterrupt(LIN_INST, DL_UART_EXTEND_IIDX_TX);
            }
            break;
        }
        case DL_UART_EXTEND_IIDX_FRAMING_ERROR:
        {
            DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_MAIN_INTERRUPT_FRAMING_ERROR);
            DL_UART_Extend_receiveData(LIN_INST); // Clear data
            break;
        }
        case DL_UART_EXTEND_IIDX_OVERRUN_ERROR:
        {
            DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_MAIN_INTERRUPT_OVERRUN_ERROR);
            DL_UART_Extend_receiveData(LIN_INST); // Clear data
            break;
        }
        case DL_UART_EXTEND_IIDX_BREAK_ERROR:
        {
            DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_MAIN_INTERRUPT_BREAK_ERROR);
            break;
        }
        case DL_UART_EXTEND_INTERRUPT_RX_TIMEOUT_ERROR:
        {
            DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_EXTEND_INTERRUPT_RX_TIMEOUT_ERROR);
            break;
        }
        default:
        {
            DL_UART_Extend_receiveData(LIN_INST); // Clear unused data
            break;
        }
    }
}
