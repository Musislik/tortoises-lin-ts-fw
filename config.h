#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define FACTORY_BLOCK_ADDR  0x1400
#define CONFIG_BLOCK_ADDR   0x1800
#define EXTREMES_BLOCK_ADDR 0x1C00

#define FACTORY_MAGIC  0xFAFAFAFA
#define CONFIG_MAGIC   0xC0C0C0C1
#define EXTREMES_MAGIC 0xECECECEC

#define OFFSET_MV_DEFAULT 0xFFFF // Default maximum uint16_t which means default offset
#define GAIN_SENS_DEFAULT 0x0000 // Default 0 means default gain

// PIDs baseline (+3 for testing Device 0 in Pyxis: 0x20, 0x21, 0x22)
#define PID_GET_TEMP_DEFAULT   0x20
#define PID_GET_CONFIG_DEFAULT 0x21
#define PID_SET_CONFIG_DEFAULT 0x22

typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic;
    uint32_t factory_sn;
} FactoryBlock_t;

typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic;
    uint32_t logical_node_id;
    uint16_t offset_mv;
    uint16_t gain_sens;
    uint8_t pid_get_temp;
    uint8_t pid_get_config;
    uint8_t pid_set_config;
    uint8_t filter_hw_adc;
    uint8_t filter_sw_mode;
    uint8_t _padding[7]; // Pad to 24 bytes (multiple of 8 for Flash ECC)
} ConfigBlock_t;

typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic;
    int16_t min_temp;
    int16_t max_temp;
} ExtremesBlock_t;

extern ConfigBlock_t gActiveConfig;
extern FactoryBlock_t gActiveFactory;
extern ExtremesBlock_t gActiveExtremes;

void configInit(void);
bool configSaveUser(const ConfigBlock_t *newConfig);
bool configSaveExtremes(const ExtremesBlock_t *newExtremes);

#endif // CONFIG_H
