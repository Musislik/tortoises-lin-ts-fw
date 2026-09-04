#include "config.h"
#include "ti_msp_dl_config.h"
#include <string.h>

ConfigBlock_t gActiveConfig = {
    .magic = CONFIG_MAGIC,
    .logical_node_id = 1,
    .offset_mv = OFFSET_MV_DEFAULT,
    .gain_sens = GAIN_SENS_DEFAULT,
    .pid_get_temp = PID_GET_TEMP_DEFAULT,
    .pid_get_config = PID_GET_CONFIG_DEFAULT,
    .pid_set_config = PID_SET_CONFIG_DEFAULT,
    .filter_hw_adc = 0,
    .filter_sw_mode = 0
};
FactoryBlock_t gActiveFactory = {
    .magic = FACTORY_MAGIC,
    .factory_sn = 0x12345678
};
ExtremesBlock_t gActiveExtremes = {0};

void configInit(void) {
    // Check Factory block
    FactoryBlock_t *fb = (FactoryBlock_t *)FACTORY_BLOCK_ADDR;
    if (fb->magic == FACTORY_MAGIC) {
        memcpy(&gActiveFactory, fb, sizeof(FactoryBlock_t));
    } else {
        gActiveFactory.magic = FACTORY_MAGIC;
        gActiveFactory.factory_sn = 0xFFFFFFFF; // Unknown
    }

    // Check Config block
    ConfigBlock_t *cb = (ConfigBlock_t *)CONFIG_BLOCK_ADDR;
    if (cb->magic == CONFIG_MAGIC) {
        memcpy(&gActiveConfig, cb, sizeof(ConfigBlock_t));
    } else {
        // Load defaults
        gActiveConfig.magic = CONFIG_MAGIC;
        gActiveConfig.logical_node_id = 1;
        gActiveConfig.offset_mv = OFFSET_MV_DEFAULT;
        gActiveConfig.gain_sens = GAIN_SENS_DEFAULT;
        gActiveConfig.pid_get_temp = PID_GET_TEMP_DEFAULT;
        gActiveConfig.pid_get_config = PID_GET_CONFIG_DEFAULT;
        gActiveConfig.pid_set_config = PID_SET_CONFIG_DEFAULT;
        gActiveConfig.filter_hw_adc = 0; // Default HW filter
        gActiveConfig.filter_sw_mode = 0; // Default SW filter
        memset(gActiveConfig._padding, 0xFF, sizeof(gActiveConfig._padding));
    }

    // Check Extremes block
    ExtremesBlock_t *eb = (ExtremesBlock_t *)EXTREMES_BLOCK_ADDR;
    if (eb->magic == EXTREMES_MAGIC) {
        memcpy(&gActiveExtremes, eb, sizeof(ExtremesBlock_t));
    } else {
        gActiveExtremes.magic = EXTREMES_MAGIC;
        gActiveExtremes.min_temp = 0x7FFF;
        gActiveExtremes.max_temp = (int16_t)0x8000;
    }
}

bool configSaveUser(const ConfigBlock_t *newConfig) {
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, CONFIG_BLOCK_ADDR, DL_FLASHCTL_REGION_SELECT_MAIN);

    DL_FLASHCTL_COMMAND_STATUS status = DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, CONFIG_BLOCK_ADDR, DL_FLASHCTL_COMMAND_SIZE_SECTOR);
    if (status != DL_FLASHCTL_COMMAND_STATUS_PASSED) {
        return false;
    }

    // Copy to active config to apply immediately
    memcpy(&gActiveConfig, newConfig, sizeof(ConfigBlock_t));
    gActiveConfig.magic = CONFIG_MAGIC; // Ensure magic is right

    // Program 64-bit blocks
    uint32_t *dataPtr = (uint32_t *)&gActiveConfig;
    for (int i = 0; i < sizeof(ConfigBlock_t) / 8; i++) {
        status = DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(FLASHCTL, CONFIG_BLOCK_ADDR + (i * 8), &dataPtr[i * 2]);
        if (status != DL_FLASHCTL_COMMAND_STATUS_PASSED) {
            return false;
        }
    }
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    return true;
}

bool configSaveExtremes(const ExtremesBlock_t *newExtremes) {
    DL_FlashCTL_executeClearStatus(FLASHCTL);
    DL_FlashCTL_unprotectSector(FLASHCTL, EXTREMES_BLOCK_ADDR, DL_FLASHCTL_REGION_SELECT_MAIN);

    DL_FLASHCTL_COMMAND_STATUS status = DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, EXTREMES_BLOCK_ADDR, DL_FLASHCTL_COMMAND_SIZE_SECTOR);
    if (status != DL_FLASHCTL_COMMAND_STATUS_PASSED) {
        return false;
    }

    memcpy(&gActiveExtremes, newExtremes, sizeof(ExtremesBlock_t));
    gActiveExtremes.magic = EXTREMES_MAGIC;

    uint32_t *dataPtr = (uint32_t *)&gActiveExtremes;
    status = DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(FLASHCTL, EXTREMES_BLOCK_ADDR, dataPtr);
    
    return status == DL_FLASHCTL_COMMAND_STATUS_PASSED;
}
