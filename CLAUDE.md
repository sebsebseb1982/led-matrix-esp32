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

The firmware runs on an ESP32 driving a 64×64 HUB75 RGB LED matrix panel (FM6126A driver via I2S DMA). It acts as a home dashboard: on boot it connects to WiFi, fetches 18 hours of temperature history, an hourly exterior forecast and ventilation status from Home Assistant, renders curves on the panel, then enters deep sleep after 3 minutes without PIR motion. A PIR sensor on GPIO 33 wakes it (ext0), and a timer wakeup is armed for the next predicted curve crossing.

### Component map

| File | Role |
|---|---|
| `src/main.cpp` | Entry point: wires up components, 10s refresh loop, deep sleep after 3min of PIR inactivity |
| `src/led-panel.cpp` | Wraps `MatrixPanel_I2S_DMA`; handles standby/wakeup via PIR |
| `src/dashboard.cpp` | Renders temperature curves, current value overlay, ventilation icon |
| `src/weather-service.cpp` | Fetches HA data into static float arrays; interpolates gaps; uses `RTC_DATA_ATTR` to persist ventilation state across deep sleeps |
| `src/home-assistant.cpp` | HTTP client for HA REST API; all calls go through the `haRequest` helper (`haGet` wrapper for GETs; POST for the `weather.get_forecasts` service); fetches time-series in 4-hour chunks with ArduinoJson filter to save RAM |
| `src/brightness.cpp` | Reads ADC pin 34 (light sensor), exponentially smoothed, to set display brightness |
| `src/buzzer.cpp` | Blocking buzzer; plays on ventilation state change |
| `src/pir-sensor.cpp` | Reads GPIO 33 motion sensor |
| `src/wifi-connection.cpp` | WiFi setup using `secrets.h` credentials; `loop()` reconnects on link loss |
| `include/colors.h` | RGB565 constants, computed at compile time (header-only) |

### Refresh cadence

`WeatherService::refresh()` has two tiers, because one column covers ~23 min:

- **Every cycle (10 s)**: ventilation state only — it triggers the buzzer, so it must be seen quickly. 1 request.
- **Every 5 min** (`SLOW_REFRESH_MS`): 18 h history for both sensors (5 chunks each), hourly forecast from `weather.talence` (1), thermostat mode + setpoint (1), heating periods from `climate.thermostat` (5 chunks, attribute `hvac_action == heating`, drawn as a red tint under the etage curve), sun times (1). 17 requests. The crossing comes from the simulated etage vs the forecast (0 requests); fallbacks: etage regression vs forecast (1), or regression of both sensors (2) without forecast.
- **Every hour** (`MODEL_REFIT_SECS`, timestamp in RTC memory): refit of the etage thermal model `dTin/dt = a·(Text − Tin) + b·heating + c` on 48 h of history (12 h chunks: 4 per sensor + 4 for heating = 12 requests). Coefficients live in `RTC_DATA_ATTR` so a PIR wake does not refetch. 48 h, not 18: a backtest on a week of real data showed the 18 h fit is worse than a flat prediction, while the 48 h fit beats the linear trend (0.23 vs 0.26 °C RMSE at +6 h). Ventilation and solar terms made it worse and were left out. The simulation floors the prediction at the setpoint when the thermostat is in `heat` mode, since `b` stays 0 when no heating happened in the window.

A failed ventilation read returns `"?"` and is ignored rather than treated as "off", so a network blip does not fire the buzzer.

### Display rendering pipeline

`Dashboard::loop()` → `WeatherService::refresh()` → `render()` twice (forecast curves dimmed into `frameDim`, then bright into `frameLit`): fills (dimmed area under each curve, twice as dim in the forecast zone), then line curves, then sun/moon icons, then current-value text overlay, then ventilation icon. Etage (upstairs) = blue, exterior = red.

X axis (`include/weather-service.h`): columns `0..NOW_COL` (47) are the last `HISTORY_HOURS` (18 h); columns after `NOW_COL` are predictions at the same scale (~6 h), with a fainter fill and a scanner animation on the curves: exterior = forecast shifted by a constant bias so it matches the last sensor reading at `NOW_COL`; etage = thermal model simulated in 5 min steps from the last reading, driven by that forecast.

Scanner (K2000-style, left to right only): `Dashboard::animate()` runs on every `main.cpp` loop iteration and, every `SCAN_FRAME_MS`, redraws each recorded forecast-curve pixel as a blend of its `frameDim` and `frameLit` colors, based on its distance to a head sweeping from `NOW_COL` to the right edge (bright head, `SCAN_TAIL`-column fading trail, then `SCAN_PAUSE_MS` pause). Overlays are drawn in both frames, so where they cover the curve both frames match and nothing changes. The animation freezes while `loop()` blocks on HA requests.

### Time zone

`configTime(0, 0, ...)` deliberately sets the clock to **UTC**. `src/home-assistant.cpp` parses HA's UTC timestamps with `strptime` + `mktime`, and `mktime` interprets a `struct tm` as *local* time — so a non-UTC zone would shift every series. `timegm` is not available in the ESP32 newlib, hence the coupling. Do not change the zone without reworking `parseHaTimestamp`.

### Secrets

All credentials live in `include/secrets.h`, which is **gitignored and must be created locally** (it has never been committed): WiFi SSID/password and Home Assistant host + long-lived access token. Home Assistant entity IDs are hardcoded in `src/weather-service.cpp`: `sensor.domo_etage`, `sensor.domo_ext_rieur`, `input_boolean.etat_ventilation`, `climate.thermostat`, `weather.talence` (forecast).

### Hardware pin assignments

| Pin | Function |
|---|---|
| GPIO 32 | HUB75 E-pin (required for 64px tall panels) |
| GPIO 33 | PIR sensor input / deep-sleep ext0 wakeup |
| GPIO 34 | Light sensor (ADC, input-only) |
