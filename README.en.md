# ESP32-S3-RLCD-Monitor

[中文文档](README.md)

A desktop ornament that shows your DeepSeek real-time usage on a Waveshare ESP32-S3-RLCD-4.2 reflective-LCD display.

![device photo](device_photo.png)

## How it works

```
bridge daemon                              ESP32-S3-RLCD-4.2
─────────────                              ─────────────────
• fetches DeepSeek account balance         • connects to WiFi on boot
• fetches outdoor weather (open-meteo)     • polls GET /api/usage every 60 s
• extracts DeepSeek today/month tokens     • parses JSON with cJSON
  via ccusage (Claude Code session logs)   • drives LVGL centered UI:
• caches result, serves JSON on :7777        top    → time + weather + WiFi/battery
                                              middle → centered balance
                                              bottom → today/month tokens+cost+cache
                                             • reads indoor temp/RH (SHTC3)
                                             • shows time via NTP
```

### UI Layout

```
14:30 Overcast                    [≡]
FC 22-30°C / IN 26.3°C / RH 65%
           🐋 DEEPSEEK
              balance
              ¥37.42
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   TODAY              MONTH
  161M tokens       230M tokens
   ¥5.64              ¥8.08
 cache 99.0%        cache 99.0%
```

## Hardware

- [Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/wiki/ESP32-S3-RLCD-4.2) — 4.2" reflective LCD (paper-like), ESP32-S3, WiFi, RTC, temp/humidity, SD, audio.
- USB-C cable for flashing.

## Quick Start

### Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) — download the Universal Online Installer for Windows
- Node.js (optional, only needed for DeepSeek today/month token stats via ccusage)

### Build & Flash

```powershell
cd firmware
copy main\secrets.h.example main\secrets.h
# edit secrets.h: WiFi SSID/password, bridge URL

idf.py set-target esp32s3
idf.py build flash monitor
```

### Run the bridge

```powershell
cd bridge
set DEEPSEEK_API_KEY=sk-xxxxx
pip install fastapi uvicorn pydantic
python bridge.py
```

Test the API:
```bash
curl http://localhost:7777/api/usage
curl 'http://localhost:7777/api/usage?mock=1'  # mock data, no ccusage needed
```

## Power Saving

| Feature | Description |
|:--------|:------------|
| Modem Sleep | WiFi radio off between beacons, saves ~50mA |
| CPU Scaling | 80MHz idle / 240MHz burst |
| Light Sleep | Auto CPU pause when both cores idle |
| -Os compile optimization | Smaller binary, less flash access |
| AMPDU disabled | Small HTTP polls don't need aggregation |

**Estimated battery life** (2500mAh): ~15-25mA → **100-160 hours**

## License

MIT
