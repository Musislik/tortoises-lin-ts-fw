#include "adc.h"
#include "config.h"

static const DL_ADC12_ClockConfig gADC12_0ClockConfig = {
    .clockSel       = DL_ADC12_CLOCK_SYSOSC,
    .divideRatio    = DL_ADC12_CLOCK_DIVIDE_1,
    .freqRange      = DL_ADC12_CLOCK_FREQ_RANGE_20_TO_24,
};

void adcReconfigure(uint8_t hwConfig)
{
    DL_ADC12_disableConversions(ADC12_0_INST);

    uint8_t sampleTimeBits = (hwConfig >> 4) & 0x0F;
    uint8_t hwAvgBits = hwConfig & 0x0F;

    uint32_t sampleTime = 512;
    switch (sampleTimeBits) {
        case 0x0: sampleTime = 32; break;
        case 0x1: sampleTime = 64; break;
        case 0x2: sampleTime = 128; break;
        case 0x3: sampleTime = 256; break;
        case 0x4: sampleTime = 512; break;
        default: sampleTime = 512; break;
    }
    DL_ADC12_setSampleTime0(ADC12_0_INST, sampleTime);

    uint32_t hwAvgNum = DL_ADC12_HW_AVG_NUM_ACC_32;
    uint32_t hwAvgDiv = DL_ADC12_HW_AVG_DEN_DIV_BY_32;
    switch (hwAvgBits) {
        case 0x0: hwAvgNum = DL_ADC12_HW_AVG_NUM_ACC_DISABLED; hwAvgDiv = DL_ADC12_HW_AVG_DEN_DIV_BY_1; break;
        case 0x1: hwAvgNum = DL_ADC12_HW_AVG_NUM_ACC_4; hwAvgDiv = DL_ADC12_HW_AVG_DEN_DIV_BY_4; break;
        case 0x2: hwAvgNum = DL_ADC12_HW_AVG_NUM_ACC_8; hwAvgDiv = DL_ADC12_HW_AVG_DEN_DIV_BY_8; break;
        case 0x3: hwAvgNum = DL_ADC12_HW_AVG_NUM_ACC_16; hwAvgDiv = DL_ADC12_HW_AVG_DEN_DIV_BY_16; break;
        case 0x4: hwAvgNum = DL_ADC12_HW_AVG_NUM_ACC_32; hwAvgDiv = DL_ADC12_HW_AVG_DEN_DIV_BY_32; break;
        case 0x5: hwAvgNum = DL_ADC12_HW_AVG_NUM_ACC_64; hwAvgDiv = DL_ADC12_HW_AVG_DEN_DIV_BY_64; break;
        case 0x6: hwAvgNum = DL_ADC12_HW_AVG_NUM_ACC_128; hwAvgDiv = DL_ADC12_HW_AVG_DEN_DIV_BY_128; break;
        default: hwAvgNum = DL_ADC12_HW_AVG_NUM_ACC_32; hwAvgDiv = DL_ADC12_HW_AVG_DEN_DIV_BY_32; break;
    }

    if (hwAvgBits == 0x0) {
        // Just set to 1 and disable averaging mode in configConversionMem if needed, but the API expects _DISABLED.
        // Actually the enum for disabled might be DL_ADC12_HW_AVG_NUM_ACC_DISABLED, or 0.
        // Let's use DL_ADC12_HW_AVG_NUM_ACC_DISABLED.
    }
    DL_ADC12_configHwAverage(ADC12_0_INST, hwAvgNum, hwAvgDiv);

    DL_ADC12_enableConversions(ADC12_0_INST);
}

void adcInit()
{
    DL_ADC12_reset(ADC12_0_INST);
    DL_ADC12_enablePower(ADC12_0_INST);

    delay_cycles(1000);

    // PIN MUX
    DL_GPIO_initPeripheralAnalogFunction(IOMUX_PINCM21);

    // ADC
    DL_ADC12_setClockConfig(ADC12_0_INST, (DL_ADC12_ClockConfig *) &gADC12_0ClockConfig);
    DL_ADC12_initSingleSample(ADC12_0_INST,
        DL_ADC12_REPEAT_MODE_DISABLED, DL_ADC12_SAMPLING_SOURCE_AUTO, DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SAMP_CONV_RES_12_BIT, DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(ADC12_0_INST, ADC12_0_ADCMEM_0,
        DL_ADC12_INPUT_CHAN_6, DL_ADC12_REFERENCE_VOLTAGE_VDDA, DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0, DL_ADC12_AVERAGING_MODE_ENABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED, DL_ADC12_TRIGGER_MODE_AUTO_NEXT, DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    
    // Apply configuration from flash
    adcReconfigure(gActiveConfig.filterHwAdc);
}
