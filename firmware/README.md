# firmware

ESP-IDF v5.x project for the Waveshare ESP32-S3-RLCD-4.2. Polls the bridge
daemon over HTTP every `RLCD_POLL_SEC` seconds and renders a monospace
"terminal" dashboard via LVGL v9.

## Status

**Skeleton.** Display init and font are stubs — wire to vendor demo
`waveshareteam/ESP32-S3-RLCD-4.2/02_Example/ESP-IDF/09_LVGL_V9_Test/main/`
before first flash. See "Bring-up checklist" below.

## Bring-up checklist (M1)

1. Install ESP-IDF v5.x: <https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/get-started/>
2. Clone the vendor reference repo somewhere outside this project:
   ```bash
   git clone https://github.com/waveshareteam/ESP32-S3-RLCD-4.2.git ~/dev/RLCD-vendor
   ```
3. Build & flash `02_Example/ESP-IDF/09_LVGL_V9_Test` first — verifies the LCD
   and gets you the exact panel-driver init sequence. Note the screen resolution
   it reports.
4. Copy the panel init code from `09_LVGL_V9_Test/main/` into
   `main/drv/display.c` here (replacing the `TODO_…` placeholder).
5. Copy the WiFi connect from `02_Example/ESP-IDF/02_WIFI_STA/main/` into
   `main/net/wifi.c`.
6. Edit `main/secrets.h` (create from `secrets.h.example`) with your WiFi
   credentials and your bridge URL (e.g. `http://192.168.1.42:7777/api/usage`).
7. `idf.py set-target esp32s3 && idf.py build flash monitor`

## Configuration

`main/secrets.h` (not committed — see `.gitignore`):

```c
#define RLCD_WIFI_SSID     "your-ssid"
#define RLCD_WIFI_PASSWORD "your-pass"
#define RLCD_BRIDGE_URL    "http://192.168.1.42:7777/api/usage"
#define RLCD_POLL_SEC      60
```

## Layout

```
firmware/
  CMakeLists.txt              # idf project root
  sdkconfig.defaults          # PSRAM, LVGL, HTTP client knobs
  partitions.csv              # OTA-ready (factory + ota_0 + ota_1)
  main/
    CMakeLists.txt
    app_main.c                # boot: nvs -> wifi -> http task -> ui task
    secrets.h.example
    net/
      wifi.c / wifi.h         # connect + reconnect
      usage_client.c / .h     # HTTP GET + cJSON parse into usage_report_t
    ui/
      ui_main.c / ui_main.h   # main "terminal" screen
      ui_styles.c             # monospace font + palette
    drv/
      display.c / display.h   # RLCD panel init (PASTE FROM VENDOR DEMO)
      buttons.c / buttons.h   # board buttons (if any) for screen switch
```

## Verification

1. **Mock**: point `RLCD_BRIDGE_URL` at the bridge with `?mock=1` and confirm
   the canned numbers render correctly. Lets you iterate UI without real usage.
2. **Live**: switch to no query string, do a real Claude Code run, see the 5h
   token count tick up within `RLCD_POLL_SEC` seconds.
3. **Stale**: stop the bridge — UI should show "stale" header and freeze
   numbers, not crash.
