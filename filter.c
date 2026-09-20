#include "filter.h"
#include <stdint.h>
#include <stdbool.h>

#define MA_SAMPLES_4  4
#define MA_SAMPLES_8  8
#define MA_SAMPLES_16 16
#define MAX_MA_SAMPLES MA_SAMPLES_16
#define HW_VOLTAGE_MIN_MV 50.0f
#define HW_VOLTAGE_MAX_MV 3250.0f
#define TEMP_MIN_CX10 -400
#define TEMP_MAX_CX10 1500

// State for spike rejection and HW validation
typedef enum {
    FILTER_STATE_INIT,
    FILTER_STATE_LOCKED
} FilterLockState_t;

// Config variables
static FilterSwMode_t config = FILTER_SW_MODE_PASSTHROUGH;
static uint16_t calOffsetMv = 0;
static float calGainSens = 0.0f;

static FilterLockState_t lockState = FILTER_STATE_INIT;
static uint8_t startupCount = 0;
static uint8_t errorCount = 0;
static int16_t lastValidRaw = TEMP_INVALID_VALUE;
static int16_t lastFilteredOut = TEMP_INVALID_VALUE;

// Ring buffer for moving average (up to 16 samples)
static int16_t maBuffer[MAX_MA_SAMPLES];
static uint8_t maIndex = 0;
static bool maFilled = false;

// State for EMA
static int16_t emaState = 0;
static bool emaInitialized = false;


static bool isVoltageValid(const float voltageMv) {
    if (voltageMv >= HW_VOLTAGE_MIN_MV && voltageMv <= HW_VOLTAGE_MAX_MV) {
        return true;
    }
    return false;
}

void filterInit(void) {
    maIndex = 0;
    maFilled = false;
    emaInitialized = false;
    for (int i = 0; i < MAX_MA_SAMPLES; i++) {
        maBuffer[i] = 0;
    }

    lockState = FILTER_STATE_INIT;
    startupCount = 0;
    errorCount = 0;
    lastValidRaw = TEMP_INVALID_VALUE;
    lastFilteredOut = TEMP_INVALID_VALUE;
}

void setFilterConfig(const FilterSwMode_t swFilterConfig, const uint16_t offsetMv, const float gainSens) {
    if (calOffsetMv != offsetMv || config != swFilterConfig || 
        (calGainSens - gainSens > 0.0001f || calGainSens - gainSens < -0.0001f)) 
    {
        filterInit();
        calOffsetMv = offsetMv;
        calGainSens = gainSens;
        config = swFilterConfig;
    }
}

static int16_t filterProcess(int16_t rawTempCx10) {    
    if (config == FILTER_SW_MODE_PASSTHROUGH) {
        // Passthrough is primarily used for factory calibration or raw hardware diagnostics 
        // where unadulterated readings are strictly required.
        return rawTempCx10;
    }
    
    if (lockState == FILTER_STATE_INIT) {
        if (rawTempCx10 == TEMP_INVALID_VALUE) {
            startupCount = 0;
            return TEMP_INVALID_VALUE;
        }
        
        if (startupCount == 0) {
            lastValidRaw = rawTempCx10;
            startupCount = 1;
        } else {
            int32_t diff = rawTempCx10 - lastValidRaw;
            if (diff < 0) {
                diff = -diff;
            }
            
            if (diff <= 200) { // 20.0 deg C jump limit
                startupCount++;
                lastValidRaw = rawTempCx10;
                if (startupCount >= 3) {
                    lockState = FILTER_STATE_LOCKED;
                    errorCount = 0;
                    
                    maIndex = 0;
                    maFilled = false;
                    emaInitialized = false;
                }
            } else {
                lastValidRaw = rawTempCx10;
                startupCount = 1;
            }
        }
        
        if (lockState == FILTER_STATE_INIT) {
            return TEMP_INVALID_VALUE;
        }
    } else {
        bool isValid = true;
        if (rawTempCx10 == TEMP_INVALID_VALUE) {
            isValid = false;
        } else {
            int32_t diff = rawTempCx10 - lastValidRaw;
            if (diff < 0) {
                diff = -diff;
            }
            if (diff > 200) {
                isValid = false;
            }
        }
        
        if (!isValid) {
            errorCount++;
            if (errorCount >= 5) {
                lockState = FILTER_STATE_INIT;
                startupCount = 0;
                if (rawTempCx10 != TEMP_INVALID_VALUE) {
                    lastValidRaw = rawTempCx10;
                    startupCount = 1;
                }
                return TEMP_INVALID_VALUE;
            }
            return lastFilteredOut;
        }
        
        errorCount = 0;
        lastValidRaw = rawTempCx10;
    }

    // EMA Modes
    if ((config >= FILTER_SW_MODE_EMA_1) && (config <= FILTER_SW_MODE_EMA_3)) {
        if (!emaInitialized) {
            emaState = rawTempCx10;
            emaInitialized = true;
            lastFilteredOut = emaState;
            return emaState;
        }
        
        // config == 0x04 -> alpha = 0.5 (shift 1)
        // config == 0x05 -> alpha = 0.25 (shift 2)
        // config == 0x06 -> alpha = 0.125 (shift 3)
        uint8_t shift = (config - FILTER_SW_MODE_MA_16); 
        
        // EMA: y_n = y_{n-1} + alpha * (x_n - y_{n-1})
        // y_n = y_{n-1} + (x_n - y_{n-1}) / (1 << shift)
        // Integer division ensures correct rounding towards zero for negative differences.
        // The compiler optimizes this operation into a safe arithmetic shift.
        
        int32_t diff = rawTempCx10 - emaState;
        int32_t adjustment = diff / (1 << shift);
        emaState = emaState + adjustment;
        lastFilteredOut = emaState;
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
        lastFilteredOut = rawTempCx10;
        return rawTempCx10; // Edge case
    }
    
    for (uint8_t i = 0; i < count; i++) {
        sum += maBuffer[i];
    }
    
    lastFilteredOut = sum / (int32_t)count;
    return lastFilteredOut;
}


int16_t calcTemperature(const float voltageMv) {
    if (calGainSens < 0.0001f && calGainSens > -0.0001f) {
        return TEMP_INVALID_VALUE;
    }

    int16_t tempCx10 = TEMP_INVALID_VALUE;
    
    if (!isVoltageValid(voltageMv)) {
        tempCx10 = TEMP_INVALID_VALUE;
    } else {
        float diffMv = voltageMv - calOffsetMv;
        float tempCx10_f = diffMv / calGainSens;

        if (tempCx10_f < (float)TEMP_MIN_CX10 || tempCx10_f > (float)TEMP_MAX_CX10) {
            tempCx10 = TEMP_INVALID_VALUE;
        } else {
            tempCx10 = (int16_t)(tempCx10_f + (tempCx10_f >= 0.0f ? 0.5f : -0.5f));
        }
    }

    return filterProcess(tempCx10);
}
