"""SQLite knowledge store: add (dedupe/contradiction), search, forget, recovery."""
from __future__ import annotations

import hashlib
import re
import sqlite3
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

from core.guard import LIAS_DIR, contains_secret

DB_PATH = LIAS_DIR / "database" / "knowledge.db"
SCHEMA = LIAS_DIR / "database" / "schema.sql"
KINDS = ("fact", "preference", "decision", "temporary", "unverified", "historical")
_STOP = frozenset(
    "the a an is are was were be my your our this that these those what where when who how "
    "which does did do can could should would will for from with about and or of to in on at "
    "me you it its i we tell please".split()
)


def _now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _norm(s: str) -> str:
    return re.sub(r"\s+", " ", s.strip().lower())


def _hash(category: str, key: str, value: str) -> str:
    return hashlib.sha256("\x1f".join(map(_norm, (category, key, value))).encode()).hexdigest()


def _healthy(path: Path) -> bool:
    try:
        con = sqlite3.connect(path)
        try:
            return con.execute("PRAGMA integrity_check").fetchone()[0] == "ok"
        finally:
            con.close()
    except sqlite3.DatabaseError:
        return False


def _recover(path: Path) -> None:
    """Quarantine a corrupt DB and restore the newest backup, if any."""
    path.rename(path.with_name(f"{path.stem}.corrupt-{int(time.time())}{path.suffix}"))
    backups = sorted((path.parent / "backups").glob("knowledge-*.db"))
    for b in reversed(backups):
        if _healthy(b):
            path.write_bytes(b.read_bytes())
            return


def connect(path: Path = DB_PATH) -> sqlite3.Connection:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and not _healthy(path):
        _recover(path)
    con = sqlite3.connect(path)
    con.row_factory = sqlite3.Row
    con.executescript(SCHEMA.read_text(encoding="utf-8"))
    return con


def _backup(con: sqlite3.Connection, path: Path) -> None:
    """Snapshot after each committed write (DB was verified healthy on open).

    One file per day, overwritten; the newest 7 days are kept.
    """
    bdir = path.parent / "backups"
    bdir.mkdir(exist_ok=True)
    target = bdir / f"knowledge-{datetime.now():%Y%m%d}.db"
    target.unlink(missing_ok=True)
    dst = sqlite3.connect(target)
    try:
        con.backup(dst)
    finally:
        dst.close()
    for old in sorted(bdir.glob("knowledge-*.db"))[:-7]:
        old.unlink()


def add(con: sqlite3.Connection, *, kind: str, category: str, key: str, value: str,
        source: str, status: str = "confirmed", confidence: float = 1.0,
        ttl_days: float | None = None, db_path: Path = DB_PATH) -> dict[str, Any]:
    if kind not in KINDS:
        raise ValueError(f"kind must be one of {KINDS}")
    if status not in ("confirmed", "unverified"):
        raise ValueError("status must be confirmed or unverified")
    if not (key.strip() and value.strip() and category.strip() and source.strip()):
        raise ValueError("category, key, value and source are required")
    if any(contains_secret(x) for x in (key, value)):
        raise ValueError("refused: value looks like a secret or sensitive identifier")
    h = _hash(category, key, value)
    dup = con.execute(
        "SELECT id FROM knowledge WHERE hash=? AND status!='superseded'", (h,)).fetchone()
    if dup:
        con.execute("UPDATE knowledge SET updated_at=? WHERE id=?", (_now(), dup["id"]))
        con.commit()
        return {"result": "duplicate", "id": dup["id"]}
    old = con.execute(
        "SELECT id, value, status FROM knowledge WHERE category=? AND key=? COLLATE NOCASE "
        "AND status!='superseded'", (category, key)).fetchall()
    if old and status == "unverified" and any(o["status"] == "confirmed" for o in old):
        return {"result": "conflict_rejected", "existing": [dict(o) for o in old],
                "hint": "unverified info cannot override confirmed knowledge"}
    expires = None
    if ttl_days is not None:
        expires = (datetime.now(timezone.utc) + timedelta(days=ttl_days)).strftime("%Y-%m-%dT%H:%M:%SZ")
    now = _now()
    cur = con.execute(
        "INSERT INTO knowledge(kind,category,key,value,source,confidence,status,created_at,"
        "updated_at,expires_at,hash) VALUES(?,?,?,?,?,?,?,?,?,?,?)",
        (kind, category, key, value, source, confidence, status, now, now, expires, h))
    new_id = cur.lastrowid
    superseded = [o["id"] for o in old]
    if status == "confirmed":
        con.executemany("UPDATE knowledge SET status='superseded', kind='historical', "
                        "superseded_by=?, updated_at=? WHERE id=?",
                        [(new_id, now, i) for i in superseded])
    else:
        superseded = []
    con.commit()
    _backup(con, db_path)
    return {"result": "added", "id": new_id, "superseded": superseded}


def tokens(text: str) -> list[str]:
    seen: list[str] = []
    for w in re.findall(r"[A-Za-z0-9]+", text.lower()):
        if len(w) >= 3 and w not in _STOP and w not in seen:
            seen.append(w)
    return seen


def search(con: sqlite3.Connection, query: str, limit: int = 5,
           min_coverage: float = 0.5) -> list[dict[str, Any]]:
    """BM25 over FTS5; keep rows matching >= min_coverage of the query's content words."""
    toks = tokens(query)
    if not toks:
        return []
    match = " OR ".join(f'"{t}"' for t in toks)
    rows = con.execute(
        "SELECT k.*, bm25(knowledge_fts) AS score FROM knowledge_fts "
        "JOIN knowledge k ON k.id = knowledge_fts.rowid "
        "WHERE knowledge_fts MATCH ? AND k.status!='superseded' "
        "AND (k.expires_at IS NULL OR k.expires_at > ?) ORDER BY score LIMIT ?",
        (match, _now(), limit * 4)).fetchall()
    out = []
    for r in rows:
        blob = set(tokens(f"{r['category']} {r['key']} {r['value']}"))
        stems = " ".join(blob)
        hit = sum(1 for t in toks if t in blob or t[:5] in stems)
        if hit / len(toks) >= min_coverage:
            d = dict(r)
            d.pop("hash", None)
            d["coverage"] = round(hit / len(toks), 2)
            out.append(d)
    return out[:limit]


def forget(con: sqlite3.Connection, entry_id: int, db_path: Path = DB_PATH) -> dict[str, Any]:
    """Soft-delete: mark superseded/historical. Rows are never physically removed."""
    cur = con.execute("UPDATE knowledge SET status='superseded', kind='historical', updated_at=? "
                      "WHERE id=? AND status!='superseded'", (_now(), entry_id))
    con.commit()
    _backup(con, db_path)
    return {"result": "forgotten" if cur.rowcount else "not_found", "id": entry_id}


def stats(con: sqlite3.Connection) -> dict[str, Any]:
    rows = con.execute("SELECT kind, status, COUNT(*) n FROM knowledge GROUP BY kind, status").fetchall()
    return {"total": sum(r["n"] for r in rows), "by_kind_status": {f"{r['kind']}/{r['status']}": r["n"] for r in rows}}
