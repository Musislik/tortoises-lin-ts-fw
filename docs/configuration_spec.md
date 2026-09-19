# LIN Temperature Sensor - Configuration and Communication Specification

## 1. Overview & Architecture

This document defines the configuration parameters, memory layout, and LIN communication protocol for the MSPM0C1103-based LIN temperature sensor.

### Key Principles
- **LIN Protocol:** LIN 2.x physical/data layer with extended custom payload frames (allowing frames larger than 8 bytes for unified single-transaction configuration).
- **Checksum Model:** **Enhanced Checksum only** (PID included in the checksum calculation over the entire payload).
- **Endianness:** **Little-Endian (LSB first)** for all multi-byte values (`uint16_t`, `int16_t`, `uint32_t`).
- **Memory Architecture:** Parameters are structured into **three independent Flash memory blocks** (sectors) with 64-bit alignment and hardware ECC:
  1. **Factory Block (Sector 5, Read-Only):** Holds permanent manufacturing data (Factory Serial Number). Never erased or modified during runtime.
  2. **Configuration Block (Sector 6, Read/Write):** Holds user/network configuration (Logical Node ID, Calibration, Filtering, PIDs). Written only upon configuration command.
  3. **Telemetry & Extremes Block (Sector 7, Read/Write):** Holds historical operational statistics (Min/Max recorded temperatures). Managed dynamically in RAM and committed to Flash under specific hysteresis rules.

---

## 2. Parameter Definitions

| Parameter | Type | Endianness | Storage Block | Access via LIN | Description |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Magic Word** | `uint32_t` | Little-Endian | All Blocks | Internal | Block integrity & validation signature. |
| **Factory Serial Number (SN)** | `uint32_t` | Little-Endian | Factory | Read-Only | Permanent unique hardware ID burned during production. |
| **Logical Node ID** | `uint32_t` | Little-Endian | Config | Read / Write | 32-bit system position / role ID in the vehicle network. |
| **PID: Get Temperature** | `uint8_t` | N/A | Config | Read / Write | LIN Protected ID for temperature requests. |
| **PID: Get Configuration** | `uint8_t` | N/A | Config | Read / Write | LIN Protected ID for configuration requests. |
| **PID: Set Configuration** | `uint8_t` | N/A | Config | Read / Write | LIN Protected ID for configuration updates. |
| **ADC HW Config** | `uint8_t` | N/A | Config | Read / Write | Hardware ADC sample time & hardware accumulator averaging. |
| **SW Filter Config** | `uint8_t` | N/A | Config | Read / Write | Software digital filtering algorithm & window depth. |
| **Sensor Voltage Offset (`offset_mv`)** | `uint16_t` | Little-Endian | Config | Read / Write | Nominal voltage at $0^\circ\text{C}$ in $\text{mV}$. **`0xFFFF` = Default ($500\,\text{mV}$)**. |
| **Calibration Gain (`gain_sens`)** | `float` | Little-Endian IEEE 754 | Config | Read / Write | Sensor transfer curve sensitivity in $\text{mV}/^\circ\text{C}$. **`0.0f` = Default ($10.0\,\text{mV}/^\circ\text{C}$)**. |
| **Min Recorded Temperature** | `int16_t` | Little-Endian | Extremes | Read-Only | Lowest recorded temperature ($0.1^\circ\text{C}$). |
| **Max Recorded Temperature** | `int16_t` | Little-Endian | Extremes | Read-Only | Highest recorded temperature ($0.1^\circ\text{C}$). |

---

## 3. LIN Protocol & Frame Definitions

All multi-byte fields are transmitted in **Little-Endian** format (Low byte first).

```
[LIN Master] ────── Header (PID_GET_TEMP) ───────────> [Sensor]
             <───── Temperature (2B) + CS ──────────── [Sensor]

[LIN Master] ────── Header (PID_GET_CONFIG) ─────────> [Sensor]
             <───── Extended Config (17B) + CS ─────── [Sensor]

[LIN Master] ────── Header + New Config (13B) + CS ──> [Sensor]
             (Sensor updates RAM & commits to Config Flash block)
```

---

### 3.1. `PID_GET_TEMP` (Temperature Read)
- **Publisher:** Sensor (Slave)
- **Payload Length:** 2 Data Bytes + Enhanced Checksum
- **Encoding:** Little-Endian `int16_t` temperature in units of $0.1^\circ\text{C}$.

| Byte | Field | Type | Description |
| :---: | :--- | :---: | :--- |
| **0** | `Temp_LSB` | `uint8_t` | Low byte of temperature ($\times 10$). |
| **1** | `Temp_MSB` | `uint8_t` | High byte of temperature ($\times 10$). |
| **CS** | `Checksum` | `uint8_t` | Enhanced Checksum (PID + Bytes 0..1). |

*Example:* $+25.0^\circ\text{C} = 250 = \text{0x00FA}$ $\rightarrow$ Byte 0: `0xFA`, Byte 1: `0x00`.  
*Example:* $-10.0^\circ\text{C} = -100 = \text{0xFF9C}$ $\rightarrow$ Byte 0: `0x9C`, Byte 1: `0xFF`.

---

### 3.2. `PID_GET_CONFIG` (Configuration Read)
- **Publisher:** Sensor (Slave)
- **Payload Length:** 17 Data Bytes + Enhanced Checksum
- **Purpose:** Returns the complete active configuration and Factory SN in a single frame.

| Byte | Field | Type | Description |
| :---: | :--- | :---: | :--- |
| **0** | `Logical_Node_ID_B0` | `uint8_t` | Logical Node ID (Byte 0, LSB). |
| **1** | `Logical_Node_ID_B1` | `uint8_t` | Logical Node ID (Byte 1). |
| **2** | `Logical_Node_ID_B2` | `uint8_t` | Logical Node ID (Byte 2). |
| **3** | `Logical_Node_ID_B3` | `uint8_t` | Logical Node ID (Byte 3, MSB). |
| **4** | `Offset_mV_LSB`      | `uint8_t` | Sensor Voltage Offset at $0^\circ\text{C}$ in $\text{mV}$ (LSB). |
| **5** | `Offset_mV_MSB`      | `uint8_t` | Sensor Voltage Offset at $0^\circ\text{C}$ in $\text{mV}$ (MSB, `0xFFFF` = Default). |
| **6** | `Cal_Gain_LSB`       | `uint8_t` | Sensitivity LSB ($0.1\,\text{mV}/^\circ\text{C}$, `0` = Default $10.0\,\text{mV}/^\circ\text{C}$). |
| **7** | `Cal_Gain_MSB`       | `uint8_t` | Sensitivity MSB ($0.1\,\text{mV}/^\circ\text{C}$). |
| **8** | `PID_Get_Temp`       | `uint8_t` | Active PID for temperature requests. |
| **9** | `PID_Get_Config`     | `uint8_t` | Active PID for configuration read requests. |
| **10**| `PID_Set_Config`     | `uint8_t` | Active PID for configuration write requests. |
| **11**| `ADC_HW_Config`      | `uint8_t` | ADC HW sample time & hardware accumulator. |
| **12**| `SW_Filter_Config`   | `uint8_t` | SW digital filter mode. |
| **13**| `Factory_SN_B0`      | `uint8_t` | Factory Serial Number (Byte 0, LSB). |
| **14**| `Factory_SN_B1`      | `uint8_t` | Factory Serial Number (Byte 1). |
| **15**| `Factory_SN_B2`      | `uint8_t` | Factory Serial Number (Byte 2). |
| **16**| `Factory_SN_B3`      | `uint8_t` | Factory Serial Number (Byte 3, MSB). |
| **CS**| `Checksum`           | `uint8_t` | Enhanced Checksum (PID + Bytes 0..16). |

---

### 3.3. `PID_SET_CONFIG` (Write Configuration)
- **Publisher:** Master
- **Payload Length:** 13 Data Bytes + Enhanced Checksum
- **Purpose:** Transmits new configuration parameters. Sensor updates RAM and commits to Flash Sector 6.

| Byte | Field | Type | Description |
| :---: | :--- | :---: | :--- |
| **0** | `Logical_Node_ID_B0` | `uint8_t` | New Logical Node ID (Byte 0, LSB). |
| **1** | `Logical_Node_ID_B1` | `uint8_t` | New Logical Node ID (Byte 1). |
| **2** | `Logical_Node_ID_B2` | `uint8_t` | New Logical Node ID (Byte 2). |
| **3** | `Logical_Node_ID_B3` | `uint8_t` | New Logical Node ID (Byte 3, MSB). |
| **4** | `Offset_mV_LSB`      | `uint8_t` | New Offset Voltage in $\text{mV}$ (LSB). |
| **5** | `Offset_mV_MSB`      | `uint8_t` | New Offset Voltage in $\text{mV}$ (MSB, `0xFFFF` = Default $500\,\text{mV}$). |
| **6** | `Cal_Gain_LSB`       | `uint8_t` | New Sensitivity LSB ($0.1\,\text{mV}/^\circ\text{C}$, `0` = Default). |
| **7** | `Cal_Gain_MSB`       | `uint8_t` | New Sensitivity MSB ($0.1\,\text{mV}/^\circ\text{C}$). |
| **8** | `PID_Get_Temp`       | `uint8_t` | New PID for temperature requests. |
| **9** | `PID_Get_Config`     | `uint8_t` | New PID for configuration read requests. |
| **10**| `PID_Set_Config`     | `uint8_t` | New PID for configuration write requests. |
| **11**| `ADC_HW_Config`      | `uint8_t` | New ADC HW sample time & averaging settings. |
| **12**| `SW_Filter_Config`   | `uint8_t` | New SW digital filter mode. |
| **CS**| `Checksum`           | `uint8_t` | Enhanced Checksum (PID + Bytes 0..12). |

---

## 4. Measurement & Calibration Mathematical Model

### 4.1. Voltage Offset & Default Sentinel Rule
The nominal sensor output voltage at $0^\circ\text{C}$ is configurable via `offset_mv`.
Because valid sensor output voltages can legitimately be $0\,\text{mV}$ (e.g. for LM35), **`0` is a valid custom value and NOT the default sentinel**.
- **Default Sentinel:** The maximum value of `uint16_t` (`0xFFFF` = $65535$).
- When `offset_mv == 0xFFFF`, the firmware uses the **default $500\,\text{mV}$** baseline (MCP9700/TMP36).

### 4.2. Sensitivity (Gain) & Default Rule
`gain_sens` specifies the sensor transfer curve slope in units of **$0.1\,\text{mV}/^\circ\text{C}$**:
- **Default Sentinel:** `0` (or `0xFFFF`).
- When `gain_sens == 0`, firmware uses **default $10.0\,\text{mV}/^\circ\text{C}$** ($S_{\text{effective}} = 100$).
- Examples: `100` $\rightarrow 10.0\,\text{mV}/^\circ\text{C}$, `195` $\rightarrow 19.5\,\text{mV}/^\circ\text{C}$ (LMT84), `200` $\rightarrow 20.0\,\text{mV}/^\circ\text{C}$.

### 4.3. Generalized Calculation Formula

$$V_{\text{in\_mV}} = \frac{\text{ADC}_{\text{raw}} \times 3300}{4095}$$

$$V_{\text{offset\_effective}} = \begin{cases} 500 & \text{if } \text{offset\_mv} == \text{0xFFFF} \\ \text{offset\_mv} & \text{otherwise} \end{cases}$$

$$S_{\text{effective}} = \begin{cases} 100 & \text{if } \text{gain\_sens} == 0 \\ \text{gain\_sens} & \text{otherwise} \end{cases}$$

$$T_{\text{final\_x10}} = \frac{(V_{\text{in\_mV}} - V_{\text{offset\_effective}}) \times 100}{S_{\text{effective}}}$$

---

## 5. ADC Hardware & Software Filter Configuration

### 5.1. `adc_hw_config` (Hardware ADC Averaging & Sample Time)
Byte format: `[Bits 7..4: Sample Time] | [Bits 3..0: HW Hardware Accumulator]`

* **Bits 3..0: Hardware Accumulator Averaging (`DL_ADC12_HW_AVG_NUM_ACC`)**
  * `0x0`: 1x (Disabled / single conversion)
  * `0x1`: 4x hardware averaging
  * `0x2`: 8x hardware averaging
  * `0x3`: 16x hardware averaging
  * `0x4`: **32x hardware averaging (Default in `adc.c`)**
  * `0x5`: 64x hardware averaging
  * `0x6`: 128x hardware averaging

* **Bits 7..4: Sample Timer Cycles (`SCOMP0`)**
  * `0x0`: 32 clock cycles
  * `0x1`: 64 clock cycles
  * `0x2`: 128 clock cycles
  * `0x3`: 256 clock cycles
  * `0x4`: **512 clock cycles (Default in `adc.c`)**

### 5.2. `sw_filter_config` (Software Digital Filter)
* `0x00`: **Passthrough (Off)** - Raw HW-averaged sample.
* `0x01`: **Moving Average (4 samples)**.
* `0x02`: **Moving Average (8 samples)**.
* `0x03`: **Moving Average (16 samples)**.
* `0x04`: **Exponential Moving Average (EMA, $\alpha = 0.500$)** - Fast response.
* `0x05`: **Exponential Moving Average (EMA, $\alpha = 0.250$)** - Balanced smoothing.
* `0x06`: **Exponential Moving Average (EMA, $\alpha = 0.125$)** - Heavy noise suppression.

---

## 6. Telemetry & Extremes (Min/Max Temp) Flash Write Policy

To prevent premature Flash memory wear (limited to ~100k write cycles), recorded minimum and maximum temperatures are maintained in **RAM** and committed to **Flash Sector 7** only when specific conditions are met:

```
[Temperature Measurement]
          │
          ▼
   [New Record in RAM?] ─── No ───> [No Action]
          │
         Yes (Update RAM Record & Reset 1-Hour Settling Timer)
          │
          ├────────────────────────────────────────┐
          ▼                                        ▼
[Delta >= 5.0 °C from Last Flash?]    [Settled: >= 1 Hour since Last New Record?]
          │                                        │
         Yes ───► [Commit to Flash Sector 7] ◄─── Yes
```

### Commit Conditions:
1. **Stabilization / Settling Hysteresis:** A newly established extreme in RAM has **stabilized for at least 1 hour** without being superseded by a newer record (prevents repeated Flash writes during continuous temperature ramps/trends).
2. **Magnitude-based Fast Commit:** A newly recorded extreme immediately exceeds the previously saved Flash value by **$\ge 5.0^\circ\text{C}$** (`50` in tenths of $^\circ\text{C}$), bypassing the settling timer:
   ```c
   #define EXTREME_SIGNIFICANT_DELTA_X10   (50) // 5.0 deg C
   ```

---

## 7. Flash Memory Architecture (MSPM0C1103)

```
┌────────────────────────────────────────────────────────────────────────┐
│ Flash Memory Allocation (8 kB Total)                                   │
├──────────────┬──────────┬─────────────────────────────┬────────────────┤
│ Sector       │ Address  │ Block Content               │ Size & Status  │
├──────────────┼──────────┼─────────────────────────────┼────────────────┤
│ Sector 0..2  │ 0x000000 │ Firmware Program Code       │ 2.6 kB (Used)  │
│ Sector 3..4  │ 0x000C00 │ Free Flash / Code Expansion │ 2.4 kB (Free)  │
│ Sector 5     │ 0x001400 │ 1. Factory Block            │ 1 kB (Locked)  │
│ Sector 6     │ 0x001800 │ 2. Configuration Block      │ 1 kB (R/W)     │
│ Sector 7     │ 0x001C00 │ 3. Telemetry/Extremes Block │ 1 kB (R/W)     │
└──────────────┴──────────┴─────────────────────────────┴────────────────┘
```

### C Structure Definitions

```c
#include <stdint.h>

#define FLASH_FACTORY_MAGIC   (0xFAFAFAFA)
#define FLASH_CONFIG_MAGIC    (0xC0C0C0C0)
#define FLASH_EXTREMES_MAGIC  (0xECECECEC)

#define FLASH_FACTORY_ADDR    (0x00001400U)
#define FLASH_CONFIG_ADDR     (0x00001800U)
#define FLASH_EXTREMES_ADDR   (0x00001C00U)

#define OFFSET_MV_DEFAULT     (0xFFFFU)
#define GAIN_SENS_DEFAULT     (0x0000U)

/*
 * Block 1: Factory Production Block (Read-Only during normal operation)
 */
typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic;              // 0xFAFAFAFA
    uint32_t factory_sn;         // Permanent Serial Number
} FactoryBlock_t;

/*
 * Block 2: User Configuration Block (Updated upon PID_SET_CONFIG)
 */
typedef struct __attribute__((packed, aligned(8))) {
    // 64-bit Chunk 1
    uint32_t magic;              // 0xC0C0C0C0
    uint32_t logical_node_id;    // 32-bit system position ID

    // 64-bit Chunk 2
    uint16_t offset_mv;          // Offset in mV at 0 deg C (0xFFFF = 500 mV)
    uint16_t gain_sens;          // Sensitivity in 0.1 mV/deg C (0 = 100 = 10.0 mV/deg C)
    uint8_t  pid_get_temp;       // Configured PID for temperature
    uint8_t  pid_get_config;     // Configured PID for read config
    uint8_t  pid_set_config;     // Configured PID for write config
    uint8_t  filter_hw_adc;      // ADC HW Averaging & Sample Time

    // 64-bit Chunk 3
    uint8_t  filter_sw_mode;     // SW Digital Filter mode
    uint8_t  _padding[7];        // Pad to 24 bytes (multiple of 8 for Flash ECC)
} ConfigBlock_t;

/*
 * Block 3: Telemetry & Extremes Block (Updated dynamically based on write policy)
 */
typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic;              // 0xECECECEC
    int16_t  min_temp;           // Lowest recorded temp (0.1 deg C)
    int16_t  max_temp;           // Highest recorded temp (0.1 deg C)
} ExtremesBlock_t;
```

---

## 8. Factory Default Fallbacks

If the Configuration Block signature is invalid (`magic != FLASH_CONFIG_MAGIC`), RAM variables load standard defaults:

- `logical_node_id`: `0x00000001`
- `offset_mv`: `OFFSET_MV_DEFAULT` ($500\,\text{mV}$)
- `gain_sens`: `GAIN_SENS_DEFAULT` ($10.0\,\text{mV}/^\circ\text{C}$)
- `pid_get_temp`: `0x0A` (Default `LIN_SEND_TEMP_PID`)
- `pid_get_config`: `0x0B`
- `pid_set_config`: `0x0C`
- `adc_hw_config`: `0x00` (1x HW average, 32 cycles sample time)
- `sw_filter_config`: `0x00` (Passthrough, no software filter)
