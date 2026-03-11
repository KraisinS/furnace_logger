# PIN Configuration Summary

This document lists the GPIO pin assignments for the 3-sensor MAX6675 + 3-display TM1637 setup.

## Hardware: ESP32-WROOM-IE

### MAX6675 Thermocouple Sensors (Per-Sensor Pin Sets)

| Sensor | Zone | SO (MISO) | SCK (CLK) | CS |
|--------|------|-----------|-----------|----|
| Sensor 1 | Entrance | GPIO19        | GPIO18        | GPIO5  |
| Sensor 2 | Middle   | GPIO34 (*)    | GPIO23        | GPIO4  |
| Sensor 3 | Exit     | GPIO27        | GPIO13        | GPIO15 |

Notes:
- Each sensor has its own dedicated SO and SCK lines — no shared bus.
- (*) GPIO34 is input-only on ESP32, ideal for MISO/SO.
- Avoid GPIO16/GPIO17 for MAX6675 CS on ESP32-WROVER-IE; these pins are commonly tied to module PSRAM signals.
- Only VCC, GND, and common ground are shared between sensors.

### TM1637 4-Digit LED Displays

| Display | Zone | CLK | DIO |
|---------|------|-----|-----|
| Display 1 | Entrance | GPIO22 | GPIO21 |
| Display 2 | Middle   | GPIO25 | GPIO26 |
| Display 3 | Exit     | GPIO32 | GPIO33 |

## Pin Configuration via `idf.py menuconfig`

Run:

```bash
idf.py menuconfig
```

Navigate to:

- `Oven Temperature Logger Configuration -> Sensor Read Configuration`
   - `Sensor read interval (milliseconds)`

- `Oven Temperature Logger Configuration -> MAX6675 Temperature Sensors`
   - `MAX6675 Sensor 1 SO (MISO) GPIO pin`
   - `MAX6675 Sensor 1 SCK (CLK) GPIO pin`
   - `MAX6675 Sensor 1 CS GPIO pin`
   - `MAX6675 Sensor 2 SO (MISO) GPIO pin`
   - `MAX6675 Sensor 2 SCK (CLK) GPIO pin`
   - `MAX6675 Sensor 2 CS GPIO pin`
   - `MAX6675 Sensor 3 SO (MISO) GPIO pin`
   - `MAX6675 Sensor 3 SCK (CLK) GPIO pin`
   - `MAX6675 Sensor 3 CS GPIO pin`

- `Oven Temperature Logger Configuration -> TM1637 Display Configuration`
   - Display pin pairs and brightness

## Wiring Diagram (Per-Sensor Signal Model)

```text
ESP32-WROOM-IE
┌────────────────────────┐
│  Sensor 1 (Entrance)   │
│  GPIO19 ───────────────┼──→ MAX1 SO   (dedicated)
│  GPIO18 ───────────────┼──→ MAX1 SCK  (dedicated)
│  GPIO5  ───────────────┼──→ MAX1 CS
│                        │
│  Sensor 2 (Middle)     │
│  GPIO34 ───────────────┼──→ MAX2 SO   (dedicated, input-only)
│  GPIO23 ───────────────┼──→ MAX2 SCK  (dedicated)
│  GPIO4  ───────────────┼──→ MAX2 CS
│                        │
│  Sensor 3 (Exit)       │
│  GPIO27 ───────────────┼──→ MAX3 SO   (dedicated)
│  GPIO13 ───────────────┼──→ MAX3 SCK  (dedicated)
│  GPIO15 ───────────────┼──→ MAX3 CS
│                        │
│  GPIO22 ───────────────┼──→ TM1637 #1 CLK
│  GPIO21 ───────────────┼──→ TM1637 #1 DIO
│  GPIO25 ───────────────┼──→ TM1637 #2 CLK
│  GPIO26 ───────────────┼──→ TM1637 #2 DIO
│  GPIO32 ───────────────┼──→ TM1637 #3 CLK
│  GPIO33 ───────────────┼──→ TM1637 #3 DIO
│                        │
│  3.3V ─────────────────┼──→ MAX6675 VCC (all)
│  GND  ─────────────────┼──→ MAX6675 GND (all)
│  5V   ─────────────────┼──→ TM1637 VCC (if needed)
│                        │
│  SD Card (future)      │
│  GPIO35 ───────────────┼──→ SD MISO  (input-only)
│  GPIO14 ───────────────┼──→ SD MOSI
│  GPIO27 ───────────────┼──→ SD CLK
│  GPIO4  ───────────────┼──→ SD CS
│                        │
└────────────────────────┘
```

## Notes

1. MAX6675 is read-only. There is no MOSI pin for this device.
2. Use only 3.3V logic on MAX6675 signal lines.
3. If using an external power source for displays, tie grounds together:
    ESP32 GND, display power GND, and all sensor GND must be common.
4. For noise reduction, keep SO/SCK/CS wires short and avoid long parallel runs with power cables.

## Build and Flash

```bash
idf.py menuconfig
idf.py build
idf.py -p /dev/cu.usbserial-1460 flash monitor
```
