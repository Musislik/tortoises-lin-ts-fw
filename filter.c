#include "filter.h"
#include <stdint.h>
#include <stdbool.h>

#define ADC_VREF_MV 3300
#define ADC_MAX_VAL 4095
#define TEMP_OFFSET_MV 500

#define FILTER_SW_MODE_PASSTHROUGH 0x00
#define FILTER_SW_MODE_MA_4        0x01
#define FILTER_SW_MODE_MA_8        0x02
#define FILTER_SW_MODE_MA_16       0x03
#define FILTER_SW_MODE_EMA_1       0x04
#define FILTER_SW_MODE_EMA_3       0x06

#define MA_SAMPLES_4  4
#define MA_SAMPLES_8  8
#define MA_SAMPLES_16 16

#define TEMP_MULTIPLIER        100

// Ring buffer for moving average (up to 16 samples)
#define MAX_MA_SAMPLES 16
static uint32_t maBuffer[MAX_MA_SAMPLES];
static uint8_t maIndex = 0;
static bool maFilled = false;

// State for EMA
static uint32_t emaState = 0;
static bool emaInitialized = false;

void filterInit(void) {
    maIndex = 0;
    maFilled = false;
    emaInitialized = false;
    for (int i = 0; i < MAX_MA_SAMPLES; i++) {
        maBuffer[i] = 0;
    }
}

uint32_t filterProcess(uint32_t rawAdc, uint8_t swFilterConfig) {
    if (swFilterConfig == FILTER_SW_MODE_PASSTHROUGH) {
        // Passthrough is primarily used for factory calibration or raw hardware diagnostics 
        // where unadulterated ADC readings are strictly required.
        return rawAdc;
    }
    
    // EMA Modes
    if ((swFilterConfig >= FILTER_SW_MODE_EMA_1) && (swFilterConfig <= FILTER_SW_MODE_EMA_3)) {
        if (!emaInitialized) {
            emaState = rawAdc;
            emaInitialized = true;
            return emaState;
        }
        
        // swFilterConfig == 0x04 -> alpha = 0.5 (shift 1)
        // swFilterConfig == 0x05 -> alpha = 0.25 (shift 2)
        // swFilterConfig == 0x06 -> alpha = 0.125 (shift 3)
        uint8_t shift = (swFilterConfig - FILTER_SW_MODE_MA_16); 
        
        // EMA: y_n = y_{n-1} + alpha * (x_n - y_{n-1})
        // y_n = y_{n-1} + (x_n - y_{n-1}) >> shift
        // Bitwise right-shift is used here as a highly optimized substitute for floating-point division 
        // to calculate the alpha weighting on resource-constrained microcontrollers.
        
        int32_t diff = (int32_t)rawAdc - (int32_t)emaState;
        emaState = emaState + (diff >> shift);
        return emaState;
    }
    
    // Moving Average Modes (0x01: 4, 0x02: 8, 0x03: 16)
    uint8_t numSamples = MA_SAMPLES_4;
    if (swFilterConfig == FILTER_SW_MODE_MA_8) {
        numSamples = MA_SAMPLES_8;
    } else if (swFilterConfig == FILTER_SW_MODE_MA_16) {
        numSamples = MA_SAMPLES_16;
    }
    
    maBuffer[maIndex] = rawAdc;
    maIndex++;
    if (maIndex >= numSamples) {
        maIndex = 0;
        maFilled = true;
    }
    
    uint32_t sum = 0;
    uint8_t count = maFilled ? numSamples : maIndex;
    if (count == 0) {
        // Strict safeguard against division-by-zero during the very first execution cycle 
        // before the buffer has accumulated any elements.
        return rawAdc; // Edge case
    }
    
    for (uint8_t i = 0; i < count; i++) {
        sum += maBuffer[i];
    }
    
    return sum / count;
}

int32_t calcTemperature(uint32_t rawAdc, uint16_t offsetMv, float gainSens) {
    if (gainSens == 0.0f || ADC_MAX_VAL == 0) 
    {
        return INT32_MAX;
    }

    float voltageMv = (float)((float)rawAdc * ADC_VREF_MV) / ADC_MAX_VAL;
    
    float diffMv = voltageMv - offsetMv;
    
    float tempCx10_f = diffMv / gainSens;

    return (int32_t)(tempCx10_f + (tempCx10_f >= 0.0f ? 0.5f : -0.5f));
}
