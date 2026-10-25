#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CONFIG_BLOCK_ADDR   0x1C00
#define CONFIG_MAGIC        0xCFCFCF01

#define OFFSET_MV_DEFAULT   0x01F40000  // Default offset for TMP235 (500.0 mV in Q16.16)
#define GAIN_SENS_DEFAULT   0x000A0000U // Default gain for TMP235 (10.0 mV/degC in UQ16.16)

// PIDs baseline
#define PID_GET_TEMP_DEFAULT   0x1D     // dec30
#define PID_GET_CONFIG_DEFAULT 0x1E
#define PID_SET_CONFIG_DEFAULT 0x1F

/**
 * @brief User configuration block structure with merged operational extremes.
 */
typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic;
    uint32_t logicalNodeId;
    int32_t  offsetMv;
    uint32_t gainSens;
    uint8_t  pidGetTemp;
    uint8_t  pidGetConfig;
    uint8_t  pidSetConfig;
    uint8_t  filterHwAdc;
    uint8_t  filterSwMode;
    int16_t  minTemp;
    int16_t  maxTemp;
    uint8_t  _padding[3]; // Pad to maintain 32-bit alignment for CRC
    uint32_t crc;         // CRC-32 integrity checksum over preceding bytes
} ConfigBlock_t;

_Static_assert(sizeof(ConfigBlock_t) == 32, "ConfigBlock_t must be exactly 32 bytes");
_Static_assert(offsetof(ConfigBlock_t, crc) == 28, "ConfigBlock_t crc must be at offset 28");

extern ConfigBlock_t gActiveConfig;

/**
 * @brief Initializes the configuration block from flash.
 *
 * This routine verifies the integrity of the unified configuration block using
 * the magic number. If the block is invalid or empty, default values including
 * initial temperature extremes are loaded.
 */
void configInit(void);

/**
 * @brief Saves the unified configuration block to flash.
 *
 * @param newConfig Pointer to the configuration data to write.
 * @return true if successful, false otherwise.
 */
bool configSave(const ConfigBlock_t *newConfig);

#define configSaveUser configSave

#endif // CONFIG_H
