#include "filter.h"
#include <stdint.h>
#include <stdbool.h>

#define MA_SAMPLES_4  4
#define MA_SAMPLES_8  8
#define MA_SAMPLES_16 16

// Config variables
static FilterSwMode_t config;

// Temperature validity
static const int32_t TEMP_VALID_RANGE_BOT = -20;

// Ring buffer for moving average (up to 16 samples)
#define MAX_MA_SAMPLES 16
static int32_t maBuffer[MAX_MA_SAMPLES];
static uint8_t maIndex = 0;
static bool maFilled = false;

// State for EMA
static int32_t emaState = 0;
static bool emaInitialized = false;

void filterInit(void) {
    maIndex = 0;
    maFilled = false;
    emaInitialized = false;
    for (int i = 0; i < MAX_MA_SAMPLES; i++) {
        maBuffer[i] = 0;
    }
    config = FILTER_SW_MODE_PASSTHROUGH;
}

static int32_t filterProcess(int32_t rawTempCx10) {    
    if (config == FILTER_SW_MODE_PASSTHROUGH) {
        // Passthrough is primarily used for factory calibration or raw hardware diagnostics 
        // where unadulterated readings are strictly required.
        return rawTempCx10;
    }
    
    // EMA Modes
    if ((config >= FILTER_SW_MODE_EMA_1) && (config <= FILTER_SW_MODE_EMA_3)) {
        if (!emaInitialized) {
            emaState = rawTempCx10;
            emaInitialized = true;
            return emaState;
        }
        
        // config == 0x04 -> alpha = 0.5 (shift 1)
        // config == 0x05 -> alpha = 0.25 (shift 2)
        // config == 0x06 -> alpha = 0.125 (shift 3)
        uint8_t shift = (config - FILTER_SW_MODE_MA_16); 
        
        // EMA: y_n = y_{n-1} + alpha * (x_n - y_{n-1})
        // y_n = y_{n-1} + (x_n - y_{n-1}) >> shift
        // Bitwise right-shift is used here as a highly optimized substitute for floating-point division 
        // to calculate the alpha weighting on resource-constrained microcontrollers.
        
        int32_t diff = rawTempCx10 - emaState;
        emaState = emaState + (diff >> shift);
        return emaState;
    }
    
    // Moving Average Modes (0x01: 4, 0x02: 8, 0x03: 16)
    uint8_t numSamples = MA_SAMPLES_4;
    if (config == FILTER_SW_MODE_MA_8) {
        numSamples = MA_SAMPLES_8;
    } else if (config == FILTER_SW_MODE_MA_16) {
        numSamples = MA_SAMPLES_16;
    }
    
    maBuffer[maIndex] = rawTempCx10;
    maIndex++;
    if (maIndex >= numSamples) {
        maIndex = 0;
        maFilled = true;
    }
    
    int32_t sum = 0;
    uint8_t count = maFilled ? numSamples : maIndex;
    if (count == 0) {
        // Strict safeguard against division-by-zero during the very first execution cycle 
        // before the buffer has accumulated any elements.
        return rawTempCx10; // Edge case
    }
    
    for (uint8_t i = 0; i < count; i++) {
        sum += maBuffer[i];
    }
    
    return sum / (int32_t)count;
}

int32_t calcTemperature(float voltageMv, uint16_t offsetMv, float gainSens) {
    if (gainSens == 0.0f) 
    {
        return INT32_MAX;
    }

    float voltageMv = (float)((float)rawAdc * ADC_VREF_MV) / ADC_MAX_VAL;
    
    float diffMv = voltageMv - offsetMv;
    
    float tempCx10_f = diffMv / gainSens;

    int32_t tempCx10 = (int32_t)(tempCx10_f + (tempCx10_f >= 0.0f ? 0.5f : -0.5f));

    return filterProcess(tempCx10);
}

void setSwFilterConfig(FilterSwMode_t swFilterConfig)
{
    config = swFilterConfig;
}