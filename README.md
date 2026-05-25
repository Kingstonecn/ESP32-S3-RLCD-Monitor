# RLCD — Claude Usage Dashboard

A desktop ornament that shows your live Claude (Pro/Max + API) usage on a
Waveshare ESP32-S3-RLCD-4.2 reflective-LCD board.

```
claude-code @ rlcd ~ $ status        14:30
──────────────────────────────────────────
5h window  [██████░░░░] 62%
  162,438 tokens · $4.21
  reset in 2h 14m

weekly     [████░░░░░░] 41%

today     382k tok    $9.14
month    8.4M tok   $187.22
total   18.2M tok   $214.07

──────────────────────────────────────────
models: opus 71% · sonnet 24% · haiku 5%
```

## Architecture

```
Linux PC                                   ESP32-S3-RLCD-4.2
─────────                                  ─────────────────
~/.claude/**/*.jsonl                       LVGL terminal UI
        │                                          ▲
        ▼                       LAN HTTP           │
   bridge daemon  ──── GET /api/usage (60s) ──────┘
   (spawns ccusage)
   :7777
```

- **Bridge** (`bridge/`) — Python FastAPI daemon. Spawns `ccusage blocks/daily/monthly --json`,
  flattens into one schema, serves at `http://<pc-ip>:7777/api/usage`. Runs under systemd
  `--user`.
- **Firmware** (`firmware/`) — ESP-IDF + LVGL v9 project. Polls bridge every 60 s, renders
  a monospace dashboard on the RLCD.

## Quick start

1. **Bridge** (on the PC that runs Claude Code):
   ```bash
   cd bridge
   uv run bridge.py
   curl http://localhost:7777/api/usage | jq
   ```
2. **Install as a service**:
   ```bash
   scripts/install-bridge-linux.sh
   ```
3. **Firmware** — flash following `firmware/README.md` (requires hardware + ESP-IDF v5.x).

## Hardware

- [Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/wiki/ESP32-S3-RLCD-4.2)
- 4.2" reflective LCD (paper-like), ESP32-S3, WiFi, RTC, temp/humidity, SD, audio.
- Vendor demos: <https://github.com/waveshareteam/ESP32-S3-RLCD-4.2>
