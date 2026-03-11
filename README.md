# Oven Temperature Logger

Professional ESP32-WROOM-IE based temperature monitoring system for a 30-meter industrial oven using three MAX6675 thermocouple sensors with WiFi connectivity and SD card data logging.

## Project Overview

This project monitors temperature at three critical points along a 30m industrial oven:
- **Entrance** (Sensor 1): Temperature at oven inlet
- **Middle** (Sensor 2): Temperature at mid-span  
- **Exit** (Sensor 3): Temperature at oven outlet

Data is logged to an SD card for historical analysis and streamed via WiFi for real-time remote monitoring.

## Project Structure

```
oven_temp_logger/
├── main/
│   ├── main.c                    # Main application with task management
│   ├── CMakeLists.txt            # Main component configuration
│   └── Kconfig.projbuild         # Project configuration options
├── components/
│   ├── temp_sensor/
│   │   ├── temp_sensor.c         # Temperature sensor interface
│   │   ├── temp_sensor.h         # Temperature sensor header
│   │   ├── max6675.c             # MAX6675 driver implementation
│   │   ├── max6675.h             # MAX6675 driver header
│   │   └── CMakeLists.txt        # Component configuration
│   ├── sd_logging/
│   │   ├── sd_logging.c          # SD card logging implementation
│   │   ├── include/sd_logging.h  # SD card logging interface
│   │   └── CMakeLists.txt        # Component configuration
│   └── wifi_manager/
│       ├── wifi_manager.c        # WiFi management implementation
│       ├── include/wifi_manager.h # WiFi management interface
│       └── CMakeLists.txt        # Component configuration
├── CMakeLists.txt                # Top-level build configuration
├── sdkconfig                     # SDK build configuration
├── README.md                     # This file
└── .gitignore                    # Git ignore patterns
```

## Hardware Configuration

### ESP32-WROOM-IE Board
- 32-bit processor with dual-core
- Built-in WiFi (2.4 GHz b/g/n)
- 40 GPIO pins
- 4MB Flash memory

### GPIO Pin Assignments

| Device | Signal | GPIO | Pin # |
|--------|--------|------|-------|
| **MAX6675 Sensor 1** | SO (MISO) | 19 | - |
| | SCK (CLK) | 18 | - |
| | CS (Entrance) | 5 | - |
| **MAX6675 Sensor 2** | SO (MISO) | 34 (input-only) | - |
| | SCK (CLK) | 23 | - |
| | CS (Middle) | 17 | - |
| **MAX6675 Sensor 3** | SO (MISO) | 36 (input-only) | - |
| | SCK (CLK) | 13 | - |
| | CS (Exit) | 16 | - |
| **TM1637 Display 1** | CLK | 22 | - |
| | DIO | 21 | - |
| **TM1637 Display 2** | CLK | 25 | - |
| | DIO | 26 | - |
| **TM1637 Display 3** | CLK | 32 | - |
| | DIO | 33 | - |
| **SD Card (future)** | MISO (DO) | 35 (input-only) | - |
| | MOSI (DIN) | 14 | - |
| | CLK | 27 | - |
| | CS | 4 | - |

### Temperature Sensor Specifications (MAX6675)

- **Range:** -200°C to +700°C (Type K thermocouple)
- **Resolution:** 0.25°C per LSB (12-bit)
- **Interface:** SPI (Type-0)
- **Max Frequency:** 5 MHz
- **Features:**
  - Cold-junction compensated
  - Open thermocouple detection
  - Single supply operation (3.3-5V)

### Wiring Diagram

```
ESP32-WROOM-IE:
    GPIO19 (SO1)  <---- SO  (Sensor 1 - Entrance)   dedicated
    GPIO18 (SCK1) ----> SCK (Sensor 1 - Entrance)   dedicated
    GPIO5  (CS1)  ----> CS  (Sensor 1 - Entrance)

    GPIO34 (SO2)  <---- SO  (Sensor 2 - Middle)      dedicated, input-only
    GPIO23 (SCK2) ----> SCK (Sensor 2 - Middle)      dedicated
    GPIO17 (CS2)  ----> CS  (Sensor 2 - Middle)

    GPIO36 (SO3)  <---- SO  (Sensor 3 - Exit)        dedicated, input-only
    GPIO13 (SCK3) ----> SCK (Sensor 3 - Exit)        dedicated
    GPIO16 (CS3)  ----> CS  (Sensor 3 - Exit)

    GPIO35 (MISO) <---- DO  (SD Card - future)       input-only
    GPIO14 (MOSI) ----> DIN (SD Card - future)
    GPIO27 (CLK)  ----> CLK (SD Card - future)
    GPIO4  (CS)   ----> CS  (SD Card - future)

    GND ------------> GND (all devices)
    3.3V ------------> VCC (MAX6675, all)
    5V  ------------> VCC (TM1637, all)
```

## Prerequisites

- ESP-IDF 4.4.7 installed and configured
- ESP32 development board (ESP32-WROOM-IE)
- USB cable for flashing and monitoring
- Python 3.6 or higher
- 3x MAX6675 modules with Type K thermocouples
- SD card module with SPI interface
- SD card (any capacity, FAT32 formatted)

## Setup & Build

### 1. Set up ESP-IDF environment

If not already done:
```bash
export IDF_PATH=/Users/kraisinsongwatana/esp/v4.4.7/esp-idf
source $IDF_PATH/export.sh
```

### 2. Navigate to project directory

```bash
cd ~/esp/v4.4.7/oven_temp_logger
```

### 3. Configure the project (interactive menuconfig)

```bash
idf.py menuconfig
```

Navigate to "Oven Temperature Logger Configuration" to set:
- WiFi SSID and password
- Temperature alert thresholds
- Logging intervals
- GPIO pins (if different from defaults)

### 4. Build the project

```bash
idf.py build
```

### 5. Flash to ESP32

First, identify your serial port:
- macOS: `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART`
- Linux: `/dev/ttyUSB0` or `/dev/ttyACM0`
- Windows: `COM3`, `COM4`, etc.

Then flash:
```bash
idf.py -p /dev/cu.SLAB_USBtoUART flash
```

### 6. Monitor Serial Output

```bash
idf.py -p /dev/cu.SLAB_USBtoUART monitor
```

Press `Ctrl+]` to exit monitoring.

### Combined Build, Flash & Monitor

```bash
idf.py -p /dev/cu.SLAB_USBtoUART build flash monitor
```

## Application Features

### Temperature Sensing
- **3-Point Measurement:** Entrance, middle, and exit temperatures
- **Fast Sampling:** Configurable read interval (default 1000ms)
- **Error Detection:** Automatic thermocouple open-circuit detection
- **Dual Task Architecture:**
  - High-priority sensor reading task
  - Medium-priority data logging task

### Data Logging
- **SD Card Storage:** CSV format with timestamps
- **Daily Log Files:** Separate file for each date
- **Configurable Interval:** Default 5-second log interval (editable via menuconfig)
- **Automatic Cleanup:** Old log files can be purged based on retention policy

### WiFi Connectivity
- **Station Mode:** Connects to existing WiFi network
- **Auto-Reconnect:** Automatic reconnection on connection loss
- **Data Transmission:** Real-time temperature data upload capability
- **Signal Monitoring:** RSSI tracking (when connected)

### Temperature Alerts
- **Maximum Temperature Alert:** Triggers when any sensor exceeds threshold (default 250°C)
- **Minimum Temperature Alert:** Triggers when any sensor drops below threshold (default 20°C)
- **Log Alerts:** Displayed in serial output
- **Customizable Thresholds:** Set via menuconfig

## Configuration via menuconfig

Key configurable parameters:

```
MAX6675 Temperature Sensors:
  - SENSOR1_MISO_PIN / SENSOR1_CLK_PIN / SENSOR1_CS_PIN
  - SENSOR2_MISO_PIN / SENSOR2_CLK_PIN / SENSOR2_CS_PIN
  - SENSOR3_MISO_PIN / SENSOR3_CLK_PIN / SENSOR3_CS_PIN
  - SENSOR_READ_INTERVAL: 1000ms (adjust for faster/slower sampling)
  - Per-sensor SO/SCK/CS pin assignment

SD Card Configuration:
  - SD_CARD_CS_PIN: GPIO4
  - ENABLE_SD_LOGGING: Yes/No
  - LOG_INTERVAL_SECONDS: 5 (choose 1-3600)

WiFi Configuration:
  - ENABLE_WIFI: Yes/No
  - WIFI_SSID: Your network name
  - WIFI_PASSWORD: Your password
  - WIFI_RECONNECT_INTERVAL: 10 seconds

Temperature Alerts:
  - MAX_TEMPERATURE_ALERT: 250°C
  - MIN_TEMPERATURE_ALERT: 20°C
  - ENABLE_ALERTS: Yes/No
```

## Typical Output

Serial monitor output after startup:

```
=====================================
    Oven Temperature Logger v1.0
    ESP32-WROOM-IE
    ESP-IDF Version: v4.4.7
=====================================
Chip: ESP32 with 2 CPU cores
Flash: 2 MB embedded
Features: WiFi=yes, BLE=yes
=====================================

I (100) OvenTempLogger: Starting initialization sequence...
I (105) OvenTempLogger: Initializing MAX6675 temperature sensors...
I (110) TempSensor: Initializing temperature sensors
I (115) TempSensor: SPI Bus: MOSI=23, MISO=19, CLK=18
I (118) TempSensor: Sensor 1 (Entrance): CS=5
I (120) TempSensor: Sensor 2 (Middle): CS=17
I (123) TempSensor: Sensor 3 (Exit): CS=16
I (127) OvenTempLogger: Initializing SD card for data logging...
I (132) SDLogging: Initializing SD card on GPIO 4
I (175) OvenTempLogger: Initializing WiFi...
I (180) WiFiManager: Initializing WiFi
I (185) OvenTempLogger: Initialization complete!
I (190) OvenTempLogger: Application running...

I (1000) TempSensor: Entrance: 25.50°C | Middle: 26.00°C | Exit: 25.75°C
I (5000) SDLogging: Temperature data logged: E=25.50°C | M=26.00°C | Ex=25.75°C
```

## Temperature Data Log Format (CSV)

Logs are stored as `/sdcard/logs/YYYY-MM-DD.csv`:

```
timestamp,sensor_entrance_C,sensor_middle_C,sensor_exit_C,status
2026-03-04T08:30:45,25.50,26.00,25.75,OK
2026-03-04T08:30:50,25.75,26.25,26.00,OK
2026-03-04T08:30:55,26.00,26.50,26.25,OK
...
```

## API Reference

### Temperature Sensor Component

```c
// Initialize sensors
esp_err_t temp_sensor_init(int mosi_pin, int miso_pin, int clk_pin,
                           int sensor1_cs, int sensor2_cs, int sensor3_cs);

// Read all three sensors
esp_err_t temp_sensor_read_all(temp_readings_t *readings);

// Read single sensor
esp_err_t temp_sensor_read(int sensor_id, float *temperature);

// Check sensor status
bool temp_sensor_is_ready(int sensor_id);

// Cleanup
esp_err_t temp_sensor_deinit(void);
```

### SD Logging Component

```c
// Initialize SD card
esp_err_t sd_logging_init(int cs_pin);

// Log temperature data
esp_err_t sd_logging_record_data(const temp_readings_t *readings);

// Get SD info
esp_err_t sd_logging_get_info(uint64_t *total_size, uint64_t *used_size);

// List and cleanup
esp_err_t sd_logging_list_files(void);
esp_err_t sd_logging_cleanup_old_files(int days_to_keep);
```

### WiFi Manager Component

```c
// Initialize WiFi
esp_err_t wifi_manager_init(const char *ssid, const char *password);

// Register connection callback
esp_err_t wifi_manager_register_callback(wifi_event_callback_t callback);

// Check connection status
bool wifi_manager_is_connected(void);

// Get connection info
esp_err_t wifi_manager_get_info(char *ip_addr, int8_t *rssi);

// Send data
esp_err_t wifi_manager_send_data(const temp_readings_t *readings);
```

## Troubleshooting

### Serial Port Not Found
```bash
# macOS: List serial devices
ls /dev/cu.*

# Linux: List serial devices
ls /dev/tty*
```

### Build Errors
```bash
# Clean and rebuild
idf.py fullclean
idf.py build
```

### SD Card Not Detected
- Verify GPIO4 is correctly connected to SD module CS
- Check SD card is FAT32 formatted
- Ensure 3.3V power supply to SD module

### WiFi Connection Fails
- Verify SSID and password in menuconfig
- Check WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
- Test with 5dBm antenna setting if signal is weak

### Temperature Sensor Reads Zero/Invalid
- Verify thermocouple connection (not reversed)
- Check MAX6675 SPI connections:
  - CLK to GPIO18
  - MOSI to GPIO23
  - MISO to GPIO19
  - Correct CS pin to GPIO5/17/16
- Verify 3.3V power supply to MAX6675

## Performance Characteristics

| Parameter | Value |
|-----------|-------|
| **Sensor Update Rate** | 1000ms (configurable) |
| **Log Write Frequency** | 5 seconds (configurable) |
| **Temperature Resolution** | 0.25°C |
| **WiFi Upload Rate** | 5 seconds (configurable) |
| **Power Consumption** | ~80mA @ 3.3V (typical) |
| **Flash Used** | ~400KB (estimated) |
| **RAM Used** | ~100KB (estimated) |

## Future Enhancements

- [ ] MQTT protocol support for cloud integration
- [ ] Web dashboard for local network viewing
- [ ] Email/SMS alerts for threshold violations
- [ ] Backup to cloud storage (AWS S3, Google Cloud)
- [ ] Power consumption optimization
- [ ] Battery-backed real-time clock (RTC)
- [ ] Secondary sensor redundancy
- [ ] Predictive maintenance alerts

## Resources

- [MAX6675 Datasheet](https://datasheets.maximintegrated.com/en/ds/MAX6675.pdf)
- [ESP-IDF 4.4.7 Documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/)
- [ESP32 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf)
- [Type K Thermocouple Reference](https://www.omega.com/en-us/resources/thermocouples)

## License

MIT License

## Author Notes

- This project uses ESP-IDF 4.4.7 (long-term support version)
- SPI bus is shared between MAX6675 sensors and SD card with independent chip selects
- All GPIO pins are configurable via menuconfig for maximum flexibility
- The application uses FreeRTOS task prioritization for consistent sensor reading
- Timestamps in logs are UTC; adjust in firmware if local time needed
- SD card format: FAT32 recommended for maximum compatibility
