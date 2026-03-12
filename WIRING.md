# Wiring Connection Table

## 3-Zone Oven Temperature Monitor
ESP32 + 3x MAX6675 Thermocouple Sensors + 3x TM1637 7-Segment Displays

---

## Overview
This document describes all electrical connections between the ESP32 and peripheral devices for the 3-zone oven temperature monitoring system.

**Total GPIO Pins Used: 15**
- 9 MAX6675 pins (3 sensors × SO/SCK/CS)
- 6 TM1637 pins (3 displays × 2 pins each)

---

## MAX6675 Sensor Connections

### Per-Sensor Signal Pins (SO/SCK/CS)

| Sensor | Zone     | ESP32 SO (MISO) GPIO      | ESP32 SCK (CLK) GPIO | ESP32 CS GPIO |
|--------|----------|---------------------------|----------------------|---------------|
| 1      | Entrance | GPIO 19                   | GPIO 18              | GPIO 5        |
| 2      | Middle   | GPIO 34 (input-only pin)  | GPIO 23              | GPIO 4        |
| 3      | Exit     | GPIO 27                   | GPIO 13              | GPIO 15       |

Each sensor has its own dedicated SO and SCK lines. No SPI signals are shared between sensors.

### MAX6675 Power Connections
Each MAX6675 module requires power:

| MAX6675 Pin | Connection | Notes |
|-------------|------------|-------|
| VCC         | 3.3V       | Connect to ESP32 3.3V output |
| GND         | GND        | Connect to ESP32 GND         |

**Note:** All three sensors share the same power rails.

---

## TM1637 Display Connections

Each TM1637 display has independent CLK and DIO pins:

### Display 1 (Entrance Zone)

| ESP32 GPIO | TM1637 Pin | Function | Wire Color Suggestion |
|------------|------------|----------|----------------------|
| GPIO 22    | CLK        | Clock    | Red                  |
| GPIO 21    | DIO        | Data I/O | Brown                |

### Display 2 (Middle Zone)

| ESP32 GPIO | TM1637 Pin | Function | Wire Color Suggestion |
|------------|------------|----------|----------------------|
| GPIO 25    | CLK        | Clock    | White                |
| GPIO 26    | DIO        | Data I/O | Gray                 |

### Display 3 (Exit Zone)

| ESP32 GPIO | TM1637 Pin | Function | Wire Color Suggestion |
|------------|------------|----------|----------------------|
| GPIO 32    | CLK        | Clock    | Pink                 |
| GPIO 33    | DIO        | Data I/O | Cyan                 |

### TM1637 Power Connections
Each TM1637 display requires power:

| TM1637 Pin | Connection | Notes |
|------------|------------|-------|
| VCC        | 5V         | Connect to external 5V supply (can use ESP32 5V if sufficient current) |
| GND        | GND        | Connect to common GND                                                  |

**Important:** TM1637 displays typically require 5V for proper brightness. Share common GND with ESP32.

---

## Complete Wiring Summary

### ESP32 GPIO Pin Usage

| GPIO Pin | Function                          | Component           | Zone/Purpose          |
|----------|-----------------------------------|---------------------|-----------------------|
| 19       | MAX6675 Sensor 1 SO (MISO)        | Sensor 1            | Entrance              |
| 18       | MAX6675 Sensor 1 SCK (CLK)        | Sensor 1            | Entrance              |
| 5        | MAX6675 Sensor 1 CS               | Sensor 1            | Entrance              |
| 34       | MAX6675 Sensor 2 SO (MISO, in-only)| Sensor 2           | Middle                |
| 23       | MAX6675 Sensor 2 SCK (CLK)        | Sensor 2            | Middle                |
| 4        | MAX6675 Sensor 2 CS               | Sensor 2            | Middle                |
| 27       | MAX6675 Sensor 3 SO (MISO)        | Sensor 3            | Exit                  |
| 13       | MAX6675 Sensor 3 SCK (CLK)        | Sensor 3            | Exit                  |
| 15       | MAX6675 Sensor 3 CS               | Sensor 3            | Exit                  |
| 22       | TM1637 Display 1 CLK              | Display 1           | Entrance              |
| 21       | TM1637 Display 1 DIO              | Display 1           | Entrance              |
| 25       | TM1637 Display 2 CLK              | Display 2           | Middle                |
| 26       | TM1637 Display 2 DIO              | Display 2           | Middle                |
| 32       | TM1637 Display 3 CLK              | Display 3           | Exit                  |
| 33       | TM1637 Display 3 DIO              | Display 3           | Exit                  |
| 35       | SD Card MISO (input-only)         | SD Card             | Future                |
| 14       | SD Card MOSI                      | SD Card             | Future                |
| 12       | SD Card CLK                       | SD Card             | Future                |
| 2        | SD Card CS                        | SD Card             | Future                |

---

### SD Card Module (6-pin) Wiring

| SD Module Pin | ESP32 Pin | Notes |
|---------------|-----------|-------|
| VVV (VCC)     | 3.3V      | Do not use 5V logic |
| GND           | GND       | Common ground required |
| MISO          | GPIO35    | Input-only, good for DO |
| MOSI          | GPIO14    | ESP32 output to DI |
| SCK           | GPIO12    | SPI clock |
| CS            | GPIO2     | Keep HIGH during boot |

Boot note: GPIO2 and GPIO12 are ESP32 boot-strap pins. If the module holds either in the wrong state during reset, boot can fail.

---

## Power Distribution

### Power Requirements

| Component      | Voltage | Current (Typical) | Quantity | Total Current |
|----------------|---------|-------------------|----------|---------------|
| ESP32-WROOM-IE | 3.3V    | ~240 mA          | 1        | 240 mA        |
| MAX6675        | 3.3V    | ~50 mA           | 3        | 150 mA        |
| TM1637 Display | 5V      | ~80 mA (max)     | 3        | 240 mA        |

### Power Connection Strategy

**Option 1: USB Power (Recommended for Development)**
```
USB 5V ──→ ESP32 5V Pin
  ├──→ ESP32 (internal regulator to 3.3V)
  ├──→ MAX6675 sensors via ESP32 3.3V pin
  └──→ TM1637 displays (direct 5V)
```

**Option 2: External Power Supply (Recommended for Production)**
```
5V Power Supply
  ├──→ ESP32 5V Pin
  │    ├──→ ESP32 internal regulator
  │    └──→ MAX6675 sensors via 3.3V pin
  └──→ TM1637 displays (direct)
```

### Ground Connections
**Critical:** All components MUST share a common ground.

```
GND Reference:
ESP32 GND ──┬── MAX6675 Sensor 1 GND
            ├── MAX6675 Sensor 2 GND
            ├── MAX6675 Sensor 3 GND
            ├── TM1637 Display 1 GND
            ├── TM1637 Display 2 GND
            ├── TM1637 Display 3 GND
            └── Power Supply GND
```

---

## Hardware Notes

### MAX6675 Modules
- **Type:** K-Type Thermocouple Interface
- **Temperature Range:** 0°C to 1024°C
- **Resolution:** 0.25°C (12-bit)
- **Communication:** SPI (bit-banged in this implementation)
- **Module Pinout (HW-550):** GND, VCC, SCK, CS, SO

### TM1637 Displays (HW-069)
- **Type:** 4-digit 7-segment LED display with colon
- **Driver IC:** TM1637
- **Communication:** Custom 2-wire protocol (CLK + DIO)
- **Brightness Levels:** 8 levels (0-7), configurable in software
- **Module Pinout:** CLK, DIO, VCC, GND

### Thermocouple Connections
Each MAX6675 sensor connects to a K-type thermocouple:
- **Red Wire:** + (plus)
- **Yellow/Blue Wire:** - (minus)

Polarity matters! Reversed polarity will result in inverted temperature readings.

---

## Verification Checklist

Before powering on, verify:

- [ ] Sensor 1 SO/SCK/CS match menuconfig values
- [ ] Sensor 2 SO/SCK/CS match menuconfig values
- [ ] Sensor 3 SO/SCK/CS match menuconfig values
- [ ] Each TM1637 display has unique CLK/DIO pair
- [ ] All MAX6675 sensors connected to 3.3V (NOT 5V)
- [ ] All TM1637 displays connected to 5V
- [ ] Common ground established between ESP32, all sensors, and all displays
- [ ] All thermocouples connected with correct polarity (red = +)
- [ ] No short circuits between adjacent GPIO pins
- [ ] Power supply can provide at least 650 mA total current

---

## Troubleshooting

### Display shows "Err"
- Thermocouple disconnected or faulty
- Wrong MAX6675 pin connections
- MAX6675 receiving 5V instead of 3.3V

### Display blank or dim
- TM1637 not receiving 5V power
- Wrong CLK/DIO pin configuration
- Brightness set too low (increase in menuconfig)

### Temperature reads 0°C or -0.25°C constantly
- Thermocouple connected with reversed polarity
- MAX6675 CS pin not correctly assigned

### All displays show "Err"
- SO/SCK/CS pin mapping in menuconfig does not match wiring
- GND not common between ESP32 and MAX6675 modules

---

## Configuration Changes

To modify pin assignments, run:
```bash
idf.py menuconfig
```

Navigate to:
```
Oven Temperature Logger Configuration
  → Sensor Read Configuration
  → MAX6675 Temperature Sensors
  → TM1637 Display Configuration
```

Make changes, save, and rebuild the project.

---

## Schematic Diagram (ASCII)

```
                              ┌─────────────┐
                              │   ESP32     │
                              │ WROOM-IE    │
                              │             │
        ┌─────────────────────┤ GPIO 18(SCK)│
        │     ┌───────────────┤ GPIO 19(SO) │
        │     │               │             │
        │     │     ┌─────────┤ GPIO 5(CS1) │
        │     │     │   ┌─────┤ GPIO4(CS2)  │
        │     │     │   │ ┌───┤ GPIO15(CS3) │
        │     │     │   │ │   │             │
        ▼     ▼     ▼   ▼ ▼   │             │
      ┌─────────────────────┐ │ GPIO 22 ────┼──→ Display 1 CLK
      │  MAX6675 Sensor 1   │ │ GPIO 21 ────┼──→ Display 1 DIO
      │  (Entrance/CS=5)    │ │ GPIO 25 ────┼──→ Display 2 CLK
      │  SCK  SO  CS  VCC GND│ │ GPIO 26 ────┼──→ Display 2 DIO
      └─────────────────────┘ │ GPIO 32 ────┼──→ Display 3 CLK
              ▲               │ GPIO 33 ────┼──→ Display 3 DIO
              │               │             │
      ┌─────────────────────┐ │ 3.3V ───────┼──→ All MAX6675 VCC
      │  MAX6675 Sensor 2   │ │ 5V ─────────┼──→ All TM1637 VCC
      │  (Middle/CS=4)      │ │ GND ────────┼──→ Common GND
      │  SCK  SO  CS  VCC GND│ └─────────────┘
      └─────────────────────┘
              ▲
              │
      ┌─────────────────────┐
      │  MAX6675 Sensor 3   │     ┌──────────────┐
      │  (Exit/CS=15)       │     │  TM1637 #1   │ Temperature Display
      │  SCK  SO  CS  VCC GND│     │ CLK DIO 5V G │ Zone: Entrance
      └─────────────────────┘     └──────────────┘

    Each sensor can use a unique SO/SCK/CS pin set. The diagram above shows the
    default mapping where SO and SCK are reused and only CS is unique.

                                  ┌──────────────┐
                                  │  TM1637 #2   │ Temperature Display
                                  │ CLK DIO 5V G │ Zone: Middle
                                  └──────────────┘

                                  ┌──────────────┐
                                  │  TM1637 #3   │ Temperature Display
                                  │ CLK DIO 5V G │ Zone: Exit
                                  └──────────────┘
```

---

## Document Information

- **Created:** March 7, 2026
- **Firmware Version:** v2.0 (3-Zone Monitor)
- **ESP-IDF Version:** v4.4.7
- **Target Hardware:** ESP32-WROOM-IE

For software documentation, see [README.md](README.md)
For hardware setup, see [HARDWARE_SETUP.md](HARDWARE_SETUP.md)
For quick start guide, see [QUICKSTART.md](QUICKSTART.md)
