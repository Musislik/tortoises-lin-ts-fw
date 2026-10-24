#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "ti_msp_dl_config.h"

#define LOGICAL_NODE_ID_DEFAULT 1
#define FILTER_HW_ADC_DEFAULT   0
#define FILTER_SW_MODE_DEFAULT  0
#define MIN_TEMP_DEFAULT        0x7FFF
#define MAX_TEMP_DEFAULT        ((int16_t)0x8000)
#define FLASH_BLOCK_SIZE_BYTES  8
#define FLASH_WORDS_PER_BLOCK   2

ConfigBlock_t gActiveConfig = {0};

void configInit(void) {
    ConfigBlock_t *cb = (ConfigBlock_t *)CONFIG_BLOCK_ADDR;
    if (cb->magic == CONFIG_MAGIC) {
        memcpy(&gActiveConfig, cb, sizeof(ConfigBlock_t));
    } else {
        // Load defaults as a safeguard against uninitialized or corrupted flash memory.
        // This ensures the device remains fully operational and doesn't brick itself on the first boot.
        gActiveConfig.magic = CONFIG_MAGIC;
        gActiveConfig.logicalNodeId = LOGICAL_NODE_ID_DEFAULT;
        gActiveConfig.offsetMv = OFFSET_MV_DEFAULT;
        gActiveConfig.gainSens = GAIN_SENS_DEFAULT;
        gActiveConfig.pidGetTemp = PID_GET_TEMP_DEFAULT;
        gActiveConfig.pidGetConfig = PID_GET_CONFIG_DEFAULT;
        gActiveConfig.pidSetConfig = PID_SET_CONFIG_DEFAULT;
        gActiveConfig.filterHwAdc = FILTER_HW_ADC_DEFAULT;
        gActiveConfig.filterSwMode = FILTER_SW_MODE_DEFAULT;
        gActiveConfig.minTemp = MIN_TEMP_DEFAULT;
        gActiveConfig.maxTemp = MAX_TEMP_DEFAULT;
        memset(gActiveConfig._padding, 0xFF, sizeof(gActiveConfig._padding));

        // Persist default configuration to flash on first boot so that subsequent
        // boots find a valid magic signature and the sector is permanently provisioned.
        configSave(&gActiveConfig);
    }
}

bool configSave(const ConfigBlock_t *newConfig) {
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, CONFIG_BLOCK_ADDR, DL_FLASHCTL_REGION_SELECT_MAIN);

    DL_FLASHCTL_COMMAND_STATUS status = DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, CONFIG_BLOCK_ADDR, DL_FLASHCTL_COMMAND_SIZE_SECTOR);
    if (status != DL_FLASHCTL_COMMAND_STATUS_PASSED) {
        return false;
    }

    // Copy to active config to apply immediately.
    // We update the active RAM struct before programming flash so that the system immediately 
    // uses the new parameters (e.g. LIN PIDs) even while the slower flash write operation is ongoing.
    memcpy(&gActiveConfig, newConfig, sizeof(ConfigBlock_t));
    gActiveConfig.magic = CONFIG_MAGIC; // Ensure magic is right

    // Program 64-bit blocks
    uint32_t *dataPtr = (uint32_t *)&gActiveConfig;
    for (int i = 0; i < sizeof(ConfigBlock_t) / FLASH_BLOCK_SIZE_BYTES; i++) {
        status = DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(FLASHCTL, CONFIG_BLOCK_ADDR + (i * FLASH_BLOCK_SIZE_BYTES), &dataPtr[i * FLASH_WORDS_PER_BLOCK]);
        if (status != DL_FLASHCTL_COMMAND_STATUS_PASSED) {
            return false;
        }
    }
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    return true;
}
