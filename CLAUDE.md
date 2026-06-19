# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Flash Commands

This project uses PlatformIO (target: `esp32dev`).

```bash
pio run                          # build only
pio run --target upload          # build and flash to ESP32
pio device monitor               # open serial monitor (115200 baud)
pio run --target upload && pio device monitor  # flash then monitor
```

## Architecture

The firmware runs on an ESP32 driving a 64×64 HUB75 RGB LED matrix panel (FM6126A driver via I2S DMA). It acts as a home dashboard: on boot it connects to WiFi, fetches 24 hours of temperature history and ventilation status from Home Assistant, renders curves on the panel, then enters deep sleep after 60 seconds. A PIR sensor on GPIO 33 can wake the display from standby mid-cycle.

### Component map

| File | Role |
|---|---|
| `src/main.cpp` | Entry point: wires up components, 10s refresh loop, 60s deep sleep |
| `src/led-panel.cpp` | Wraps `MatrixPanel_I2S_DMA`; handles standby/wakeup via PIR |
| `src/dashboard.cpp` | Renders temperature curves, current value overlay, ventilation icon |
| `src/weather-service.cpp` | Fetches HA data into static float arrays; interpolates gaps; uses `RTC_DATA_ATTR` to persist ventilation state across deep sleeps |
| `src/home-assistant.cpp` | HTTP client for HA REST API; fetches time-series in 4-hour chunks with ArduinoJson filter to save RAM |
| `src/brightness.cpp` | Reads ADC pin 34 (light sensor) to set display brightness |
| `src/buzzer.cpp` | Non-blocking buzzer; plays on ventilation state change |
| `src/pir-sensor.cpp` | Reads GPIO 33 motion sensor |
| `src/wifi-connection.cpp` | WiFi setup using `secrets.h` credentials |
| `src/colors.cpp` | RGB565 color helpers for the DMA display |

### Display rendering pipeline

`Dashboard::loop()` → `WeatherService::refresh()` → draws fills (dimmed area under each curve), then line curves, then current-value text overlay, then ventilation icon. Etage (upstairs) = blue, exterior = red. The x-axis maps 64 pixels to the last 24 hours of data.

### Secrets

All credentials live in `include/secrets.h` (tracked in git): WiFi SSID/password and Home Assistant host + long-lived access token. To adapt to a different environment, update these defines. Home Assistant entity IDs are hardcoded in `src/weather-service.cpp`: `sensor.temperature_etage`, `sensor.domo_ext_rieur`, `input_boolean.etat_ventilation`.

### Hardware pin assignments

| Pin | Function |
|---|---|
| GPIO 32 | HUB75 E-pin (required for 64px tall panels) |
| GPIO 33 | PIR sensor input / deep-sleep ext0 wakeup |
| GPIO 34 | Light sensor (ADC, input-only) |
