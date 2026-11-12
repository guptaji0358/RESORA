"""Automation runner: manifest discovery, arg validation, cache, timeout, verification, log.

An automation is `automations/<name>.json` (manifest) + `automations/<name>.py`.
The script reads one JSON object on stdin and prints one JSON object on stdout.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

from core.guard import LIAS_DIR, PROJECT_ROOT, resolve_inside

AUTOMATIONS = LIAS_DIR / "automations"
CACHE = LIAS_DIR / "cache"
LOGS = LIAS_DIR / "logs"
MAX_OUT = 20000
_TYPES = {"str": str, "int": int, "bool": bool, "path": str}


def log_event(event: dict[str, Any], logs: Path = LOGS) -> None:
    """Append one JSONL event; never raises (logging must not break a run)."""
    try:
        logs.mkdir(parents=True, exist_ok=True)
        f = logs / "events.jsonl"
        if f.exists() and f.stat().st_size > 1_000_000:
            f.replace(logs / "events.1.jsonl")
        with f.open("a", encoding="utf-8") as fh:
            fh.write(json.dumps({"ts": round(time.time(), 3), **event}) + "\n")
    except OSError:
        pass


def list_automations(root: Path = AUTOMATIONS) -> list[dict[str, Any]]:
    out = []
    for m in sorted(root.glob("*.json")):
        d = json.loads(m.read_text(encoding="utf-8"))
        out.append({k: d.get(k) for k in ("name", "description", "args", "destructive")})
    return out


def validate_args(spec: dict[str, Any], given: dict[str, Any]) -> dict[str, Any]:
    unknown = set(given) - set(spec)
    if unknown:
        raise ValueError(f"unknown args: {sorted(unknown)}")
    out: dict[str, Any] = {}
    for name, rule in spec.items():
        if name in given:
            val = given[name]
        elif "default" in rule:
            val = rule["default"]
        elif rule.get("required"):
            raise ValueError(f"missing required arg: {name}")
        else:
            continue
        typ = rule.get("type", "str")
        if typ not in _TYPES:
            raise ValueError(f"bad manifest type for {name}: {typ}")
        if not isinstance(val, _TYPES[typ]) or (typ == "int" and isinstance(val, bool)):
            raise ValueError(f"arg {name} must be {typ}")
        if typ == "path":
            val = str(resolve_inside(val))
        if "max" in rule and typ == "int" and val > rule["max"]:
            raise ValueError(f"arg {name} exceeds max {rule['max']}")
        out[name] = val
    return out


def _cache_key(name: str, args: dict[str, Any], script: Path, manifest: Path) -> str:
    stamp = f"{script.stat().st_mtime_ns}:{manifest.stat().st_mtime_ns}"
    return hashlib.sha256(json.dumps([name, args, stamp], sort_keys=True).encode()).hexdigest()[:24]


def run(name: str, args: dict[str, Any] | None = None, *, use_cache: bool = True,
        confirm: bool = False, root: Path = AUTOMATIONS, cache: Path = CACHE,
        logs: Path = LOGS) -> dict[str, Any]:
    """Run an automation; always returns an envelope-shaped dict, never raises."""
    t0 = time.perf_counter()

    def done(status: str, **kw: Any) -> dict[str, Any]:
        res = {"status": status, "automation": name,
               "duration_ms": round((time.perf_counter() - t0) * 1000), **kw}
        log_event({"type": "run", "automation": name, "status": status,
                   "duration_ms": res["duration_ms"], "cached": kw.get("cached", False)}, logs)
        return res

    if not name.replace("_", "").isalnum():
        return done("error", error="invalid automation name")
    manifest, script = root / f"{name}.json", root / f"{name}.py"
    if not manifest.is_file() or not script.is_file():
        return done("error", error=f"unknown automation: {name}")
    try:
        spec = json.loads(manifest.read_text(encoding="utf-8"))
        clean = validate_args(spec.get("args", {}), args or {})
    except (ValueError, json.JSONDecodeError) as e:
        return done("invalid_input", error=str(e))
    if spec.get("destructive") and not confirm:
        return done("needs_confirmation", error="destructive automation requires --confirm")
    missing = [d for d in spec.get("deps", []) if importlib.util.find_spec(d) is None]
    if missing:
        return done("missing_dependency", error=f"missing modules: {missing}")

    ttl = spec.get("cache_ttl_s", 0)
    cfile = cache / f"{_cache_key(name, clean, script, manifest)}.json"
    if use_cache and ttl and cfile.is_file() and time.time() - cfile.stat().st_mtime < ttl:
        try:
            return done("ok", data=json.loads(cfile.read_text(encoding="utf-8")), cached=True)
        except (OSError, json.JSONDecodeError):
            pass

    try:
        p = subprocess.run([sys.executable, "-I", "-X", "utf8", str(script)],
                           input=json.dumps(clean), capture_output=True, text=True,
                           encoding="utf-8", timeout=spec.get("timeout_s", 30), cwd=PROJECT_ROOT)
    except subprocess.TimeoutExpired:
        return done("timeout", error=f"exceeded {spec.get('timeout_s', 30)}s")
    if p.returncode != 0:
        return done("failed", error=(p.stderr.strip() or "nonzero exit")[:500], exit_code=p.returncode)
    try:
        data = json.loads(p.stdout)
        if not isinstance(data, dict):
            raise ValueError("output is not a JSON object")
    except ValueError as e:
        return done("failed", error=f"verification failed: {e}")
    text = json.dumps(data, ensure_ascii=False)
    if len(text) > MAX_OUT:
        return done("ok", data={"truncated": True, "preview": text[:MAX_OUT]}, truncated=True)
    if ttl:
        try:
            cache.mkdir(parents=True, exist_ok=True)
            cfile.write_text(text, encoding="utf-8")
        except OSError:
            pass
    return done("ok", data=data, cached=False)
