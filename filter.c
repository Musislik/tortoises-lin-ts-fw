#include "filter.h"


#define ADC_VREF_MV 3300
#define ADC_MAX_VAL 4095
#define TEMP_OFFSET_MV 500

// Ring buffer for moving average (up to 16 samples)
#define MAX_MA_SAMPLES 16
static uint32_t ma_buffer[MAX_MA_SAMPLES];
static uint8_t ma_index = 0;
static bool ma_filled = false;

// State for EMA
static uint32_t ema_state = 0;
static bool ema_initialized = false;

void filterInit(void) {
    ma_index = 0;
    ma_filled = false;
    ema_initialized = false;
    for (int i = 0; i < MAX_MA_SAMPLES; i++) {
        ma_buffer[i] = 0;
    }
}

uint32_t filterProcess(uint32_t raw_adc, uint8_t sw_filter_config) {
    if (sw_filter_config == 0x00) {
        // Passthrough
        return raw_adc;
    }
    
    // EMA Modes
    if (sw_filter_config >= 0x04 && sw_filter_config <= 0x06) {
        if (!ema_initialized) {
            ema_state = raw_adc;
            ema_initialized = true;
            return ema_state;
        }
        
        // sw_filter_config == 0x04 -> alpha = 0.5 (shift 1)
        // sw_filter_config == 0x05 -> alpha = 0.25 (shift 2)
        // sw_filter_config == 0x06 -> alpha = 0.125 (shift 3)
        uint8_t shift = (sw_filter_config - 0x03); 
        
        // EMA: y_n = y_{n-1} + alpha * (x_n - y_{n-1})
        // y_n = y_{n-1} + (x_n - y_{n-1}) >> shift
        
        int32_t diff = (int32_t)raw_adc - (int32_t)ema_state;
        ema_state = ema_state + (diff >> shift);
        return ema_state;
    }
    
    // Moving Average Modes (0x01: 4, 0x02: 8, 0x03: 16)
    uint8_t num_samples = 4;
    if (sw_filter_config == 0x02) num_samples = 8;
    else if (sw_filter_config == 0x03) num_samples = 16;
    
    ma_buffer[ma_index] = raw_adc;
    ma_index++;
    if (ma_index >= num_samples) {
        ma_index = 0;
        ma_filled = true;
    }
    
    uint32_t sum = 0;
    uint8_t count = ma_filled ? num_samples : ma_index;
    if (count == 0) return raw_adc; // Edge case
    
    for (uint8_t i = 0; i < count; i++) {
        sum += ma_buffer[i];
    }
    
    return sum / count;
}

int32_t calcTemperature(uint32_t raw_adc, uint16_t offset_mV, uint16_t gain_sens) {
    int32_t voltage_mV = ((int32_t)raw_adc * ADC_VREF_MV) / ADC_MAX_VAL;
    
    int32_t offset_eff = (offset_mV == 0xFFFF) ? 500 : (int32_t)offset_mV;
    int32_t sens_eff = (gain_sens == 0) ? 100 : (int32_t)gain_sens;
    
    int32_t temp_C_x10 = ((voltage_mV - offset_eff) * 100) / sens_eff;
    
    return temp_C_x10;
}
