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

1. **Bridge** (wherever Claude Code actually runs — laptop, desktop, or VPS):
   ```bash
   cd bridge
   uv run python bridge.py
   curl http://localhost:7777/api/usage | jq
   ```
2. **Install as a service**:
   ```bash
   scripts/install-bridge-linux.sh
   ```
3. **Firmware** — flash following `firmware/README.md` (requires hardware + ESP-IDF v5.x).

## Deployment models

| Mode | Bind `RLCD_HOST` to | Notes |
| --- | --- | --- |
| Bridge on same LAN as the ESP32 | LAN IP (or `0.0.0.0`) | Simplest. Recommended `RLCD_AUTH_TOKEN` even on LAN. |
| Bridge on a VPS / remote host (Claude Code runs there) | overlay-network IP (Tailscale / ZeroTier) | ESP32 needs the same overlay reachable from home. Typically: home router or an always-on box joins the overlay and advertises the route. **Do not expose the public IP without a token.** |
| Bridge on dev box for firmware bring-up | `127.0.0.1` | UI work with `?mock=1`, no real data leaves the host. |

Always set `RLCD_AUTH_TOKEN` (random ≥24 bytes) when the bridge listens on
anything other than loopback. The ESP32 sends it as `X-RLCD-Token`.

### ZeroTier MTU gotcha

If the bridge is on a VPS reached over ZeroTier and the device sits behind a
home router, you may see the TCP handshake succeed but the response never
arrive (`Established ... 0 bytes received`, or the ESP32 logs
`ESP_ERR_HTTP_CONNECT`). ZeroTier's default 2800-byte MTU is larger than the
real path to the home LAN (~1400), so large responses get black-holed by
broken PMTUD. Fix on the VPS with `scripts/vps-zt-mtu-fix.sh` (lowers the zt
MTU to 1400 and clamps outgoing TCP MSS to 1360); install it as the
`rlcd-zt-fix.service` boot unit so it survives reboots. The cleanest
alternative is to set the network MTU to 1400 in ZeroTier Central, which
propagates to every member.

## Hardware

- [Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/wiki/ESP32-S3-RLCD-4.2)
- 4.2" reflective LCD (paper-like), ESP32-S3, WiFi, RTC, temp/humidity, SD, audio.
- Vendor demos: <https://github.com/waveshareteam/ESP32-S3-RLCD-4.2>
