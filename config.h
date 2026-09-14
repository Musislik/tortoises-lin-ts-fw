#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define FACTORY_BLOCK_ADDR  0x1400
#define CONFIG_BLOCK_ADDR   0x1800
#define EXTREMES_BLOCK_ADDR 0x1C00

#define FACTORY_MAGIC  0xFAFAFAFA
#define CONFIG_MAGIC   0xC0C0C0C0
#define EXTREMES_MAGIC 0xECECECEC

#define OFFSET_MV_DEFAULT 0xFFFF // Default maximum uint16_t which means default offset
#define GAIN_SENS_DEFAULT 0x0000 // Default 0 means default gain

// PIDs baseline
#define PID_GET_TEMP_DEFAULT   0x0A
#define PID_GET_CONFIG_DEFAULT 0x0B
#define PID_SET_CONFIG_DEFAULT 0x0C

/**
 * @brief Factory block structure for one-time programmed data.
 */
typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic;
    uint32_t factorySn;
} FactoryBlock_t;

/**
 * @brief User configuration block structure.
 */
typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic;
    uint32_t logicalNodeId;
    uint16_t offsetMv;
    uint16_t gainSens;
    uint8_t pidGetTemp;
    uint8_t pidGetConfig;
    uint8_t pidSetConfig;
    uint8_t filterHwAdc;
    uint8_t filterSwMode;
    uint8_t _padding[7]; // Pad to 24 bytes (multiple of 8 for Flash ECC)
} ConfigBlock_t;

/**
 * @brief Extremes block structure for recording min/max temperatures.
 */
typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic;
    int16_t minTemp;
    int16_t maxTemp;
} ExtremesBlock_t;

extern ConfigBlock_t gActiveConfig;
extern FactoryBlock_t gActiveFactory;
extern ExtremesBlock_t gActiveExtremes;

/**
 * @brief Initializes the configuration blocks from flash.
 */
void configInit(void);

/**
 * @brief Saves the user configuration block to flash.
 * @param newConfig Pointer to the new configuration data.
 * @return true if successful, false otherwise.
 */
bool configSaveUser(const ConfigBlock_t *newConfig);

/**
 * @brief Saves the extremes block to flash.
 * @param newExtremes Pointer to the new extremes data.
 * @return true if successful, false otherwise.
 */
bool configSaveExtremes(const ExtremesBlock_t *newExtremes);

#endif // CONFIG_H
