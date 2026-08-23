/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 */

#include "ti_msp_dl_config.h"
#include "defs.h"
#include "adc.h"
#include "config.h"
#include "filter.h"
#include "telemetry.h"

// Variables for LIN TX/RX
uint8_t txBuffer[LIN_TX_BUFFER_LEN] = {0};
volatile uint32_t txBufferIx = 0;
volatile uint32_t txBufferLen = 0;

uint8_t rxBuffer[LIN_RX_BUFFER_LEN] = {0};
volatile uint32_t rxBufferIx = 0;
volatile uint32_t expectedRxLen = 0;
volatile uint8_t activeRxPid = 0;

volatile linRxState_t linRxState = LIN_RX_STATE_INIT;

// SYSTICK for telemetry and ADC timing
volatile uint32_t systick_1ms_counter = 0;
volatile uint32_t systick_1s_counter = 0;
volatile uint32_t systick_100ms_counter = 0;
volatile bool tick_1s_flag = false;
volatile bool tick_100ms_flag = false;

// Pending config save from ISR
volatile bool gPendingConfigSave = false;
ConfigBlock_t gPendingConfig;

// Latest measured temperature available to LIN ISR
volatile int16_t gLatestTemperature = 0;

void SysTick_Handler(void) {
    systick_1ms_counter++;
    systick_1s_counter++;
    systick_100ms_counter++;

    if (systick_1s_counter >= 1000) {
        systick_1s_counter = 0;
        tick_1s_flag = true;
    }

    if (systick_100ms_counter >= 100) {
        systick_100ms_counter = 0;
        tick_100ms_flag = true;
    }
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

    delay_cycles(240000);

    NVIC_ClearPendingIRQ(LIN_INST_INT_IRQN);
    NVIC_EnableIRQ(LIN_INST_INT_IRQN);
    
    // Setup SysTick for 1ms
    SysTick_Config(CPUCLK_FREQ / 1000);

    __enable_irq();

    delay_cycles(240000);
}

int main(void)
{
    initHardware();

    // Start first conversion
    DL_ADC12_startConversion(ADC12_0_INST);

    while (1) {
        if (gPendingConfigSave) {
            __disable_irq();
            ConfigBlock_t newCfg = gPendingConfig;
            gPendingConfigSave = false;
            __enable_irq();

            configSaveUser(&newCfg);
            adcReconfigure(newCfg.filter_hw_adc);
            filterInit(); // Reset software filter
            
            // Start a new conversion after reconfiguring ADC
            DL_ADC12_startConversion(ADC12_0_INST);
        }

        if (tick_100ms_flag) {
            tick_100ms_flag = false;
            
            // Read ADC result of the previous 100ms cycle
            uint32_t adcTempVal = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
            
            uint32_t filteredAdc = filterProcess(adcTempVal, gActiveConfig.filter_sw_mode);
            int32_t temp = calcTemperature(filteredAdc, gActiveConfig.offset_mv, gActiveConfig.gain_sens);
            
            // Atomic update of global temp for LIN ISR
            __disable_irq();
            gLatestTemperature = (int16_t)temp;
            __enable_irq();

            // Run telemetry (will safely write Flash if needed)
            telemetryUpdate((int16_t)temp);

            // Start next ADC conversion for the next 100ms cycle
            DL_ADC12_startConversion(ADC12_0_INST);
        }

        if (tick_1s_flag) {
            tick_1s_flag = false;
            telemetryTick();
        }

        // Sleep safely (wake up on any interrupt)
        __WFI();
    }
}

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
                    if (rxByte == gActiveConfig.pid_get_temp)
                    {
                        linRxState = LIN_RX_STATE_IDLE;

                        // Fast reply using latest prepared temp
                        int16_t temp = gLatestTemperature;

                        // Little-endian
                        txBuffer[0] = (uint8_t)(temp & 0xFF);
                        txBuffer[1] = (uint8_t)((temp >> 8) & 0xFF);
                        txBuffer[2] = calcChecksum(rxByte, txBuffer, 2);

                        txBufferIx = 1;
                        txBufferLen = 3;

                        DL_UART_Extend_transmitData(LIN_INST, txBuffer[0]);
                        DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_TX);
                    }
                    else if (rxByte == gActiveConfig.pid_get_config)
                    {
                        linRxState = LIN_RX_STATE_IDLE;
                        
                        // Populate 13 bytes
                        txBuffer[0] = (uint8_t)(gActiveConfig.logical_node_id & 0xFF);
                        txBuffer[1] = (uint8_t)((gActiveConfig.logical_node_id >> 8) & 0xFF);
                        txBuffer[2] = (uint8_t)((gActiveConfig.logical_node_id >> 16) & 0xFF);
                        txBuffer[3] = (uint8_t)((gActiveConfig.logical_node_id >> 24) & 0xFF);
                        txBuffer[4] = (uint8_t)(gActiveConfig.offset_mv & 0xFF);
                        txBuffer[5] = (uint8_t)((gActiveConfig.offset_mv >> 8) & 0xFF);
                        txBuffer[6] = (uint8_t)(gActiveConfig.gain_sens & 0xFF);
                        txBuffer[7] = (uint8_t)((gActiveConfig.gain_sens >> 8) & 0xFF);
                        txBuffer[8] = gActiveConfig.pid_get_temp;
                        txBuffer[9] = gActiveConfig.pid_get_config;
                        txBuffer[10] = gActiveConfig.pid_set_config;
                        txBuffer[11] = gActiveConfig.filter_hw_adc;
                        txBuffer[12] = gActiveConfig.filter_sw_mode;
                        
                        txBuffer[13] = (uint8_t)(gActiveFactory.factory_sn & 0xFF);
                        txBuffer[14] = (uint8_t)((gActiveFactory.factory_sn >> 8) & 0xFF);
                        txBuffer[15] = (uint8_t)((gActiveFactory.factory_sn >> 16) & 0xFF);
                        txBuffer[16] = (uint8_t)((gActiveFactory.factory_sn >> 24) & 0xFF);
                        
                        txBuffer[17] = calcChecksum(rxByte, txBuffer, 17);
                        
                        txBufferIx = 1;
                        txBufferLen = 18;
                        
                        DL_UART_Extend_transmitData(LIN_INST, txBuffer[0]);
                        DL_UART_Extend_enableInterrupt(LIN_INST, DL_UART_EXTEND_INTERRUPT_TX);
                    }
                    else if (rxByte == gActiveConfig.pid_set_config)
                    {
                        // We need to receive 13 bytes + checksum
                        linRxState = LIN_RX_STATE_RX_DATA;
                        rxBufferIx = 0;
                        expectedRxLen = 14; // 13 data + 1 cs
                        activeRxPid = rxByte;
                    }
                    else
                    {
                        linRxState = LIN_RX_STATE_IDLE;
                    }
                    break;
                    
                case LIN_RX_STATE_RX_DATA:
                    rxBuffer[rxBufferIx++] = rxByte;
                    if (rxBufferIx >= expectedRxLen) {
                        linRxState = LIN_RX_STATE_IDLE;
                        
                        // Validate checksum
                        uint8_t cs = calcChecksum(activeRxPid, rxBuffer, 13);
                        if (cs == rxBuffer[13]) {
                            // Valid frame, parse it
                            ConfigBlock_t newConfig = gActiveConfig;
                            newConfig.logical_node_id = ((uint32_t)rxBuffer[3] << 24) | ((uint32_t)rxBuffer[2] << 16) | ((uint32_t)rxBuffer[1] << 8) | rxBuffer[0];
                            newConfig.offset_mv = ((uint16_t)rxBuffer[5] << 8) | rxBuffer[4];
                            newConfig.gain_sens = ((uint16_t)rxBuffer[7] << 8) | rxBuffer[6];
                            newConfig.pid_get_temp = rxBuffer[8];
                            newConfig.pid_get_config = rxBuffer[9];
                            newConfig.pid_set_config = rxBuffer[10];
                            newConfig.filter_hw_adc = rxBuffer[11];
                            newConfig.filter_sw_mode = rxBuffer[12];
                            
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
