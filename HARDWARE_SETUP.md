# Hardware Setup Guide

## Overview

This document provides detailed hardware setup instructions for the Oven Temperature Logger project.

## Bill of Materials (BOM)

| Qty | Component | Part Number | Notes |
|-----|-----------|-------------|-------|
| 1   | ESP32-WROOM-IE | ESP32-WROOM-IE(M612E68H12F0ZXH) | 32-bit dual-core, WiFi |
| 3   | MAX6675 Module | MAX6675+K | SPI thermocouple reader |
| 3   | Type K Thermocouple | - | Temperature sensors (30m total) |
| 1   | SD Card Module | SPI/microSD | Data logging storage |
| 1   | SD Card | 8GB+ microSD | FAT32 formatted |
| 1   | USB to Serial | CH340 or CP2102 | For flashing and monitoring |
| 1   | Breadboard | 830 points | Prototyping |
| 20  | Jumper Wires | M-M | GPIO connections |
| 1   | 3.3V Power Supply | LM1117-3.3 | For sensors (if separate) |

## Pinout Reference

### ESP32-WROOM-IE GPIO Pinout

```
                        ESP32-WROOM-IE
 
     GND [  1] [GND]  2
     3V3 [  3] [EN]   4
  GPIO36 [  5] [GPIO0] 6
  GPIO39 [  7] [GPIO2] 8
  GPIO34 [  9] [GPIO4 - SD CS] 10
  GPIO35 [ 11] [GPIO5 - MAX6675 CS1] 12
  GPIO32 [ 13] [GPIO18 - CLK] 14
  GPIO33 [ 15] [GPIO19 - MISO] 16
  GPIO25 [ 17] [GPIO23 - MOSI] 18
  GPIO26 [ 19] [GPIO1 - TX] 20
  GPIO27 [ 21] [GPIO3 - RX] 22
  GPIO14 [ 23] [GPIO22] 24
  GPIO12 [ 25] [GPIO21] 26
  GPIO13 [ 27] [GND] 28
  GPIO9  [ 29] [GPIO11] 30
  GPIO10 [ 31] [GPIO6] 32
  GPIO11 [ 33] [GPIO7] 34
  GPIO6  [ 35] [GPIO8] 36
  GPIO7  [ 37] [GPIO19 - MISO*] 38  * also on pin 16
  GPIO8  [ 39] [USB+] 40
  GPIO15 [PWM] [USB-] 42
  GPIO2  [ADC] [GND] 44
```

## Step-by-Step Connection Guide

### Step 1: ESP32-WROOM-IE to USB Serial Adapter

Connect for programming and monitoring:

```
ESP32 GPIO1 (TX)  → USB Serial RX
ESP32 GPIO3 (RX)  → USB Serial TX
ESP32 GND        → USB Serial GND
ESP32 3V3 (if needed) → USB Serial 3.3V
```

### Step 2: SPI Bus Setup (Shared for MAX6675 & SD Card)

**Clock and MISO lines (shared by all devices):**

```
ESP32 GPIO19 (MISO) → MAX6675 #1 SO (Serial Output)
                   → MAX6675 #2 SO
                   → MAX6675 #3 SO
                   → SD Card DO

ESP32 GPIO18 (CLK)  → MAX6675 #1 SCK
                   → MAX6675 #2 SCK
                   → MAX6675 #3 SCK
                   → SD Card CLK
```

**Note:** MAX6675 is read-only (no MOSI needed). It only outputs data via SO pin.

**MOSI line (for SD Card only):**

```
ESP32 GPIO23 (MOSI) → SD Card DIN
```

### Step 3: MAX6675 Chip Select Lines (Individual per Sensor)

```
ESP32 GPIO5  → MAX6675 #1 CS (Entrance)
ESP32 GPIO17 → MAX6675 #2 CS (Middle)
ESP32 GPIO16 → MAX6675 #3 CS (Exit)
```

### Step 4: SD Card Module

```
ESP32 GPIO4 → SD Card CS
```

### Step 5: Power Distribution

```
ESP32 3V3 → MAX6675 #1 VCC
        → MAX6675 #2 VCC
        → MAX6675 #3 VCC
        → SD Card VCC

ESP32 GND → MAX6675 #1 GND
        → MAX6675 #2 GND
        → MAX6675 #3 GND
        → SD Card GND
        → USB Serial GND
```

### Step 6: Type K Thermocouple Connections

Connect to each MAX6675 module:

```
MAX6675 #1 (Entrance):
  Thermocouple + (Yellow/Red) → T+
  Thermocouple - (Red)        → T-

MAX6675 #2 (Middle):
  Thermocouple + → T+
  Thermocouple - → T-

MAX6675 #3 (Exit):
  Thermocouple + → T+
  Thermocouple - → T-
```

**Important:** Thermocouple polarity is critical for accurate readings.

## Breadboard Wiring Layout Example

```
                    Power Rail
  [3V3]─────────────────────────[GND]
    ↓                              ↓
  ┌─────────────────────────────────┐
  │  MAX6675#1  MAX6675#2  MAX6675#3│
  │   VCC GND    VCC GND    VCC GND │
  │    ↓  ↓      ↓  ↓       ↓  ↓   │
  │   [1][·]    [·][·]     [·][·]  │
  │   [·][·]    [·][·]     [·][·]  │
  │   [·][·]    [·][·]     [·][·]  │
  │   CS DIN DO CS DIN DO   CS DIN DO
  │    ↓  ↓  ↓   ↓  ↓  ↓    ↓  ↓  ↓
  ├────┼──┼──┼───┼──┼──┼────┼──┼──┼──┐
  │ G5 │23│19│18│… │  │ G4 │  │  │  │
  └────┴──┴──┴────┴──┴──────┴──┴──┴──┘
        ESP32 GPIO pins
```

## Troubleshooting Connections

### MAX6675 Not Reading
1. **Check SPI polarity:**
   - CPOL=0 (clock idle low)
   - CPHA=0 (sample on leading edge)

2. **Verify CS timing:**
   - CS should go low before transmission
   - CS should go high after transmission

3. **Thermocouple polarity:**
   - Yellow/Red → T+
   - Red → T-
   - Reversed = negative readings

### SD Card Not Detected
1. **Signal integrity:**
   - Use short wires (< 10cm) if possible
   - Add 10k pull-up resistors on CS lines if needed

2. **Power supply:**
   - SD card modules typically need 200mA during write
   - Use separate 3.3V regulator if needed

3. **Card format:**
   - Format SD card as FAT32
   - Recommended: 4KB cluster size

### WiFi Module Issues
- ESP32-WROOM-IE has built-in antenna
- Keep away from other 2.4GHz devices
- Position antenna away from metallic components

## Recommended Safety Measures

1. **Thermocouple Safety:**
   - Thermocouples can conduct high voltage
   - Insulate exposed wires
   - Consider shielded cables for EMI-prone environments

2. **Power Supply:**
   - Use protected 3.3V supply (1A minimum)
   - Add 100µF decoupling capacitor near ESP32 VCC
   - Add 10µF + 100nF capacitors near each MAX6675

3. **EMI Protection:**
   - Keep signal wires away from power wires
   - Use twisted pairs for thermocouple lines
   - Consider ferrite beads on SPI lines if long runs

## Testing Connections

Before uploading code:

1. **Continuity testing:**
   ```bash
   # Check for shorts between 3V3 and GND
   # Use multimeter in continuity mode
   ```

2. **Power supply voltage:**
   ```bash
   # Measure 3.3V on all VCC pins
   # Should read 3.2-3.4V
   ```

3. **SPI clock signal:**
   - Connect oscilloscope to GPIO18 (CLK)
   - Should show ~1MHz square wave during SPI operations

4. **Thermocouple readings:**
   - Connect thermocouple to room temperature (or ice bath/boiling water)
   - Read raw SPI data to verify responses

## Production Considerations

- **PCB design:** Use multi-layer board with ground plane
- **Connector type:** M23 connectors for thermocouple inputs
- **Enclosure:** IP65 or higher for industrial environment
- **Cooling:** Add heat sinks if devices dissipate heat
- **Cable routing:** Separate signal and power cables

## Component Links

- [MAX6675 Datasheet](https://datasheets.maximintegrated.com/en/ds/MAX6675.pdf)
- [ESP32-WROOM-IE Datasheet](https://www.espressif.com/en/products/socs/esp32)
- [Type K Thermocouple Color Code](https://www.omega.com/en-us/resources/thermocouples)
