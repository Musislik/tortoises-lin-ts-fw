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

volatile int16_t tempCal = 0;
volatile bool flagSaveTempCal = false;

volatile linRxState_t linRxState = LIN_RX_STATE_INIT;
uint8_t rxBuffer[LIN_RX_BUFFER_LEN] = {0};
uint32_t rxBufferLen = 0;
uint32_t rxBufferIx = 0;
volatile bool flagCallRxHandler;

uint8_t txBuffer[LIN_TX_BUFFER_LEN] = {0};
volatile uint32_t txBufferIx = 0;
volatile uint32_t txBufferLen = 0;

extern int16_t loadTempCal(void);
extern void saveTempCal(int16_t);
extern void adcInit(void);

void printTemp()
{
        char *adc_msg = " | ADC: ";
        while (*adc_msg) {
            DL_UART_Extend_transmitDataBlocking(LIN_INST, *adc_msg++);
        }
        uint32_t temp = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
        char buffer[10];
        int idx = 0;
        
        do {
            buffer[idx++] = (temp % 10) + '0'; 
            temp /= 10;
        } while (temp > 0);
        
        while (idx > 0) {
            DL_UART_Extend_transmitDataBlocking(LIN_INST, buffer[--idx]);
        }
}

int32_t calcTempx10(uint32_t adc_raw) 
{
    int32_t voltage_mV = ((int32_t)adc_raw * 3300) / 4095;    
    int16_t temp_C_x10 = (int16_t)(voltage_mV - 500);       // 5.6 degC = 56

    temp_C_x10 = temp_C_x10 - tempCal;
    
    return temp_C_x10;
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

void LIN_RX_Handler(uint8_t rxByte)
{
    uint8_t linPid;
    switch (linRxState) {
        case LIN_RX_STATE_AWAITING:
        {            
            // Sync byte expected
            rxBufferLen = 0;
            
            if (rxByte == LIN_SYNC_BYTE) 
                linRxState = LIN_RX_STATE_PID;
            else
                linRxState = LIN_RX_STATE_IDLE;
            break;
        }
        case LIN_RX_STATE_PID:        
        {
            linPid = rxByte;

            if (linPid == LIN_SEND_TEMP_PID)
            {
                linRxState = LIN_RX_STATE_IDLE;
                //LIN_STATE = LIN_STATE_TX;
                rxBufferLen = 0;

                uint32_t adcTempVal = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
                int32_t temp = calcTempx10(adcTempVal);

                txBuffer[0] = (uint8_t)((temp >> 8) & 0xFF);
                txBuffer[1] = (uint8_t)((temp) & 0xFF); 
                txBuffer[2] = calcChecksum(LIN_SEND_TEMP_PID, txBuffer, 2);
                
                DL_UART_Extend_transmitData(LIN_INST, txBuffer[0]);
                txBufferIx = 1;
                
                DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_TX);
                break; 
            }
            rxBufferLen = 0;
            break;
                   
        }
        default:
        {            
            break;
        }            
    }
}

int main(void)
{
    //DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_RX);
    SYSCFG_DL_init();   

    //DL_GPIO_togglePins(GPIO_PORT, GPIO_DBG_PIN);
    
    delay_cycles(240000000);    // 10s delay for SWD 

    //DL_GPIO_togglePins(GPIO_PORT, GPIO_DBG_PIN);
    delay_cycles(240000);
    
    DL_GPIO_initDigitalOutput(IOMUX_PINCM20);       // Init GPIO - LIN enable pin
    DL_GPIO_clearPins(GPIOA, DL_GPIO_PIN_19);
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_19);    //
    DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_19);         // Enable LIN tranceiver

    //DL_GPIO_togglePins(GPIO_PORT, GPIO_DBG_PIN);
    delay_cycles(240000);

    //tempCal = loadTempCal();

    adcInit();     // Init DMA so it transfers ADC result to temp_raw variable

    
    // Interrupt settings
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_RX);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_FRAMING_ERROR);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_RX_TIMEOUT_ERROR);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_OVERRUN_ERROR);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_LIN_COUNTER_OVERFLOW);
    DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_LINC0_MATCH);

    //DL_GPIO_togglePins(GPIO_PORT, GPIO_DBG_PIN);
    delay_cycles(240000);

    NVIC_ClearPendingIRQ(LIN_INST_INT_IRQN);
    NVIC_EnableIRQ(LIN_INST_INT_IRQN);
    __enable_irq();

    //DL_GPIO_togglePins(GPIO_PORT, GPIO_DBG_PIN);
    delay_cycles(240000);

    //DL_UART_transmitDataBlocking(LIN_INST, 0x56);

    while (1) {
        if (flagSaveTempCal) {
            flagSaveTempCal = false;
            saveTempCal(tempCal);
        }
        if (flagCallRxHandler == true) {
            flagCallRxHandler = false;
            LIN_RX_Handler(rxBuffer[rxBufferLen - 1]);
        }

        //__WFI();
    }
}

void LIN_INST_IRQHandler(void)
{
    uint8_t rxByte;

    /// Break detection
    uint32_t pendingFlags = DL_UART_Extend_getEnabledInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LINC0_MATCH | DL_UART_INTERRUPT_RX);

    if ((pendingFlags & DL_UART_INTERRUPT_LINC0_MATCH) == DL_UART_INTERRUPT_LINC0_MATCH) 
    {
        DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LINC0_MATCH);

        linRxState = LIN_RX_STATE_AWAITING;
        return;
    }

    pendingFlags = DL_UART_Extend_getEnabledInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW);

    /// Lin counter overflow
    if ((pendingFlags & DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW) == DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW) 
    {
        DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_INTERRUPT_LIN_COUNTER_OVERFLOW);
        
        return;      
    }
    
    
    switch (DL_UART_Extend_getPendingInterrupt(LIN_INST)) 
    {
        
        case DL_UART_EXTEND_IIDX_RX:    // Data received 
        {
            rxByte = DL_UART_Extend_receiveData(LIN_INST);

            if (LinDataExpected() && ((rxBufferLen + 1) < LIN_RX_BUFFER_LEN)) 
            {
                rxBuffer[rxBufferLen] = rxByte;
                rxBufferLen++;
                flagCallRxHandler = true;
            }
            else 
            {
                DL_UART_Extend_receiveData(LIN_INST);
                break;
            }
        }
        case DL_UART_EXTEND_IIDX_TX:
        {
            if(txBufferIx < txBufferLen)
            {
                DL_UART_Extend_transmitData(LIN_INST, txBuffer[txBufferIx]);
                txBufferIx++;
            }
            else
            {
                DL_UART_disableInterrupt(LIN_INST, DL_UART_EXTEND_IIDX_TX);
            }
            break;
        }
        case DL_UART_EXTEND_IIDX_FRAMING_ERROR:
        {
            NVIC_ClearPendingIRQ(LIN_INST_INT_IRQN);
            DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_MAIN_INTERRUPT_FRAMING_ERROR);
            DL_UART_Extend_receiveData(LIN_INST);
            break;
        }
        case DL_UART_EXTEND_IIDX_OVERRUN_ERROR:
        {
            NVIC_ClearPendingIRQ(LIN_INST_INT_IRQN);
            DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_MAIN_INTERRUPT_OVERRUN_ERROR);
            DL_UART_Extend_receiveData(LIN_INST);
            break;
        }
        case DL_UART_EXTEND_IIDX_BREAK_ERROR:
        {
            NVIC_ClearPendingIRQ(LIN_INST_INT_IRQN);
            DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_MAIN_INTERRUPT_BREAK_ERROR);
            break;
        }
        case DL_UART_EXTEND_INTERRUPT_RX_TIMEOUT_ERROR:
        {
            NVIC_ClearPendingIRQ(LIN_INST_INT_IRQN);
            DL_UART_Extend_clearInterruptStatus(LIN_INST, DL_UART_EXTEND_INTERRUPT_RX_TIMEOUT_ERROR);
            break;
        }
        default:
        {
            DL_UART_Extend_receiveData(LIN_INST);
            NVIC_ClearPendingIRQ(LIN_INST_INT_IRQN);
            break;
        }
    }    
            /*if (current_pid == 8) // Calibration
            {
                temp_raw = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
                temp = calcTempx10(temp_raw);

                tempCal = tempCal + temp;
                flagSaveTempCal = true;
            }*/
    
}
