"""RLCD bridge daemon.

GET /api/usage           -> live usage (cached 60s)
GET /api/usage?mock=1    -> deterministic mock payload, for firmware bring-up
GET /healthz             -> liveness

Run: `uv run bridge.py` or `uvicorn bridge:app --host 0.0.0.0 --port 7777`
"""
from __future__ import annotations

import os
import threading
import time
import traceback
from datetime import datetime, timezone, timedelta
from pathlib import Path

from dotenv import load_dotenv
# Auto-load .env from the same directory as bridge.py.
# override=True ensures .env always wins over system-level env vars.
load_dotenv(dotenv_path=Path(__file__).parent / ".env", override=True)

from fastapi import FastAPI, Header, HTTPException, Query
from fastapi.responses import JSONResponse

from schema import (
    ActiveBlock,
    Bucket,
    ClaudeLimits,
    ClaudeUsage,
    DeepSeek,
    ModelBreakdown,
    OpenCodeGo,
    OtherAgentUsage,
    UsageReport,
    Weather,
)
from sources.weather import fetch_weather
from sources.deepseek import fetch_deepseek
from sources.opencode import fetch_opencode_go


def _local_now():
    return datetime.now(timezone.utc) + timedelta(hours=8)

REFRESH_INTERVAL_SEC = int(os.environ.get("RLCD_REFRESH_SEC", "45"))
INCLUDE_OTHERS = os.environ.get("RLCD_INCLUDE_OTHERS", "1") != "0"
AUTH_TOKEN = os.environ.get("RLCD_AUTH_TOKEN") or None  # blank/unset = no auth

app = FastAPI(title="RLCD bridge", version="0.1.0")

_cache_lock = threading.Lock()
_cache: dict[str, object] = {"report": None, "ts": 0.0, "error": None}


USD_CNY = float(os.environ.get("RLCD_USD_CNY", "7.25"))  # USD→CNY rate


def _fetch_ds_usage() -> dict:
    """Run ccusage daily, return dict with today/month DeepSeek usage stats.
    Uses UTC (timezone.utc) to compute today/month boundaries so the
    dashboard aligns with DeepSeek's own billing page (which also uses UTC).
    """
    import json, os, subprocess
    from datetime import datetime, timezone
    nodejs = r"C:\Program Files\nodejs"
    npx_js = os.path.join(nodejs, "node_modules", "npm", "bin", "npx-cli.js")
    env = os.environ.copy()
    if nodejs not in env.get("PATH", ""):
        env["PATH"] = nodejs + os.pathsep + env.get("PATH", "")
    ccusage = os.environ.get("CCUSAGE_CMD")
    default = {
        "today_tokens": 0, "today_cost_cny": 0.0, "today_cache_pct": 0.0,
        "month_tokens": 0, "month_cost_cny": 0.0, "month_cache_pct": 0.0,
    }
    try:
        if ccusage:
            proc = subprocess.run(
                ccusage + " claude daily --json",
                capture_output=True, text=True, timeout=30, shell=True, env=env,
            )
        else:
            proc = subprocess.run(
                [os.path.join(nodejs, "node.exe"), npx_js,
                 "-y", "ccusage@latest", "claude", "daily", "--json"],
                capture_output=True, text=True, timeout=30, env=env,
                creationflags=subprocess.CREATE_NO_WINDOW,
            )
        if proc.returncode != 0:
            return default
        data = json.loads(proc.stdout)
        # UTC dates for today/month boundaries
        utc_now = datetime.now(timezone.utc)
        today_str = utc_now.strftime("%Y-%m-%d")
        month_str = utc_now.strftime("%Y-%m")

        def _sum_model(entries, date_filter):
            inp = out = cr = cc = cost = 0
            for e in entries:
                if e.get("date", "") != date_filter:
                    continue
                for mb in e.get("modelBreakdowns", []):
                    if "deepseek" not in (mb.get("modelName") or "").lower():
                        continue
                    inp += int(mb.get("inputTokens", 0) or 0)
                    out += int(mb.get("outputTokens", 0) or 0)
                    cr += int(mb.get("cacheReadTokens", 0) or 0)
                    cc += int(mb.get("cacheCreationTokens", 0) or 0)
            total = inp + out + cr + cc
            # Recompute CNY cost using DeepSeek's own pricing table
            #   flash: cache_hit=0.02, miss=1.00, out=2.00  (per 1M tok)
            #   pro:   cache_hit=0.025, miss=3.00, out=6.00
            flash_miss = flash_hit = flash_out = 0
            pro_miss = pro_hit = pro_out = 0
            for e in entries:
                if e.get("date", "") != date_filter:
                    continue
                for mb in e.get("modelBreakdowns", []):
                    if "deepseek" not in (mb.get("modelName") or "").lower():
                        continue
                    is_pro = "pro" in (mb.get("modelName") or "").lower()
                    i = int(mb.get("inputTokens", 0) or 0)
                    c = int(mb.get("cacheReadTokens", 0) or 0)
                    o = int(mb.get("outputTokens", 0) or 0)
                    if is_pro:
                        pro_miss += i; pro_hit += c; pro_out += o
                    else:
                        flash_miss += i; flash_hit += c; flash_out += o
            cost_cny = (
                flash_miss / 1e6 * 1.00 + flash_hit / 1e6 * 0.02 + flash_out / 1e6 * 2.00 +
                pro_miss   / 1e6 * 3.00 + pro_hit   / 1e6 * 0.025 + pro_out   / 1e6 * 6.00
            )
            cache_pct = (cr / (inp + cr + cc) * 100) if (inp + cr + cc) > 0 else 0.0
            return total, round(cost_cny * 1.02, 2), cache_pct

        today_tok, today_cny, today_cache = _sum_model(data.get("daily", []), today_str)

        # ccusage tags entries by local time (UTC+8). If UTC today != local
        # today (which happens during 08:00-16:00 UTC, i.e. 16:00-00:00 CST),
        # also include the entry tagged with the local date so data split
        # across the UTC boundary is not lost.
        from datetime import timedelta
        local_today_str = (utc_now + timedelta(hours=8)).strftime("%Y-%m-%d")
        if local_today_str != today_str:
            lt_tok, lt_cny, lt_cache = _sum_model(data.get("daily", []), local_today_str)
            if lt_tok > 0:
                today_tok += lt_tok
                today_cny += lt_cny
                # recalc cache rate for combined set (weighted average)
                if today_tok > 0 and lt_tok > 0:
                    today_cache = (today_cache * (today_tok - lt_tok) + lt_cache * lt_tok) / today_tok

        # No yesterday fallback — zero today means no usage, show it as-is.
        _yesterday_label = "TODAY"

        # Month: sum all entries where date starts with month_str
        month_tok = month_cny = month_cache_numer = month_cache_denom = 0
        for e in data.get("daily", []):
            if not e.get("date", "").startswith(month_str):
                continue
            mt, mc, mcp = _sum_model([e], e["date"])
            month_tok += mt
            month_cny += mc
        # Rebuild month cache rate properly
        month_inp = month_out = month_cr = month_cc = 0
        for e in data.get("daily", []):
            if not e.get("date", "").startswith(month_str):
                continue
            for mb in e.get("modelBreakdowns", []):
                if "deepseek" not in (mb.get("modelName") or "").lower():
                    continue
                month_inp += int(mb.get("inputTokens", 0) or 0)
                month_cr += int(mb.get("cacheReadTokens", 0) or 0)
                month_cc += int(mb.get("cacheCreationTokens", 0) or 0)
        month_cache = (month_cr / (month_inp + month_cr + month_cc) * 100) if (month_inp + month_cr + month_cc) > 0 else 0.0

        return {
            "today_tokens": today_tok,
            "today_cost_cny": round(today_cny, 2),
            "today_cache_pct": round(today_cache, 1),
            "today_label": _yesterday_label,
            "month_tokens": month_tok,
            "month_cost_cny": round(month_cny, 2),
            "month_cache_pct": round(month_cache, 1),
        }
    except Exception:
        return default


def _build_live_report() -> UsageReport:
    """Build report. Extract DeepSeek usage from ccusage."""
    from schema import Bucket, ClaudeUsage
    empty = Bucket(tokens_used=0, cost_usd=0.0)
    ds_usage = _fetch_ds_usage()
    ds = fetch_deepseek(ds_usage["today_tokens"])
    if ds is not None:
        ds.today_cost_cny = ds_usage["today_cost_cny"]
        ds.today_cache_pct = ds_usage["today_cache_pct"]
        ds.today_label = ds_usage["today_label"]
        ds.month_tokens = ds_usage["month_tokens"]
        ds.month_cost_cny = ds_usage["month_cost_cny"]
        ds.month_cache_pct = ds_usage["month_cache_pct"]
    claude = ClaudeUsage(
        weekly=empty, today=empty, month=empty, lifetime=empty
    )
    return UsageReport(
        updated_at=_local_now(),
        claude=claude,
        other=[],
        weather=fetch_weather(),
        deepseek=ds,
        opencode_go=fetch_opencode_go(),
    )


def _get_cached() -> tuple[UsageReport | None, str | None]:
    # Non-blocking: a background thread keeps the cache warm, so clients
    # (the ESP32, with a short HTTP timeout) never wait on a cold ccusage run.
    with _cache_lock:
        return _cache.get("report"), _cache.get("error")


def _refresh_once() -> None:
    try:
        rep = _build_live_report()
        with _cache_lock:
            old = _cache.get("report")
            # Guard: if ccusage temporarily returns empty (npx cold start,
            # JSONL file locked during write, etc.), don't let zeros
            # overwrite previously valid data on the display.
            if (old is not None and isinstance(old, UsageReport)
                    and old.deepseek is not None and old.deepseek.today_tokens > 0
                    and rep.deepseek is not None and rep.deepseek.today_tokens == 0):
                rep.deepseek.today_tokens = old.deepseek.today_tokens
                rep.deepseek.today_cost_cny = old.deepseek.today_cost_cny
                rep.deepseek.today_cache_pct = old.deepseek.today_cache_pct
                rep.deepseek.today_label = old.deepseek.today_label
                if rep.deepseek.month_tokens == 0 and old.deepseek.month_tokens > 0:
                    rep.deepseek.month_tokens = old.deepseek.month_tokens
                    rep.deepseek.month_cost_cny = old.deepseek.month_cost_cny
                    rep.deepseek.month_cache_pct = old.deepseek.month_cache_pct
            # Also guard: if the whole deepseek object disappeared (API error
            # + ccusage failed), keep the previous deepseek data on screen.
            if (old is not None and isinstance(old, UsageReport)
                    and old.deepseek is not None
                    and rep.deepseek is None):
                rep.deepseek = old.deepseek
            _cache.update(report=rep, ts=time.time(), error=None)
    except Exception as e:
        with _cache_lock:
            _cache["error"] = f"{type(e).__name__}: {e}"


def _refresher_loop() -> None:
    while True:
        _refresh_once()
        time.sleep(REFRESH_INTERVAL_SEC)


_refresher_started = False


def _start_refresher() -> None:
    global _refresher_started
    if _refresher_started:
        return
    _refresher_started = True
    threading.Thread(target=_refresher_loop, name="usage-refresher", daemon=True).start()


def _mock_report() -> UsageReport:
    now = _local_now()
    return UsageReport(
        updated_at=now,
        source="mock",
        claude=ClaudeUsage(
            active_block=ActiveBlock(
                started_at=now.replace(hour=10, minute=0, second=0, microsecond=0),
                ends_at=now.replace(hour=15, minute=0, second=0, microsecond=0),
                tokens_used=162_438,
                cost_usd=4.21,
                percent_used=0.62,
                minutes_remaining=134,
                projection_tokens=260_000,
                projection_cost_usd=6.80,
            ),
            weekly=Bucket(tokens_used=2_410_000, cost_usd=58.13, percent_used=0.41),
            today=Bucket(tokens_used=382_000, cost_usd=9.14),
            month=Bucket(tokens_used=8_400_000, cost_usd=187.22),
            lifetime=Bucket(tokens_used=18_200_000, cost_usd=214.07),
            by_model=[
                ModelBreakdown(model="claude-opus-4-7", tokens=12_900_000, cost_usd=180.00),
                ModelBreakdown(model="claude-sonnet-4-6", tokens=4_400_000, cost_usd=28.00),
                ModelBreakdown(model="claude-haiku-4-5", tokens=900_000, cost_usd=6.07),
            ],
            limits=ClaudeLimits(
                util_5h=0.24, util_7d=0.56, status="ok",
                reset_5h=now.replace(hour=15, minute=0, second=0, microsecond=0),
                reset_7d=now.replace(hour=6, minute=0, second=0, microsecond=0),
                reset_5h_min=99, reset_7d_min=2640,
            ),
        ),
        other=[
            OtherAgentUsage(
                agent="codex",
                today=Bucket(tokens_used=124_000, cost_usd=0.31),
                month=Bucket(tokens_used=1_800_000, cost_usd=4.40),
                lifetime=Bucket(tokens_used=5_200_000, cost_usd=11.90),
            ),
        ],
        weather=Weather(temp_c=24.3, temp_min=22.0, temp_max=30.0, code=2, condition="Partly", icon="partly", city="Shanghai"),
        deepseek=DeepSeek(balance=70.79, currency="CNY", granted=0.0, topped=70.79,
                          today_tokens=2_400_000, today_cost_cny=5.42, today_cache_pct=96.8,
                          month_tokens=18_000_000, month_cost_cny=38.15, month_cache_pct=95.2,
                          today_label="TODAY",
                          available=True),
    )


@app.on_event("startup")
def _on_startup():
    _start_refresher()


@app.get("/healthz")
def healthz():
    return {"ok": True, "cache_age_sec": int(time.time() - float(_cache.get("ts", 0.0) or 0))}


def _check_auth(token_header: str | None, token_query: str | None) -> None:
    if AUTH_TOKEN is None:
        return
    presented = token_header or token_query
    if presented != AUTH_TOKEN:
        raise HTTPException(status_code=401, detail="invalid or missing token")


@app.get("/api/usage")
def get_usage(
    mock: int = Query(0),
    token: str | None = Query(None),
    x_rlcd_token: str | None = Header(None),
):
    _check_auth(x_rlcd_token, token)
    if mock:
        return _mock_report().model_dump(mode="json")
    rep, err = _get_cached()
    if rep is None:
        return JSONResponse(
            status_code=503,
            content={"error": err or "no data yet", "hint": "is ccusage installed and is ~/.claude populated?"},
        )
    payload = rep.model_dump(mode="json")
    if err:
        payload["stale"] = True
        payload["error"] = err
    return payload


def main():
    import uvicorn

    host = os.environ.get("RLCD_HOST", "0.0.0.0")
    port = int(os.environ.get("RLCD_PORT", "7777"))
    uvicorn.run(app, host=host, port=port, log_level="info")


if __name__ == "__main__":
    main()
