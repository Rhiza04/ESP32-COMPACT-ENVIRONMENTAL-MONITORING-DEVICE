# ESP32 Environmental Monitoring System

A dual-sensor IoT environmental monitoring system built using ESP32, DHT22, DS18B20, SSD1306 OLED, and Firebase Realtime Database.

## Overview

This project implements an embedded environmental monitoring system capable of measuring temperature and humidity using two independent sensors.

The system provides local sensor visualization through an OLED display and periodically synchronizes sensor measurements with Firebase Realtime Database for remote monitoring.

## Features

- **Dual Sensor Monitoring:** DHT22 for temperature and humidity, and DS18B20 for temperature.
- **OLED Visualization:** Displays sensor readings through a 128×64 SSD1306 OLED.
- **Physical Display Switching:** Switch between sensor displays using a push button.
- **Non-blocking Sensor Acquisition:** DS18B20 temperature conversion operates asynchronously.
- **Periodic Cloud Synchronization:** Uploads sensor measurements to Firebase every 15 minutes.
- **Firebase Server Timestamp:** Records server-generated timestamps for sensor uploads.
- **Sensor Error Detection:** Reports individual sensor statuses (`OK` / `ERROR`).
- **Wi-Fi Connectivity:** Connects to a wireless network for cloud communication.

## Hardware Components

| Component | Purpose |
|---|---|
| ESP32 WROOM | Main microcontroller |
| DHT22 | Temperature and humidity measurement |
| DS18B20 | Temperature measurement |
| SSD1306 OLED 128×64 | Sensor visualization |
| Push Button | Display mode switching |
| 4.7 kΩ resistor | DS18B20 pull-up resistor |

## Pin Configuration

| Component | ESP32 GPIO |
|---|---|
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| DHT22 DATA | GPIO 27 |
| DS18B20 DATA | GPIO 18 |
| Push Button | GPIO 32 |

## Software and Libraries

- Arduino IDE
- ESP32 Arduino Core
- Adafruit GFX Library
- Adafruit SSD1306 Library
- DHT Sensor Library
- OneWire Library
- DallasTemperature Library
- Firebase ESP Client
- ESP32 Wi-Fi Library

## System Architecture

The ESP32 collects data from both sensors independently. Measurements are displayed locally through the OLED and periodically uploaded to Firebase Realtime Database.

## Firebase Database Structure

```json
{
  "Sensors": {
    "DHT22": {
      "Temperature": 28.5,
      "Humidity": 65.2,
      "Status": "OK",
      "Timestamp": "07, November, 2026, 11:50:32",
      "TimestampUnixMs": 1794023432000
    },
    "DS18B20": {
      "Temperature": 27.8,
      "Status": "OK",
      "Timestamp": "07, November, 2026, 11:50:32",
      "TimestampUnixMs": 1794023432000
    }
  }
}
```

*Example data structure. Actual sensor values and timestamps will vary.*

## Monitoring Interval

| Operation | Interval |
|---|---|
| DHT22 acquisition | 2.5 seconds |
| DS18B20 acquisition | 2 seconds |
| DS18B20 conversion | 200 milliseconds |
| Firebase synchronization | 15 minutes |

## Installation and Configuration

1. Clone this repository.
2. Open the Arduino project in Arduino IDE.
3. Install the required libraries.
4. Configure your Wi-Fi credentials.
5. Configure your Firebase Realtime Database URL.
6. Configure Firebase database access rules.
7. Select your ESP32 board and upload the firmware.

**Security Notice:** Do not commit Wi-Fi passwords, private credentials, or unrestricted Firebase database rules. Configure appropriate Firebase authentication and database security rules before deploying this project publicly.

## Future Improvements

- Historical sensor data logging.
- Real-time cloud dashboard.
- Sensor data visualization and analytics.
- Configurable monitoring intervals.
- Improved Wi-Fi reconnection and upload retry mechanisms.
- Firebase authentication and database security rules.

## License

See the LICENSE file for licensing information.
