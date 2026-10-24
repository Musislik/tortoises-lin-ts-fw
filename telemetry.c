#include "telemetry.h"
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define EXTREME_SIGNIFICANT_DELTA_X10 50 // 5.0 deg C
#define SETTLING_TIME_SEC 3600 // 1 hour

int16_t gCurrentMinTemp = 0;
int16_t gCurrentMaxTemp = 0;

static int16_t gLastCommittedMinTemp = 0;
static int16_t gLastCommittedMaxTemp = 0;

static uint32_t settlingTimer = 0;
static bool needsCommit = false;

static bool commitExtremes(void) {
    ConfigBlock_t newConfig = gActiveConfig;
    newConfig.minTemp = gCurrentMinTemp;
    newConfig.maxTemp = gCurrentMaxTemp;

    if (configSave(&newConfig)) {
        gLastCommittedMinTemp = gCurrentMinTemp;
        gLastCommittedMaxTemp = gCurrentMaxTemp;
        needsCommit = false;
        return true;
    }
    return false;
}

void telemetryInit(void) {
    gCurrentMinTemp = gActiveConfig.minTemp;
    gCurrentMaxTemp = gActiveConfig.maxTemp;
    gLastCommittedMinTemp = gCurrentMinTemp;
    gLastCommittedMaxTemp = gCurrentMaxTemp;
    settlingTimer = 0;
    needsCommit = false;
}

void telemetryNotifyConfigSaved(void) {
    gLastCommittedMinTemp = gCurrentMinTemp;
    gLastCommittedMaxTemp = gCurrentMaxTemp;
    needsCommit = false;
    settlingTimer = 0;
}

void telemetryUpdate(int16_t newTemp) {
    bool recordBroken = false;

    if (newTemp < gCurrentMinTemp) {
        gCurrentMinTemp = newTemp;
        recordBroken = true;
    }
    
    if (newTemp > gCurrentMaxTemp) {
        gCurrentMaxTemp = newTemp;
        recordBroken = true;
    }

    if (recordBroken) {
        settlingTimer = 0; // Reset settling timer
        needsCommit = true;

        // Check if delta is significant enough to commit immediately.
        // We bypass the settling timer for large temperature jumps to guarantee that severe thermal 
        // events are permanently recorded in flash even if the device loses power abruptly right after.
        int32_t minDelta = (int32_t)gLastCommittedMinTemp - (int32_t)gCurrentMinTemp;
        int32_t maxDelta = (int32_t)gCurrentMaxTemp - (int32_t)gLastCommittedMaxTemp;
        
        if (minDelta >= EXTREME_SIGNIFICANT_DELTA_X10 || maxDelta >= EXTREME_SIGNIFICANT_DELTA_X10) {
            // Immediate commit
            commitExtremes();
        }
    }
}

void telemetryTick(void) {
    if (needsCommit) {
        // The settling timer acts as a debounce mechanism. Minor extreme changes are held 
        // in RAM until the temperature stabilizes, preventing excessive flash erase cycles.
        settlingTimer++;
        if (settlingTimer >= SETTLING_TIME_SEC) {
            if (commitExtremes()) {
                // Extremes committed successfully
            } else {
                // If save fails (e.g. flash controller busy), we back off by 1 second 
                // and try again, ensuring the extreme is eventually committed.
                settlingTimer = SETTLING_TIME_SEC - 1;
            }
        }
    }
}
