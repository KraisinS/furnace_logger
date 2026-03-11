# Quick Start Guide

Get your Oven Temperature Logger up and running in 5 minutes.

## Prerequisites

- ESP32-WROOM-IE development board
- 3x MAX6675 thermocouple modules with Type K thermocouples
- SD card module with microSD card
- USB cable (CH340 or CP2102 compatible)
- macOS/Linux system with terminal
- ESP-IDF 4.4.7 installed

## Quick Start Steps

### 1. Clone/Navigate to Project

```bash
cd ~/esp/v4.4.7/oven_temp_logger
```

### 2. Set Up Environment

```bash
source ~/esp/v4.4.7/esp-idf/export.sh
```

### 3. Configure WiFi (Important!)

```bash
idf.py menuconfig
```

- Navigate to: **Oven Temperature Logger Configuration** → **WiFi Configuration**
- Set your WiFi SSID
- Set your WiFi password
- Press `Q` to quit and save

### 4. Build & Flash

```bash
idf.py -p /dev/cu.SLAB_USBtoUART build flash monitor
```

If your port is different, use:
```bash
ls /dev/cu.* # macOS
ls /dev/tty* # Linux
```

### 5. Monitor Output

You should see startup messages like:

```
=====================================
    Oven Temperature Logger v1.0
    ESP32-WROOM-IE
=====================================
I (XXX) OvenTempLogger: Initializing MAX6675 temperature sensors...
I (XXX) OvenTempLogger: Initializing SD card for data logging...
I (XXX) OvenTempLogger: Initializing WiFi...
I (XXX) OvenTempLogger: Application running...
```

## What's Connected?

### GPIO Pins Used

| Pin | Device | Purpose |
|-----|--------|---------|
| GPIO5 | MAX6675 #1 | Entrance temperature |
| GPIO17 | MAX6675 #2 | Middle temperature |
| GPIO16 | MAX6675 #3 | Exit temperature |
| GPIO4 | SD Card | Data logging |
| GPIO18,19,23 | SPI Bus | All sensors use this |

See [HARDWARE_SETUP.md](./HARDWARE_SETUP.md) for detailed wiring.

## First Time Setup

### Hardware Connections

1. **Wire the SPI bus (shared by all sensors):**
   - ESP32 GPIO23 → All MAX6675 DIN pins
   - ESP32 GPIO19 → All MAX6675 DO pins
   - ESP32 GPIO18 → All MAX6675 CLK pins

2. **Wire individual chip selects:**
   - ESP32 GPIO5 → MAX6675 #1 CS
   - ESP32 GPIO17 → MAX6675 #2 CS
   - ESP32 GPIO16 → MAX6675 #3 CS

3. **Wire SD card:**
   - ESP32 GPIO4 → SD Card CS

4. **Connect power & ground:**
   - ESP32 3V3 → All VCC pins
   - ESP32 GND → All GND pins

5. **Connect thermocouples:**
   - Type K thermocouple → Each MAX6675's T+ and T- pins

### Verify Setup

```bash
# Connect via USB and monitor
idf.py -p /dev/cu.SLAB_USBtoUART monitor

# Press Enter to confirm connection
# You should see temperature readings
```

### Data Logging

Temperature data is logged to the SD card in `/sdcard/logs/YYYY-MM-DD.csv`:

```
timestamp,sensor_entrance_C,sensor_middle_C,sensor_exit_C,status
2026-03-04T08:30:45,25.50,26.00,25.75,OK
```

### WiFi Remote Monitoring

Once connected to WiFi, the device sends data that can be:
- Monitored via MQTT (when implemented)
- Sent to cloud services
- Displayed on web dashboard

## Configuration

### Key Settings (via menuconfig)

```bash
idf.py menuconfig
```

**Temperature Alerts:**
- Max threshold: Default 250°C
- Min threshold: Default 20°C

**Logging:**
- Sensor read interval: 1000ms (adjustable)
- SD log interval: 5 seconds (adjustable)

**WiFi:**
- Network name (SSID)
- Password
- Auto-reconnect interval

## Common Issues & Solutions

### "Serial port not found"

Find your USB device:
```bash
# macOS
ls /dev/cu.*

# Linux
ls /dev/ttyUSB*

# Windows
Mode COM1
```

### "Build failed"

Clean and rebuild:
```bash
idf.py fullclean
idf.py build
```

### "Temperature reads 0°C"

- Check thermocouple connections (not reversed)
- Verify MAX6675 SPI connections
- Look for error messages in monitor output

### "SD card not detected"

- Verify GPIO4 → SD Card CS connection
- Ensure SD card is FAT32 formatted
- Check 3.3V power supply to SD module

### "WiFi not connecting"

- Check SSID and password in menuconfig
- Ensure WiFi is 2.4GHz (ESP32 limitation)
- Check WiFi signal strength
- Look for error messages in monitor

## Full Documentation

For complete information, see:

- [README.md](./README.md) - Full project documentation
- [HARDWARE_SETUP.md](./HARDWARE_SETUP.md) - Detailed hardware wiring
- [MAX6675 Datasheet](https://datasheets.maximintegrated.com/en/ds/MAX6675.pdf)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/)

## Next Steps

1. **Test data logging:**
   - Check SD card has log files
   - Verify CSV format looks correct

2. **Set up monitoring:**
   - Configure WiFi data transmission
   - Set up MQTT broker (optional)

3. **Configure alerts:**
   - Adjust temperature thresholds for your oven
   - Enable email/SMS notifications (future)

4. **Optimize performance:**
   - Adjust sampling intervals via menuconfig
   - Fine-tune WiFi upload frequency

## Support

For issues or questions:
1. Check monitor output for error messages
2. Review [Troubleshooting](./README.md#troubleshooting) section
3. See [HARDWARE_SETUP.md](./HARDWARE_SETUP.md) for wiring verification

---

**Happy monitoring! 🌡️**
