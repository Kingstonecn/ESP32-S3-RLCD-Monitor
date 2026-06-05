# ESP32-S3-RLCD-Monitor

[中文文档](README.md)

A desktop ornament that shows your DeepSeek real-time usage on a Waveshare ESP32-S3-RLCD-4.2 reflective-LCD board.

![device photo](device_photo.png)

## How it works

```
bridge daemon                              ESP32-S3-RLCD-4.2
─────────────                              ─────────────────
• fetches DeepSeek account balance         • connects to WiFi on boot
• fetches outdoor weather (open-meteo)     • polls GET /api/usage every 60 s
• extracts DeepSeek today/month tokens     • parses JSON with cJSON
  from ccusage (Claude Code session logs)  • drives LVGL centered UI:
• caches result, serves JSON on :7777        top    → balance
                                              middle → today/month tokens+cost+cache
                                             • reads indoor temp/RH (SHTC3)
                                             • shows time via NTP (CST-8)
```

## Hardware

- [Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/wiki/ESP32-S3-RLCD-4.2) — 4.2" reflective LCD (paper-like), ESP32-S3, WiFi, RTC, temp/humidity, SD, audio.
- USB-C cable for flashing.

## Quick start

### Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/)
- Node.js (for ccusage, optional — needed only for DeepSeek today/month token data)

### Build & flash

```bash
git clone https://github.com/CEJXXX/token-monitor-RLCD.git
cd ESP32-S3-RLCD-Monitor/firmware
cp main/secrets.h.example main/secrets.h
# edit secrets.h: WiFi SSID/pass + bridge URL
idf.py set-target esp32s3
idf.py build flash monitor
```

### Run the bridge

```bash
cd ESP32-S3-RLCD-Monitor/bridge
pip install fastapi uvicorn pydantic
DEEPSEEK_API_KEY=sk-... python bridge.py
```

## Project layout

```
ESP32-S3-RLCD-Monitor/
├── bridge/                    # Python FastAPI bridge daemon
│   ├── bridge.py              # main app + background refresh cache
│   ├── schema.py              # Pydantic response models
│   └── sources/
│       ├── deepseek.py        # DeepSeek balance API
│       └── weather.py         # open-meteo (no API key needed)
├── firmware/                  # ESP-IDF v5 + LVGL v9 project
│   ├── main/
│   │   ├── secrets.h.example
│   │   └── user_config.h
│   └── components/
│       ├── net_app/           # WiFi STA + NTP
│       ├── sensor/            # SHTC3 temp/humidity
│       ├── usage_client/      # HTTP poll + cJSON parse
│       └── ui_app/            # LVGL dashboard + icons
├── scripts/
└── docs/
```

## License

MIT
