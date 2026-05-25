# firmware

ESP-IDF v5.x project for the Waveshare ESP32-S3-RLCD-4.2. Polls the bridge
daemon every `RLCD_POLL_SEC` seconds, renders a monospace terminal dashboard
on the 400×300 reflective LCD via LVGL v9.

## Layout

```
firmware/
├── CMakeLists.txt
├── partitions.csv
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── main.cpp              # entry; wires display + lvgl + UserApp_*
│   ├── user_config.h         # RLCD pins / resolution (from vendor demo)
│   ├── idf_component.yml     # lvgl/lvgl ^9.4.0
│   └── secrets.h.example     # → copy to secrets.h (gitignored)
└── components/
    ├── port_bsp/             # RLCD panel driver (verbatim from vendor 09_LVGL_V9_Test)
    ├── app_bsp/              # LVGL port: double-buffer in PSRAM + tick + lock (verbatim from vendor)
    ├── net_app/              # WiFi STA (blocking connect helper)
    ├── usage_client/         # esp_http_client + cJSON parse → usage_report_t
    ├── ui_app/               # our LVGL terminal-style dashboard
    └── user_app/             # glue: wifi init, ui init, polling task
```

`port_bsp/` and `app_bsp/` are an unmodified copy of the vendor demo at
`ESP32-S3-RLCD-4.2-Demo/02_ESP-IDF/09_LVGL_V9_Test/components/`. Don't
edit them — pull a refresh from the vendor zip if upstream changes.

## Build & flash

```bash
cd firmware
cp main/secrets.h.example main/secrets.h
$EDITOR main/secrets.h           # fill in WiFi + bridge URL + token

idf.py set-target esp32s3
idf.py build flash monitor
```

The first `idf.py build` will fetch `lvgl/lvgl@^9.4.0` via the IDF component
manager.

## What to expect

- Boot:
  - serial prints `connecting to <ssid>...` → `got IP ...`
  - LVGL initialises, screen shows `claude-code @ rlcd ~ $ status` +
    `waiting for bridge...`
- Within `RLCD_POLL_SEC` seconds the dashboard fills with real numbers.
- If the bridge becomes unreachable: header shows `(stale)`, numbers freeze.

## Verification

1. **Mock data**: set `RLCD_BRIDGE_URL` to `http://<bridge>/api/usage?mock=1`,
   flash, confirm UI shape matches `docs/ui-mockup.txt`.
2. **Live data**: switch to no query string, run Claude Code on the bridge
   host for a minute, watch the 5h tokens count up on the next poll.
3. **Resilience**: stop the bridge service — UI should mark stale and
   keep the last good numbers, no crash.

## Hardware reference (from vendor `user_config.h`)

```
RLCD: 400 × 300, monochrome via RGB565→threshold in flush callback
SPI3 — MOSI=12, SCK=11, DC=5, CS=40, RST=41 (TE=6)
I2C  — SDA=13, SCL=14  (used by onboard PCF85063 RTC and SHTC3, unused here)
```
