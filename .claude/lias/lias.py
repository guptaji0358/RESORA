"""LIAS CLI. stdout: one compact JSON envelope. Diagnostics: stderr / logs/events.jsonl.

  lias.py run <name> [--json '{...}'] [--no-cache] [--confirm]
  lias.py list
  lias.py know add|search|forget|stats ...
  lias.py doctor
"""
from __future__ import annotations

import argparse
import json
import sqlite3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from core import runner, store  # noqa: E402
from core.protocol import emit, envelope  # noqa: E402


def _parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="lias")
    sub = p.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("name")
    r.add_argument("--json", default="{}")
    r.add_argument("--no-cache", action="store_true")
    r.add_argument("--confirm", action="store_true")
    sub.add_parser("list")
    sub.add_parser("doctor")
    sub.add_parser("stats")
    k = sub.add_parser("know").add_subparsers(dest="kcmd", required=True)
    a = k.add_parser("add")
    a.add_argument("--kind", required=True, choices=store.KINDS)
    a.add_argument("--category", required=True)
    a.add_argument("--key", required=True)
    a.add_argument("--value", required=True)
    a.add_argument("--source", required=True, help="e.g. 'user, 2026-11-12'")
    a.add_argument("--status", default="confirmed", choices=("confirmed", "unverified"))
    a.add_argument("--confidence", type=float, default=1.0)
    a.add_argument("--ttl-days", type=float)
    s = k.add_parser("search")
    s.add_argument("query")
    s.add_argument("--limit", type=int, default=5)
    f = k.add_parser("forget")
    f.add_argument("id", type=int)
    k.add_parser("stats")
    return p


def _doctor() -> dict:
    checks = {"python": sys.version.split()[0]}
    c = sqlite3.connect(":memory:")
    try:
        c.execute("CREATE VIRTUAL TABLE t USING fts5(a)")
        checks["fts5"] = True
    except sqlite3.Error:
        checks["fts5"] = False
    checks["db_healthy"] = store.DB_PATH.exists() and store._healthy(store.DB_PATH) or "absent (created on first write)"
    checks["automations"] = [a["name"] for a in runner.list_automations()]
    return checks


def main(argv: list[str] | None = None) -> int:
    ns = _parser().parse_args(argv)
    try:
        if ns.cmd == "run":
            res = runner.run(ns.name, json.loads(ns.json), use_cache=not ns.no_cache, confirm=ns.confirm)
            emit(res)
            return 0 if res["status"] == "ok" else 1
        if ns.cmd == "list":
            emit(envelope("ok", runner.list_automations()))
        elif ns.cmd == "stats":
            emit(envelope("ok", runner.event_stats()))
        elif ns.cmd == "doctor":
            emit(envelope("ok", _doctor()))
        else:
            con = store.connect()
            try:
                if ns.kcmd == "add":
                    res = store.add(con, kind=ns.kind, category=ns.category, key=ns.key,
                                    value=ns.value, source=ns.source, status=ns.status,
                                    confidence=ns.confidence, ttl_days=ns.ttl_days)
                    emit(envelope("ok", res))
                elif ns.kcmd == "search":
                    emit(envelope("ok", store.search(con, ns.query, ns.limit)))
                elif ns.kcmd == "forget":
                    emit(envelope("ok", store.forget(con, ns.id)))
                else:
                    emit(envelope("ok", store.stats(con)))
            finally:
                con.close()
        return 0
    except (ValueError, json.JSONDecodeError) as e:
        emit(envelope("invalid_input", error=str(e)))
        return 2
    except sqlite3.Error as e:
        emit(envelope("error", error=f"database error: {e}"))
        return 3


if __name__ == "__main__":
    sys.exit(main())
