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

void telemetryInit(void) {
    gCurrentMinTemp = gActiveExtremes.minTemp;
    gCurrentMaxTemp = gActiveExtremes.maxTemp;
    gLastCommittedMinTemp = gCurrentMinTemp;
    gLastCommittedMaxTemp = gCurrentMaxTemp;
    settlingTimer = 0;
    needsCommit = false;
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
        int16_t minDelta = gLastCommittedMinTemp - gCurrentMinTemp;
        int16_t maxDelta = gCurrentMaxTemp - gLastCommittedMaxTemp;
        
        if (minDelta >= EXTREME_SIGNIFICANT_DELTA_X10 || maxDelta >= EXTREME_SIGNIFICANT_DELTA_X10) {
            // Immediate commit
            ExtremesBlock_t newExt;
            newExt.minTemp = gCurrentMinTemp;
            newExt.maxTemp = gCurrentMaxTemp;
            if (configSaveExtremes(&newExt)) {
                gLastCommittedMinTemp = gCurrentMinTemp;
                gLastCommittedMaxTemp = gCurrentMaxTemp;
                needsCommit = false;
            }
        }
    }
}

void telemetryTick(void) {
    if (needsCommit) {
        // The settling timer acts as a debounce mechanism. Minor extreme changes are held 
        // in RAM until the temperature stabilizes, preventing excessive flash erase cycles.
        settlingTimer++;
        if (settlingTimer >= SETTLING_TIME_SEC) {
            ExtremesBlock_t newExt;
            newExt.minTemp = gCurrentMinTemp;
            newExt.maxTemp = gCurrentMaxTemp;
            if (configSaveExtremes(&newExt)) {
                gLastCommittedMinTemp = gCurrentMinTemp;
                gLastCommittedMaxTemp = gCurrentMaxTemp;
                needsCommit = false;
            } else {
                // If save fails (e.g. flash controller busy), we back off by 1 second 
                // and try again, ensuring the extreme is eventually committed.
                settlingTimer = SETTLING_TIME_SEC - 1;
            }
        }
    }
}
