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

The firmware runs on an ESP32 driving a 64×64 HUB75 RGB LED matrix panel (FM6126A driver via I2S DMA). It acts as a home dashboard: on boot it connects to WiFi, fetches 24 hours of temperature history and ventilation status from Home Assistant, renders curves on the panel, then enters deep sleep after 3 minutes without PIR motion. A PIR sensor on GPIO 33 wakes it (ext0), and a timer wakeup is armed for the next predicted curve crossing.

### Component map

| File | Role |
|---|---|
| `src/main.cpp` | Entry point: wires up components, 10s refresh loop, deep sleep after 3min of PIR inactivity |
| `src/led-panel.cpp` | Wraps `MatrixPanel_I2S_DMA`; handles standby/wakeup via PIR |
| `src/dashboard.cpp` | Renders temperature curves, current value overlay, ventilation icon |
| `src/weather-service.cpp` | Fetches HA data into static float arrays; interpolates gaps; uses `RTC_DATA_ATTR` to persist ventilation state across deep sleeps |
| `src/home-assistant.cpp` | HTTP client for HA REST API; all calls go through the `haGet` helper; fetches time-series in 4-hour chunks with ArduinoJson filter to save RAM |
| `src/brightness.cpp` | Reads ADC pin 34 (light sensor), exponentially smoothed, to set display brightness |
| `src/buzzer.cpp` | Blocking buzzer; plays on ventilation state change |
| `src/pir-sensor.cpp` | Reads GPIO 33 motion sensor |
| `src/wifi-connection.cpp` | WiFi setup using `secrets.h` credentials; `loop()` reconnects on link loss |
| `include/colors.h` | RGB565 constants, computed at compile time (header-only) |

### Refresh cadence

`WeatherService::refresh()` has two tiers, because 64 px over 24 h means one column changes every ~22 min:

- **Every cycle (10 s)**: ventilation state only — it triggers the buzzer, so it must be seen quickly. 1 request.
- **Every 5 min** (`SLOW_REFRESH_MS`): 24 h history for both sensors (6 chunks each), crossing regression (2), sun times (1). 15 requests.

A failed ventilation read returns `"?"` and is ignored rather than treated as "off", so a network blip does not fire the buzzer.

### Display rendering pipeline

`Dashboard::loop()` → `WeatherService::refresh()` → draws fills (dimmed area under each curve), then line curves, then sun/moon icons, then current-value text overlay, then ventilation icon. Etage (upstairs) = blue, exterior = red. The x-axis maps 64 pixels to the last 24 hours of data.

### Time zone

`configTime(0, 0, ...)` deliberately sets the clock to **UTC**. `src/home-assistant.cpp` parses HA's UTC timestamps with `strptime` + `mktime`, and `mktime` interprets a `struct tm` as *local* time — so a non-UTC zone would shift every series. `timegm` is not available in the ESP32 newlib, hence the coupling. Do not change the zone without reworking `parseHaTimestamp`.

### Secrets

All credentials live in `include/secrets.h`, which is **gitignored and must be created locally** (it has never been committed): WiFi SSID/password and Home Assistant host + long-lived access token. Home Assistant entity IDs are hardcoded in `src/weather-service.cpp`: `sensor.domo_etage`, `sensor.domo_ext_rieur`, `input_boolean.etat_ventilation`.

### Hardware pin assignments

| Pin | Function |
|---|---|
| GPIO 32 | HUB75 E-pin (required for 64px tall panels) |
| GPIO 33 | PIR sensor input / deep-sleep ext0 wakeup |
| GPIO 34 | Light sensor (ADC, input-only) |
