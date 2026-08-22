#include "telemetry.h"
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
    gCurrentMinTemp = gActiveExtremes.min_temp;
    gCurrentMaxTemp = gActiveExtremes.max_temp;
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

        // Check if delta is significant enough to commit immediately
        int16_t minDelta = gLastCommittedMinTemp - gCurrentMinTemp;
        int16_t maxDelta = gCurrentMaxTemp - gLastCommittedMaxTemp;
        
        if (minDelta >= EXTREME_SIGNIFICANT_DELTA_X10 || maxDelta >= EXTREME_SIGNIFICANT_DELTA_X10) {
            // Immediate commit
            ExtremesBlock_t newExt;
            newExt.min_temp = gCurrentMinTemp;
            newExt.max_temp = gCurrentMaxTemp;
            configSaveExtremes(&newExt);
            
            gLastCommittedMinTemp = gCurrentMinTemp;
            gLastCommittedMaxTemp = gCurrentMaxTemp;
            needsCommit = false;
        }
    }
}

void telemetryTick(void) {
    if (needsCommit) {
        settlingTimer++;
        if (settlingTimer >= SETTLING_TIME_SEC) {
            ExtremesBlock_t newExt;
            newExt.min_temp = gCurrentMinTemp;
            newExt.max_temp = gCurrentMaxTemp;
            configSaveExtremes(&newExt);
            
            gLastCommittedMinTemp = gCurrentMinTemp;
            gLastCommittedMaxTemp = gCurrentMaxTemp;
            needsCommit = false;
        }
    }
}
