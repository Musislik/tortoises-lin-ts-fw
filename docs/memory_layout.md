# MSPM0 Memory Layout Specification

This document defines the complete physical and logical memory architecture for the **Tortoises LIN Temperature Sensor Firmware** on the Texas Instruments **MSPM0C1103** (and MSPM0C1104 variant).

---

## 1. Hardware Architecture Overview

| Memory Region | Physical Base Address | Size | Bus / Access | Characteristics |
| :--- | :--- | :--- | :--- | :--- |
| **Main Flash (MSPM0C1103)** | `0x0000_0000` | 8 kB (`0x2000`) | I-Code / D-Code | 8 sectors of 1024 B, 64-bit ECC |
| **Main Flash (MSPM0C1104)** | `0x0000_0000` | 16 kB (`0x4000`) | I-Code / D-Code | 16 sectors of 1024 B, 64-bit ECC |
| **SRAM** | `0x2000_0000` | 1 kB (`0x0400`) | System Bus | Single-cycle, retains data in SLEEP |
| **Non-Main Flash (BCR)** | `0x41C0_0000` | 256 B (`0x0100`) | System Bus | Boot Configuration Register |

### Critical Flash Hardware Constraints:
* **Sector Erase Granularity:** Erase operations can only be executed on whole **1024-byte sectors** (`0x400`).
* **Programming Alignment:** Writes must be aligned to **64-bit boundaries (8 bytes)** due to hardware ECC generation (`DL_FlashCTL_programMemoryFromRAM64WithECCGenerated`).
* **Flash Endurance:** ~100,000 erase/write cycles per sector. Data in Sectors 6 and 7 must adhere to hysteresis and wear-reduction policies.

---

## 2. Flash Memory Map (MSPM0C1103 - 8 kB Total)

```
0x0000_0000 ┌─────────────────────────────────────────────────────────┐
            │ Sector 0: Interrupt Vectors (.intvecs) & Firmware Code  │ 1024 B
0x0000_0400 ├─────────────────────────────────────────────────────────┤
            │ Sector 1: Firmware Code (.text, .rodata, .cinit)        │ 1024 B
0x0000_0800 ├─────────────────────────────────────────────────────────┤
            │ Sector 2: Firmware Code & TI DriverLib Routines         │ 1024 B
0x0000_0C00 ├─────────────────────────────────────────────────────────┤
            │ Sector 3: Free Flash / Code Expansion Reserve           │ 1024 B
0x0000_1000 ├─────────────────────────────────────────────────────────┤
            │ Sector 4: Free Flash / Code Expansion Reserve           │ 1024 B
0x0000_1400 ├─────────────────────────────────────────────────────────┤
            │ Sector 5: Factory Block (Locked / Production Provision) │ 1024 B
0x0000_1800 ├─────────────────────────────────────────────────────────┤
            │ Sector 6: User Configuration Block (Runtime R/W)        │ 1024 B
0x0000_1C00 ├─────────────────────────────────────────────────────────┤
            │ Sector 7: Telemetry & Extremes Block (Runtime R/W)      │ 1024 B
0x0000_2000 └─────────────────────────────────────────────────────────┘
```

### Sector Breakdown Table

| Sector | Start Addr | End Addr | Size | Linker Region | Usage / Content | Write Access |
| :---: | :---: | :---: | :---: | :---: | :--- | :--- |
| **0** | `0x0000_0000` | `0x0000_03FF` | 1 kB | `FLASH` | Reset/IRQ vector table (`.intvecs`) + startup | Flashed with FW |
| **1** | `0x0000_0400` | `0x0000_07FF` | 1 kB | `FLASH` | Application logic (`main`, `adc`, `filter`) | Flashed with FW |
| **2** | `0x0000_0800` | `0x0000_0BFF` | 1 kB | `FLASH` | TI DriverLib + LIN communication handlers | Flashed with FW |
| **3** | `0x0000_0C00` | `0x0000_0FFF` | 1 kB | `FLASH` | Code reserve / future feature expansion | Flashed with FW |
| **4** | `0x0000_1000` | `0x0000_13FF` | 1 kB | `FLASH` | Code reserve (Upper limit of code `0x13FF`) | Flashed with FW |
| **5** | `0x0000_1400` | `0x0000_17FF` | 1 kB | `RESERVED` | Reserved (Formerly Factory Block, now via TI Trace ID) | Unused |
| **6** | `0x0000_1800` | `0x0000_1BFF` | 1 kB | `CONFIG` | **Unified Configuration Block (`ConfigBlock_t`)** | Runtime via LIN & Telemetry |
| **7** | `0x0000_1C00` | `0x0000_1FFF` | 1 kB | `RESERVED` | Reserved (Extremes merged into Sector 6) | Unused |

---

## 3. Dedicated Flash Data Blocks Specification

### 3.1 Hardware Silicon Serial Number (TI Factory Region)
* **Purpose:** Permanent, hardware-locked manufacturing identification.
* **Storage:** Read directly from TI hardware register `FACTORYREGION->TRACEID` (`0x41C40000`) using DriverLib function `DL_FactoryRegion_getTraceID()`.
* **Characteristics:** Unique 32-bit wafer/lot trace ID burned by TI during ATE testing. Requires zero Flash memory.

### 3.2 Sector 6: Unified Configuration & Extremes Block (`CONFIG_BLOCK_ADDR = 0x0000_1800`)
* **Purpose:** Network node identity, sensor calibration, filter modes, LIN PIDs, and operational temperature extremes.
* **Access Policy:** Read/Write. Updated dynamically when a valid `PID_SET_CONFIG` frame is received via LIN or when significant extremes are recorded.
* **Function Handler:** `configSave()` erases Sector 6 and writes 32 bytes (four 64-bit ECC chunks).
* **Layout (`ConfigBlock_t`, 32 bytes, 64-bit aligned):**
  ```c
  typedef struct __attribute__((packed, aligned(8))) {
      uint32_t magic;           // 0xCFCFCF00 (CONFIG_MAGIC)
      uint32_t logicalNodeId;   // 32-bit system node role / position ID
      int32_t  offsetMv;        // Sensor voltage at 0 deg C in mV in Q16.16 (e.g. 0x01F40000 = 500.0 mV)
      uint32_t gainSens;        // Sensitivity in UQ16.16 (e.g. 0x000A0000 = 10.0 mV/deg C)
      uint8_t  pidGetTemp;      // Configured LIN PID for temperature query
      uint8_t  pidGetConfig;    // Configured LIN PID for configuration read
      uint8_t  pidSetConfig;    // Configured LIN PID for configuration write
      uint8_t  filterHwAdc;     // ADC sample cycles & HW averaging
      uint8_t  filterSwMode;    // Digital filter mode (passthrough, MA, EMA)
      int16_t  minTemp;         // Lowest recorded temperature in 0.1 deg C units
      int16_t  maxTemp;         // Highest recorded temperature in 0.1 deg C units
      uint8_t  _padding[7];     // Padding to maintain 8-byte ECC boundary (Total 32 B)
  } ConfigBlock_t;
  ```

---

## 4. Linker Configuration & Overflow Protection

To guarantee that application code can never accidentally expand into Sector 5 (Factory Block) or Sector 6/7, the linker command file (`startup/mspm0c1103.cmd`) enforces strict boundary segmentation:

```cmd
MEMORY
{
    /* Application Flash strictly limited to 5 kB (Sectors 0 to 4). */
    /* Linker produces an error if code exceeds 0x13FF.              */
    FLASH       (RX)  : origin = 0x00000000, length = 0x00001400

    /* Sector 5: Factory Block (1 kB)                               */
    FACTORY     (R)   : origin = 0x00001400, length = 0x00000400

    /* Sector 6: User Configuration Block (1 kB)                    */
    CONFIG      (R)   : origin = 0x00001800, length = 0x00000400

    /* Sector 7: Extremes & Telemetry Block (1 kB)                  */
    EXTREMES    (R)   : origin = 0x00001C00, length = 0x00000400

    /* SRAM: 1 kB total                                             */
    SRAM        (RWX) : origin = 0x20000000, length = 0x00000400

    /* Boot Configuration Register                                  */
    BCR_CONFIG  (R)   : origin = 0x41C00000, length = 0x000000FF
}

SECTIONS
{
    .intvecs:   > 0x00000000
    .text   : palign(8) {} > FLASH
    .const  : palign(8) {} > FLASH
    .cinit  : palign(8) {} > FLASH
    .pinit  : palign(8) {} > FLASH
    .rodata : palign(8) {} > FLASH
    .ARM.exidx    : palign(8) {} > FLASH
    .init_array   : palign(8) {} > FLASH
    .binit        : palign(8) {} > FLASH
    .TI.ramfunc   : load = FLASH, palign(8), run=SRAM, table(BINIT)

    /* Factory production block placed at 0x1400 */
    .factoryBlock : palign(8) {} > FACTORY

    .vtable :   > SRAM
    .data   :   > SRAM
    .bss    :   > SRAM
    .sysmem :   > SRAM
    .stack  :   > SRAM (HIGH)

    .BCRConfig  : {} > BCR_CONFIG
}
```

---

## 5. Production Multi-Binary Generation (Solution 1 Architecture)

### Workflow Principles:
1. **Single Compilation of Application Core:**
   All common source files (`main.c`, `adc.c`, `filter.c`, `telemetry.c`, TI DriverLib, SysConfig) are compiled into a CMake `OBJECT` library (`app_core`) exactly once.
2. **List-Driven SN Generation:**
   A text file `sensors.txt` contains the list of target serial numbers:
   ```text
   1
   2
   3
   4
   ```
3. **Dedicated Target per Sensor:**
   CMake loops over `sensors.txt` and links `app_core` with a minimal `factory_block.c`:
   ```c
   #include "config.h"

   #pragma LOCATION(".factoryBlock")
   const FactoryBlock_t gFlashFactoryBlock = {
       .magic = FACTORY_MAGIC,
       .factorySn = SENSOR_SN
   };
   ```
4. **Automated Intel HEX Generation:**
   For each serial number, the linker produces an `.out` file with full debug symbols and runs `tiarmobjcopy -O ihex` to output `Tortoises_SN_1.hex`, `Tortoises_SN_2.hex`, etc., ready for flashing.

---

## 6. SRAM Allocation & Budget (1024 Bytes Total)

```
0x2000_0000 ┌─────────────────────────────────────────────────────────┐
            │ .vtable & Hardware Buffers                              │ ~64 B
0x2000_0040 ├─────────────────────────────────────────────────────────┤
            │ .data: Initialized Globals copied from Flash            │ ~32 B
0x2000_0060 ├─────────────────────────────────────────────────────────┤
            │ .bss: Zero-initialized Globals                          │
            │   - gActiveConfig (24 B)                                │
            │   - gActiveFactory (8 B)                                │
            │   - gActiveExtremes (8 B)                               │
            │   - gPendingConfig (24 B)                               │
            │   - Filter history ring buffers (~64 B)                 │ ~160 B
0x2000_0100 ├─────────────────────────────────────────────────────────┤
            │ Free SRAM Margin                                        │ ~700 B
0x2000_03C0 ├─────────────────────────────────────────────────────────┤
            │ .stack: System Stack (grows downward from 0x2000_0400)  │ 64–128 B
0x2000_0400 └─────────────────────────────────────────────────────────┘
```
* Heap allocation (`malloc`/`free`) is strictly forbidden by project coding rules; `.sysmem` is allocated 0 bytes.

---

## 7. Scaling to MSPM0C1104 (16 kB Variant)

When targeting the **MSPM0C1104**:
* Flash size doubles to **16 kB (`0x4000`)**.
* `FLASH` region expands from 5 kB to **13 kB (`0x0000_0000` .. `0x0000_33FF`)**.
* Sectors 13, 14, and 15 (or keeping Sectors 5, 6, 7 fixed) serve as the locked and runtime data blocks, ensuring software compatibility across both chip derivatives.
