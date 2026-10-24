# LIN Temperature Sensor - Configuration and Communication Specification

## 1. Overview & Architecture

This document defines the configuration parameters, memory layout, and LIN communication protocol for the MSPM0C1103-based LIN temperature sensor.

### Key Principles
- **LIN Protocol:** LIN 2.x physical/data layer with extended custom payload frames (allowing frames larger than 8 bytes for unified single-transaction configuration).
- **Checksum Model:** **Enhanced Checksum only** (PID included in the checksum calculation over the entire payload).
- **Endianness:** **Little-Endian (LSB first)** for all multi-byte values (`uint16_t`, `int16_t`, `uint32_t`).
- **Hardware Serial Number:** Read directly at runtime from TI Factory Region via DriverLib (`DL_FactoryRegion_getTraceID()`).
- **Memory Architecture:** Parameters and operational extremes are unified into a **single Flash memory block** (Sector 7, 32 bytes, 64-bit aligned for hardware ECC):
  1. **TI Factory Region (`0x41C40000`, Hardware Read-Only):** Holds unique silicon wafer/die Trace ID. Read dynamically on boot / request.
  2. **Unified Configuration Block (Sector 7, `0x1C00`, Read/Write):** Holds user/network configuration and operational extremes (Min/Max recorded temperatures) in a single 32-byte block.

---

## 2. Parameter Definitions

| Parameter | Type | Endianness | Storage Block | Access via LIN | Description |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Magic Word** | `uint32_t` | Little-Endian | Config Block | Internal | Block integrity signature (`0xCFCFCF00`). |
| **Factory Serial Number (SN)** | `uint32_t` | Little-Endian | TI Factory Region | Read-Only | Permanent unique hardware Trace ID from TI silicon (`DL_FactoryRegion_getTraceID()`). |
| **Logical Node ID** | `uint32_t` | Little-Endian | Config Block | Read / Write | 32-bit system position / role ID in the vehicle network. |
| **PID: Get Temperature** | `uint8_t` | N/A | Config Block | Read / Write | LIN Protected ID for temperature requests. |
| **PID: Get Configuration** | `uint8_t` | N/A | Config Block | Read / Write | LIN Protected ID for configuration requests. |
| **PID: Set Configuration** | `uint8_t` | N/A | Config Block | Read / Write | LIN Protected ID for configuration updates. |
| **ADC HW Config** | `uint8_t` | N/A | Config Block | Read / Write | Hardware ADC sample time & hardware accumulator averaging. |
| **SW Filter Config** | `uint8_t` | N/A | Config Block | Read / Write | Software digital filtering algorithm & window depth. |
| **Sensor Voltage Offset (`offset_mv`)** | `int32_t` | Little-Endian Q16.16 | Config Block | Read / Write | Nominal voltage at $0^\circ\text{C}$ in $\text{mV}$ in signed Q16.16 format. **Default: $500.0\,\text{mV}$ (`0x01F40000`)**. |
| **Calibration Gain (`gain_sens`)** | `uint32_t` | Little-Endian UQ16.16 | Config Block | Read / Write | Sensor transfer curve sensitivity in $\text{mV}/^\circ\text{C}$ in UQ16.16 format (**`0x000A0000` = Default $10.0\,\text{mV}/^\circ\text{C}$**). |
| **Min Recorded Temperature** | `int16_t` | Little-Endian | Config Block | Read-Only | Lowest recorded temperature ($0.1^\circ\text{C}$). |
| **Max Recorded Temperature** | `int16_t` | Little-Endian | Config Block | Read-Only | Highest recorded temperature ($0.1^\circ\text{C}$). |
| **Block CRC-32** | `uint32_t` | Little-Endian | Config Block | Internal | CRC-32 integrity checksum over preceding 28 bytes. |

---

## 3. LIN Protocol & Frame Definitions

All multi-byte fields are transmitted in **Little-Endian** format (Low byte first).

```
[LIN Master] ────── Header (PID_GET_TEMP) ───────────> [Sensor]
             <───── Temperature (2B) + CS ──────────── [Sensor]

[LIN Master] ────── Header (PID_GET_CONFIG) ─────────> [Sensor]
             <───── Extended Config (21B) + CS ─────── [Sensor]

[LIN Master] ────── Header + New Config (17B) + CS ──> [Sensor]
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
- **Payload Length:** 21 Data Bytes + Enhanced Checksum (22 Bytes Total)
- **Purpose:** Returns the complete active configuration and Factory SN in a single frame.

| Byte | Field | Type | Description |
| :---: | :--- | :---: | :--- |
| **0** | `Logical_Node_ID_B0` | `uint8_t` | Logical Node ID (Byte 0, LSB). |
| **1** | `Logical_Node_ID_B1` | `uint8_t` | Logical Node ID (Byte 1). |
| **2** | `Logical_Node_ID_B2` | `uint8_t` | Logical Node ID (Byte 2). |
| **3** | `Logical_Node_ID_B3` | `uint8_t` | Logical Node ID (Byte 3, MSB). |
| **4** | `Offset_mV_B0`        | `uint8_t` | Sensor Voltage Offset in $\text{mV}$ Q16.16 (Byte 0, LSB). |
| **5** | `Offset_mV_B1`        | `uint8_t` | Sensor Voltage Offset in $\text{mV}$ Q16.16 (Byte 1). |
| **6** | `Offset_mV_B2`        | `uint8_t` | Sensor Voltage Offset in $\text{mV}$ Q16.16 (Byte 2). |
| **7** | `Offset_mV_B3`        | `uint8_t` | Sensor Voltage Offset in $\text{mV}$ Q16.16 (Byte 3, MSB, Default $500.0\,\text{mV} = \text{0x01F40000}$). |
| **8** | `Cal_Gain_B0`        | `uint8_t` | Sensitivity UQ16.16 Byte 0 (LSB). |
| **9** | `Cal_Gain_B1`        | `uint8_t` | Sensitivity UQ16.16 Byte 1. |
| **10**| `Cal_Gain_B2`        | `uint8_t` | Sensitivity UQ16.16 Byte 2. |
| **11**| `Cal_Gain_B3`        | `uint8_t` | Sensitivity UQ16.16 Byte 3 (MSB, `0x000A0000` = Default $10.0\,\text{mV}/^\circ\text{C}$). |
| **12**| `PID_Get_Temp`       | `uint8_t` | Active PID for temperature requests. |
| **13**| `PID_Get_Config`     | `uint8_t` | Active PID for configuration read requests. |
| **14**| `PID_Set_Config`     | `uint8_t` | Active PID for configuration write requests. |
| **15**| `ADC_HW_Config`      | `uint8_t` | ADC HW sample time & hardware accumulator. |
| **16**| `SW_Filter_Config`   | `uint8_t` | SW digital filter mode. |
| **17**| `Factory_SN_B0`      | `uint8_t` | Factory Serial Number (Byte 0, LSB). |
| **18**| `Factory_SN_B1`      | `uint8_t` | Factory Serial Number (Byte 1). |
| **19**| `Factory_SN_B2`      | `uint8_t` | Factory Serial Number (Byte 2). |
| **20**| `Factory_SN_B3`      | `uint8_t` | Factory Serial Number (Byte 3, MSB). |
| **CS**| `Checksum`           | `uint8_t` | Enhanced Checksum (PID + Bytes 0..20). |

---

### 3.3. `PID_SET_CONFIG` (Write Configuration)
- **Publisher:** Master
- **Payload Length:** 17 Data Bytes + Enhanced Checksum (18 Bytes Total)
- **Purpose:** Transmits new configuration parameters. Sensor updates RAM and commits to Flash Sector 6.

| Byte | Field | Type | Description |
| :---: | :--- | :---: | :--- |
| **0** | `Logical_Node_ID_B0` | `uint8_t` | New Logical Node ID (Byte 0, LSB). |
| **1** | `Logical_Node_ID_B1` | `uint8_t` | New Logical Node ID (Byte 1). |
| **2** | `Logical_Node_ID_B2` | `uint8_t` | New Logical Node ID (Byte 2). |
| **3** | `Logical_Node_ID_B3` | `uint8_t` | New Logical Node ID (Byte 3, MSB). |
| **4** | `Offset_mV_B0`        | `uint8_t` | New Offset Voltage in $\text{mV}$ Q16.16 (Byte 0, LSB). |
| **5** | `Offset_mV_B1`        | `uint8_t` | New Offset Voltage in $\text{mV}$ Q16.16 (Byte 1). |
| **6** | `Offset_mV_B2`        | `uint8_t` | New Offset Voltage in $\text{mV}$ Q16.16 (Byte 2). |
| **7** | `Offset_mV_B3`        | `uint8_t` | New Offset Voltage in $\text{mV}$ Q16.16 (Byte 3, MSB, Default $500.0\,\text{mV} = \text{0x01F40000}$). |
| **8** | `Cal_Gain_B0`        | `uint8_t` | New Sensitivity UQ16.16 Byte 0 (LSB). |
| **9** | `Cal_Gain_B1`        | `uint8_t` | New Sensitivity UQ16.16 Byte 1. |
| **10**| `Cal_Gain_B2`        | `uint8_t` | New Sensitivity UQ16.16 Byte 2. |
| **11**| `Cal_Gain_B3`        | `uint8_t` | New Sensitivity UQ16.16 Byte 3 (MSB). |
| **12**| `PID_Get_Temp`       | `uint8_t` | New PID for temperature requests. |
| **13**| `PID_Get_Config`     | `uint8_t` | New PID for configuration read requests. |
| **14**| `PID_Set_Config`     | `uint8_t` | New PID for configuration write requests. |
| **15**| `ADC_HW_Config`      | `uint8_t` | New ADC HW sample time & averaging settings. |
| **16**| `SW_Filter_Config`   | `uint8_t` | New SW digital filter mode. |
| **CS**| `Checksum`           | `uint8_t` | Enhanced Checksum (PID + Bytes 0..16). |

---

## 4. Measurement & Calibration Mathematical Model

### 4.1. Voltage Offset & Default Sentinel Rule
The nominal sensor output voltage at $0^\circ\text{C}$ is configurable via `offset_mv` in **signed Q16.16 format** ($V_{\text{actual}} = \text{offset\_mv} / 65536\,\text{mV}$):
- **Resolution:** $1 / 65536\,\text{mV} \approx 15.26\,\mu\text{V}$.
- Because valid sensor output voltages can legitimately be $0\,\text{mV}$ (e.g. for LM35), **`0` is a valid custom value and NOT the default sentinel**.
- **Default Sentinel:** The unprogrammed flash state `0xFFFFFFFF`.
- When `offset_mv == 0xFFFFFFFF`, the firmware uses the **default $500.0\,\text{mV}$** baseline (`0x01F40000` = $32\,768\,000$).

### 4.2. Sensitivity (Gain) & UQ16.16 Representation
`gain_sens` specifies the sensor transfer curve slope in **UQ16.16 unsigned fixed-point format** ($V_{\text{actual}} = \text{gain\_sens} / 65536\,\text{mV}/^\circ\text{C}$):
- **Default Sentinel:** `0` (or `0x000A0000`).
- When `gain_sens == 0`, firmware uses **default $10.0\,\text{mV}/^\circ\text{C}$** (`0x000A0000` = $655360$).
- Examples:
  - `0x000A0000` = $10.0 \times 65536 = 655360$ $\rightarrow 10.0\,\text{mV}/^\circ\text{C}$ (TMP235, MCP9700)
  - `0x00138000` = $19.5 \times 65536 = 1277952$ $\rightarrow 19.5\,\text{mV}/^\circ\text{C}$ (LMT84)
  - `0x00140000` = $20.0 \times 65536 = 1310720$ $\rightarrow 20.0\,\text{mV}/^\circ\text{C}$

### 4.3. Generalized Calculation Formula
The conversion eliminates intermediate quantization by using an exact rational representation with reduced fraction $220 / 273$ ($\text{GCD}(3300, 4095) = 15$) and unified Q16.16 scaling:

$$\Delta_{\text{Q16.16}} = \text{ADC}_{\text{raw}} \times (220 \times 65536) - \text{offset\_mv} \times 273$$

$$\text{num} = \Delta_{\text{Q16.16}} \times 10$$

$$\text{den} = 273 \times \text{gain\_sens}$$

$$T_{\text{final\_x10}} = \begin{cases} (\text{num} + \text{den} / 2) / \text{den} & \text{if } \text{num} \ge 0 \\ (\text{num} - \text{den} / 2) / \text{den} & \text{if } \text{num} < 0 \end{cases}$$

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
│ Sector 3..6  │ 0x000C00 │ Free Flash / Code Expansion │ 4.4 kB (Free)  │
│ Sector 7     │ 0x001C00 │ Unified Configuration Block │ 1 kB (R/W)     │
└──────────────┴──────────┴─────────────────────────────┴────────────────┘
```

### C Structure Definitions

```c
#include <stdint.h>

#define CONFIG_MAGIC        (0xCFCFCF00U)
#define CONFIG_BLOCK_ADDR   (0x00001C00U)

#define OFFSET_MV_DEFAULT   (0x01F40000U) // 500.0 mV in Q16.16
#define GAIN_SENS_DEFAULT   (0x000A0000U) // 10.0 mV/deg C in UQ16.16

/*
 * Unified User Configuration Block (Updated upon PID_SET_CONFIG or telemetry extreme write)
 * Total size: 32 bytes (exactly four 64-bit ECC chunks)
 */
typedef struct __attribute__((packed, aligned(8))) {
    uint32_t magic;              // 0xCFCFCF00 (CONFIG_MAGIC)
    uint32_t logical_node_id;    // 32-bit system position ID
    int32_t  offset_mv;          // Offset in mV at 0 deg C in Q16.16 (default 0x01F40000 = 500.0 mV)
    uint32_t gain_sens;          // Sensitivity in UQ16.16 (0x000A0000 = 10.0 mV/deg C)
    uint8_t  pid_get_temp;       // Configured PID for temperature
    uint8_t  pid_get_config;     // Configured PID for read config
    uint8_t  pid_set_config;     // Configured PID for write config
    uint8_t  filter_hw_adc;      // ADC HW Averaging & Sample Time
    uint8_t  filter_sw_mode;     // SW Digital Filter mode
    int16_t  min_temp;           // Lowest recorded temperature (0.1 deg C)
    int16_t  max_temp;           // Highest recorded temperature (0.1 deg C)
    uint8_t  _padding[3];        // Alignment padding to 32-bit boundary
    uint32_t crc;                // CRC-32 integrity checksum over preceding 28 bytes
} ConfigBlock_t;
```
```

---

## 8. Factory Default Fallbacks

If the Configuration Block signature is invalid (`magic != FLASH_CONFIG_MAGIC`), RAM variables load standard defaults:

- `logical_node_id`: `0x00000001`
- `offset_mv`: `OFFSET_MV_DEFAULT` ($500.0\,\text{mV}$ in Q16.16: `0x01F40000`)
- `gain_sens`: `GAIN_SENS_DEFAULT` ($10.0\,\text{mV}/^\circ\text{C}$ in UQ16.16: `0x000A0000`)
- `pid_get_temp`: `0x0A` (Default `LIN_SEND_TEMP_PID`)
- `pid_get_config`: `0x0B`
- `pid_set_config`: `0x0C`
- `adc_hw_config`: `0x00` (1x HW average, 32 cycles sample time)
- `sw_filter_config`: `0x00` (Passthrough, no software filter)
