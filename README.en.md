# ESP32-S3-RLCD-Monitor

[中文文档](README.md)

A desktop ornament that shows your **DeepSeek** usage + **OpenCode Go** quota on a Waveshare ESP32-S3-RLCD-4.2 reflective-LCD display. **DeepSeek + OpenCode only** — no Claude data included.

![device photo](device_photo.png)

## How it works

```
~/.claude/**/*.jsonl           (Claude Code session logs → ccusage parses DeepSeek tokens)
         │
         ▼
   bridge daemon                              ESP32-S3-RLCD-4.2
   ──────────────                              ─────────────────
   • invokes ccusage for DeepSeek tokens      • connects to WiFi on boot
   • fetches DeepSeek balance (official API)  • GET /api/usage every 300 s
   • fetches outdoor weather (open-meteo)     • parses JSON with cJSON
   • scrapes OpenCode Go quota                • single-column UI (switchable views):
   • caches result, serves JSON on :7777           ▸ DeepSeek: centered balance + today/month tokens
                                                   ▸ OpenCode: 5h / week / month progress bars
                                                 • reads indoor temp/humidity (SHTC3)
                                                 • NTP time sync (CST-8)
                                                 • GPIO18 button → instant view switch
```

The bridge runs as a background process (`pythonw.exe` on Windows, systemd on Linux) on the same machine as Claude Code. A background thread refreshes the cache every 45 seconds (configurable via `RLCD_REFRESH_SEC`), so the ESP32 always reads from a warm HTTP cache.

DeepSeek `today_tokens` comes from ccusage parsing Claude Code logs; `balance` comes from the official DeepSeek API. OpenCode Go quota is scraped from its web dashboard (no public API).

### UI Layout

**DeepSeek mode (default):**

```
14:30 Overcast                    [≡]
FC 22-30°C / IN 26.3°C / RH 65%
           🐋 DEEPSEEK
              balance
              ¥ 70.79
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 TODAY              MONTH
 2.4M tokens       18.0M tokens
  ¥5.42              ¥38.15
 cache 96.8%        cache 95.2%
```

**OpenCode Go mode (press GPIO18 to switch):**

```
14:30 Overcast                    [≡]
FC 22-30°C / IN 26.3°C / RH 65%
           OPENCODE
             Go Plan
───────────────────────────────────────
5 HOUR  [████░░░░]  62%    reset in 2h 14m
WEEK    [████░░░░]  41%    reset in 2d 22h
MONTH   [████░░░░]  68%    reset in 12d 5h
```

Press GPIO18 to toggle between DeepSeek ↔ OpenCode instantly — no need to wait for the next HTTP poll (the firmware caches the last successful response locally).

## Hardware

- **[Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/wiki/ESP32-S3-RLCD-4.2)** — 4.2" reflective LCD (paper-like, 400×300 1-bit), ESP32-S3, WiFi, RTC, SHTC3 temp/humidity, SD slot, audio.
- **USB-C cable** for flashing.

## Architecture

```
Windows / Linux host                           ESP32-S3-RLCD-4.2
───────────────                                ─────────────────
~/.claude/**/*.jsonl                           LVGL single-column UI
        │                                              ▲
        ▼            LAN HTTP (300 s)                   │
   bridge daemon ── GET /api/usage ─────────────────────┘
   (ccusage + DeepSeek API + OpenCode scrape)
   :7777

                                            ┌─ GPIO18 KEY ─────┐
                                            │ DeepSeek ↔ OpenCode│
                                            │ instant, cached   │
                                            └──────────────────┘
```

### Components

| Component | Path | Description |
|:----------|:-----|:------------|
| **Bridge** | `bridge/` | Python FastAPI daemon that aggregates all data and serves JSON on `:7777` |
| **Firmware** | `firmware/` | ESP-IDF + LVGL v9 project, renders the dual-view dashboard |
| **Button** | `user_app.cpp` | GPIO18 key detection — switches view + wakes the poll task |
| **Scripts** | `scripts/` | Launcher scripts, icon generators, Linux systemd installer, etc. |

## Quick Start

### 1. Install prerequisites

- **ESP-IDF v5.x** — [download the Universal Online Installer](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/), target chip `esp32s3`
- **Node.js** — optional, only needed for DeepSeek today/month token stats (ccusage)

### 2. Clone the repo

```powershell
git clone https://github.com/Kingstonecn/ESP32-S3-RLCD-Monitor.git
cd ESP32-S3-RLCD-Monitor
```

### 3. Start the bridge daemon

```powershell
cd bridge

:: Install dependencies (pick one)
pip install fastapi uvicorn pydantic python-dotenv
:: or: uv sync  (uses pyproject.toml automatically)

:: Create .env (already in .gitignore)
:: Fill in the following (see env-vars section):
::   DEEPSEEK_API_KEY=sk-xxxxx
::   OPENCODE_GO_WORKSPACE_ID=wrk_xxxxx
::   OPENCODE_GO_AUTH_COOKIE=Fe26.2**...

:: Start bridge in foreground (for debugging)
python bridge.py
```

Verify the API:

```powershell
curl http://localhost:7777/api/usage          # live data
curl "http://localhost:7777/api/usage?mock=1" # mock data (no ccusage / DeepSeek API)
curl http://localhost:7777/healthz            # health check
```

### 4. Configure auto-start on Windows

Two launchers are provided:

```powershell
:: Launcher 1: background with a brief cmd window
scripts\start_bridge.bat

:: Launcher 2: fully silent window (recommended for startup)
scripts\start_bridge.vbs

:: To enable auto-start: put start_bridge.vbs in the startup folder
:: Win+R → shell:startup → copy start_bridge.vbs into it
```

### 5. Build & flash the firmware

#### Configure secrets

```powershell
cd firmware
copy main\secrets.h.example main\secrets.h
# Edit secrets.h: fill in WiFi SSID/password and bridge URL
```

**secrets.h fields:**

| Field | Example | Description |
|:------|:--------|:------------|
| `RLCD_WIFI_SSID` | `"MyNetwork"` | 2.4 GHz only (ESP32 doesn't support 5 GHz) |
| `RLCD_WIFI_PASSWORD` | `"password"` | WPA2 |
| `RLCD_BRIDGE_URL` | `"http://192.168.0.129:7777/api/usage"` | LAN address of the bridge |
| `RLCD_BRIDGE_TOKEN` | `""` | Must match `RLCD_AUTH_TOKEN` in `.env`; leave empty if unused |
| `RLCD_POLL_SEC` | `300` | Poll interval in seconds; pressing GPIO18 also triggers an immediate poll |

#### Build & flash

```powershell
:: Only needed once
idf.py set-target esp32s3

:: Build → flash → monitor
idf.py build flash monitor
```

It's recommended to test with mock mode first: append `?mock=1` to `RLCD_BRIDGE_URL` and verify the display renders correctly before switching to live data.

### 6. Verification

On successful connection, the serial monitor prints `connecting to <SSID>...` → `got IP`, and the dashboard fills in data piece by piece. Press GPIO18 to switch between DeepSeek / OpenCode views.

## Environment variables

Create `bridge/.env` (already in `.gitignore`) and fill in as needed:

```ini
# === Required ===
DEEPSEEK_API_KEY=sk-xxxxx          # DeepSeek balance display

# === OpenCode Go (optional — omit to disable OpenCode view) ===
OPENCODE_GO_WORKSPACE_ID=wrk_xxxxx           # From the Dashboard URL
OPENCODE_GO_AUTH_COOKIE=Fe26.2**...          # Browser cookie after login

# === Weather (optional — defaults to open-meteo, no API key needed) ===
RLCD_WEATHER_LAT=31.2304           # Latitude (default: Shanghai)
RLCD_WEATHER_LON=121.4737          # Longitude
RLCD_WEATHER_CITY=Shanghai         # City name shown on device (≤8 chars)

# === Security (required for non-LAN access) ===
RLCD_AUTH_TOKEN=<random-string>    # Must match RLCD_BRIDGE_TOKEN in secrets.h

# === Misc ===
# RLCD_REFRESH_SEC=45              # Background cache refresh interval (default 45s)
# RLCD_USD_CNY=7.25                # USD→CNY exchange rate
```

Restart the bridge after changing `.env`.

## Button behavior

| Action | Function |
|:-------|:---------|
| Press GPIO18 | Toggle DeepSeek ↔ OpenCode view |
| On press | Also resets the HTTP failure counter and wakes the poll task immediately |

The switch is **instant** — the firmware caches the last successful response, so there's no need to wait for the next HTTP poll (up to 300s). Each press also triggers a fresh HTTP request; the result is applied on the next `ui_app_update` cycle.

## OpenCode Go quota notes

OpenCode Go has no public REST API; quota data is scraped from the web dashboard. To obtain the **workspace ID** and **auth cookie**:

1. Log in to the [OpenCode Go Dashboard](https://opencode.ai/workspace/your-workspace/go)
2. F12 → Network → refresh → copy the `Cookie` header from any request
3. Fill in `workspace_id` and the full `Cookie` value in `.env`

If scraping fails, `opencode_go` returns null, the device keeps the last valid data, and view switching still works.

## Failure recovery

| Scenario | Behavior |
|:---------|:---------|
| DeepSeek API unavailable | `deepseek` field null, balance shows dashes |
| 3 consecutive HTTP poll failures | Polling pauses, screen retains last data; press GPIO18 to resume |
| OpenCode scrape fails | OpenCode view shows blank, no crash |
| WiFi timeout (20s) | Boots with power-saving disabled, background auto-reconnect |

## Power saving

| Feature | Status | Description |
|:--------|:-------|:------------|
| Modem Sleep | ✅ | WiFi radio off when idle |
| CPU scaling | ✅ | 80 MHz idle / 240 MHz burst |
| Light Sleep | ✅ | Auto CPU pause when both cores idle |
| HTTP poll 300s | ✅ | Increased from 60s to 300s, fewer wake-ups |
| Battery sampling sparse | ✅ | ADC read every 2 polls (≈10 min), 4-sample average |
| SHTC3 throttled | ✅ | Temp/humidity read every 60s |
| -Os optimization | ✅ | sdkconfig.defaults |
| AMPDU disabled | ✅ | Small HTTP requests don't need aggregation |

**Estimated battery life** (2500 mAh Li-Ion): ~15–25 mA average → **approximately 100–160 hours**

## Project structure

```
ESP32-S3-RLCD-Monitor/
├── bridge/                          # Python FastAPI bridge daemon
│   ├── bridge.py                    # Main entry + background cache refresh
│   ├── schema.py                    # Pydantic response model (incl. OpenCodeGo)
│   ├── .env                         # Local config (gitignored)
│   ├── pyproject.toml               # Python project metadata (uv/pip)
│   └── sources/
│       ├── deepseek.py              # DeepSeek balance API
│       ├── claude_local.py          # ccusage integration (DeepSeek token extraction)
│       ├── claude_limits.py         # Reads /run/rlcd/claude-limits.json
│       ├── opencode.py              # OpenCode Go quota scraper
│       └── weather.py               # Open-meteo weather (no API key)
├── firmware/                        # ESP-IDF v5 + LVGL v9 project
│   ├── main/
│   │   ├── secrets.h.example        # → copy to secrets.h (gitignored)
│   │   ├── secrets.h                # WiFi/bridge config
│   │   ├── user_config.h            # Pin definitions
│   │   └── main.cpp                 # Entry point: init display, UI, WiFi, polling
│   └── components/
│       ├── net_app/                 # WiFi STA + NTP (CST-8)
│       ├── sensor/                  # SHTC3 temp/humidity I2C
│       ├── usage_client/            # HTTP polling + cJSON parsing
│       ├── ui_app/                  # LVGL dashboard + icons + view switching
│       │   ├── ui_app.cpp           # DeepSeek / OpenCode dual-view rendering
│       │   ├── ui_app.h             # set_tracking_mode(0|1)
│       │   ├── icons.h/.c           # A8 icons (deepseek/opencode/weather)
│       │   ├── icon_wifi*.c         # RGB565 icons (wifi/wifi_low/wifi_off)
│       │   ├── icon_bat*.c          # RGB565 battery icons (full/med/low/chg)
│       │   ├── font_amt14.c         # Arial-Bold 14px (incl. ¥)
│       │   └── font_bal28.c         # DejaVuSans-Bold 28px (¥ only)
│       ├── user_app/                # Key detection, battery ADC, clock, poll scheduler
│       │   ├── user_app.cpp         # GPIO18 key_task + usage_poll_task
│       │   └── user_app.h
│       ├── app_bsp/                 # LVGL platform port (lvgl_bsp.c/.h)
│       └── port_bsp/                # Display driver (display_bsp.cpp/.h)
├── scripts/
│   ├── start_bridge.bat             # Windows background launcher (brief cmd window)
│   ├── start_bridge.vbs             # Fully silent launcher (recommended for auto-start)
│   ├── install-bridge-linux.sh      # Linux systemd installer
│   ├── rlcd-claude-limits.py        # Root timer: fetches 5h/7d limits
│   ├── rlcd-claude-limits.{service,timer}
│   ├── gen_icons.py                 # Icon generator
│   ├── copy_xiaozhi_icons.py        # Copy icons from xiaozhi-esp32
│   └── append_icons.py              # Icon append tool
├── docs/
│   ├── mockup.png                   # UI reference mockup
│   ├── mockup.py                    # Mockup generation script
│   └── ui-mockup.txt                # ASCII layout reference
└── device_photo.png                 # Actual device photo
```

## Known issues

- **DeepSeek data empty**: If `deepseek` is null, check your `DEEPSEEK_API_KEY` and `sources/deepseek.py` log output
- **OpenCode scrape failure**: Cookie expired — copy a fresh one from the browser; check `sources/opencode.py` logs
- **WiFi connection blocking**: DHCP timeout no longer freezes the device (20s timeout guard), but power-saving config is skipped until next reboot
- **Chinese font rendering**: SimHei/DengXian tested — all show squares. Weather text is English-only for now
- **View switching**: DeepSeek ↔ OpenCode toggle is instant (cached data), but fresh data only arrives on the next HTTP poll

## License

MIT
