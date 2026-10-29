# LIN Temperature Sensor - Future Features & Roadmap

This document captures planned concepts, proposed architectural enhancements, and potential future extensions that are currently deferred.

---

## 1. Power-On Rescue / Recovery Window (Safe Mode)

### Problem Description
If an operator or automated LIN Master reconfigures the sensor's PIDs via `PID_SET_CONFIG` to invalid, collided, or unknown values, the sensor might become unresponsive ("bricked") from the perspective of standard network management tools.

### Proposed Solution
Implement a temporary **Safe Configuration Window** upon device power-up:

- **Duration:** First $5.0\text{ seconds}$ after microcontroller boot / reset.
- **Behavior:** During this time window, the sensor listens and responds to **both**:
  1. The configured custom PIDs stored in Flash.
  2. The hardcoded **Factory Default PIDs**:
     - `PID_GET_TEMP` = `0x0A`
     - `PID_GET_CONFIG` = `0x0B`
     - `PID_SET_CONFIG` = `0x0C`
- **Fallback Action:** If a valid `PID_SET_CONFIG` command is received during this window on the default PID, the sensor accepts the new configuration and updates Flash.
- **Normal Operation:** After 5 seconds, the default PIDs are disabled, and only custom configured PIDs remain active to avoid collisions on production LIN networks.

---

## 2. LIN 2.x Transport Layer / Node Configuration (NAD)

- Standardized LIN 2.x diagnostic frames (`0x3C` Master Request / `0x3D` Slave Response).
- Official `Assign NAD` and `Assign Frame Identifier` services.

---

## 3. Flash Emulated EEPROM / Wear Leveling for Telemetry

- Implementation of rolling 64-bit slot records inside Sector 7 to achieve $128\times$ higher endurance for telemetry/extremes storage without needing whole-sector erases on every record update.

---

## 4. Firmware Version Reporting

### Problem Description
Currently, the device reports sensor configuration and the factory hardware Trace ID (via `PID_GET_CONFIG`), but does not expose the installed firmware version over the LIN bus. This complicates fleet diagnostics, automated testing, and verification of flashed firmware revisions in the field.

### Proposed Solution
Provide a mechanism for the LIN Master to query the active firmware revision:
- **Option A (Extend `PID_GET_CONFIG`):** Include firmware version fields (e.g., Major, Minor, Patch as 3 bytes or a 32-bit semantic version identifier) within the `PID_GET_CONFIG` response payload.
- **Option B (Dedicated PID `PID_GET_FW_VERSION`):** Define a separate PID specifically returning firmware revision, build counter, and Git hash / build timestamp.
- **Option C (LIN 2.x Diagnostic Identifier):** Expose the firmware revision through standard LIN diagnostic Read by Identifier requests (`0x3C` / `0x3D`).

