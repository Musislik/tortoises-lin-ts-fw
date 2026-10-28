#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "ti_msp_dl_config.h"

#define LOGICAL_NODE_ID_DEFAULT 1
#define FILTER_HW_ADC_DEFAULT   0
#define FILTER_SW_MODE_DEFAULT  0
#define FLASH_WORDS_TOTAL       (sizeof(ConfigBlock_t) / sizeof(uint32_t))

ConfigBlock_t gActiveConfig = {0};
static ConfigBlock_t stagedConfig = {0};

static uint32_t configCalculateCrc(const ConfigBlock_t *block) {
    if (block == NULL) {
        return 0;
    }

    const uint8_t *pData = (const uint8_t *)block;
    uint32_t crc = 0xFFFFFFFFU;

    for (size_t i = 0; i < offsetof(ConfigBlock_t, crc); i++) {
        crc ^= pData[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 1U) {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            } else {
                crc >>= 1U;
            }
        }
    }

    return ~crc;
}

void configInit(void) {
    ConfigBlock_t *cb = (ConfigBlock_t *)CONFIG_BLOCK_ADDR;
    if ((cb->magic == CONFIG_MAGIC) && (cb->crc == configCalculateCrc(cb))) {
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
        gActiveConfig.crc = configCalculateCrc(&gActiveConfig);

        // Persist default configuration to flash on first boot so that subsequent
        // boots find a valid magic signature and the sector is permanently provisioned.
        if (!configSave(&gActiveConfig)) {
            // Flash persistence failed on first boot; device remains on defaults in RAM
        }
    }
}

bool configSave(const ConfigBlock_t *pNewConfig) {
    if (pNewConfig == NULL) {
        return false;
    }

    // Prepare staged configuration block with valid magic, padding, and CRC-32
    stagedConfig = *pNewConfig;
    stagedConfig.magic = CONFIG_MAGIC;
    memset(stagedConfig._padding, 0xFF, sizeof(stagedConfig._padding));
    stagedConfig.crc = configCalculateCrc(&stagedConfig);

    // Save interrupt mask and disable interrupts to prevent bus stalls or HardFaults
    // on single-bank flash while erase and program operations are active.
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, CONFIG_BLOCK_ADDR, DL_FLASHCTL_REGION_SELECT_MAIN);

    DL_FLASHCTL_COMMAND_STATUS status = DL_FlashCTL_eraseMemoryFromRAM(
        FLASHCTL, CONFIG_BLOCK_ADDR, DL_FLASHCTL_COMMAND_SIZE_SECTOR);
    if (status != DL_FLASHCTL_COMMAND_STATUS_PASSED) {
        DL_FlashCTL_executeClearStatus(FLASHCTL);
        __set_PRIMASK(primask);
        return false;
    }

    // Program 32 bytes (8 words / 4 64-bit blocks) using DriverLib blocking API.
    // This API automatically clears status and unprotects the sector before every 64-bit block.
    status = DL_FlashCTL_programMemoryBlockingFromRAM64WithECCGenerated(
        FLASHCTL,
        CONFIG_BLOCK_ADDR,
        (uint32_t *)&stagedConfig,
        FLASH_WORDS_TOTAL,
        DL_FLASHCTL_REGION_SELECT_MAIN);

    DL_FlashCTL_executeClearStatus(FLASHCTL);

    bool isSuccess = (status == DL_FLASHCTL_COMMAND_STATUS_PASSED);
    if (isSuccess) {
        // Commit to active RAM structure while interrupts are still masked to prevent
        // torn reads in ISR.
        memcpy(&gActiveConfig, &stagedConfig, sizeof(ConfigBlock_t));
    }

    // Restore prior interrupt state safely (prevents premature unmasking during boot)
    __set_PRIMASK(primask);

    return isSuccess;
}
