"""OpenCode Go quota from the web dashboard (cookie-based scraping).

OpenCode Go has no public REST API for usage/balance. The dashboard at
/workspace/<id>/go embeds rolling/weekly/monthly quota windows in SSR
hydration data. We parse those with a regex and cache for TTL seconds.
"""
from __future__ import annotations

import os
import re
import time
import urllib.request

from schema import OpenCodeGo, OpenCodeGoWindow

WORKSPACE_ID = os.environ.get("OPENCODE_GO_WORKSPACE_ID") or ""
AUTH_COOKIE = os.environ.get("OPENCODE_GO_AUTH_COOKIE") or ""
TTL = int(os.environ.get("RLCD_OPENCODE_TTL", "300"))  # 5 min

_cache: dict[str, object] = {"data": None, "ts": 0.0}

_RE = re.compile(r"(rolling|weekly|monthly)Usage:\$R\[\d+\]=\{"
                 r"status:\"(?P<status>[^\"]*)\","
                 r"resetInSec:(?P<reset_sec>\d+),"
                 r"usagePercent:(?P<pct>\d+)\}")


def _parse_window(html: str, key: str) -> OpenCodeGoWindow:
    """Search for e.g. rollingUsage:$R[30]={status:"ok",resetInSec:13054,usagePercent:11}"""
    for m in _RE.finditer(html):
        if m.group(1) == key:
            return OpenCodeGoWindow(
                usage_pct=int(m.group("pct")),
                reset_min=int(m.group("reset_sec")) // 60,
                status=m.group("status"),
            )
    return OpenCodeGoWindow()


def fetch_opencode_go() -> OpenCodeGo | None:
    if not WORKSPACE_ID or not AUTH_COOKIE:
        return None

    now = time.time()
    cached = _cache["data"]
    if cached is not None and now - float(_cache["ts"]) < TTL:
        return cached  # type: ignore

    try:
        url = f"https://opencode.ai/workspace/{WORKSPACE_ID}/go"
        req = urllib.request.Request(
            url,
            headers={
                "Cookie": f"auth={AUTH_COOKIE}",
                "Accept": "text/html",
                "User-Agent": "RLCD-bridge/1.0",
            },
        )
        with urllib.request.urlopen(req, timeout=15) as r:
            html = r.read().decode("utf-8", errors="replace")
    except Exception:
        return cached  # type: ignore

    data = OpenCodeGo(
        rolling=_parse_window(html, "rolling"),
        weekly=_parse_window(html, "weekly"),
        monthly=_parse_window(html, "monthly"),
    )
    _cache["data"] = data
    _cache["ts"] = now
    return data
