"""Optional on-disk cache of file fingerprints (SQLite, stdlib). An entry is reused
only when path, size and modification time (ns) all still match, so an edited
file is always re-hashed. The cache never touches the images. A cache problem
deactivates the cache and is exposed as `warning`; it never fails a scan."""
from __future__ import annotations

import os
import sqlite3
from pathlib import Path

SCHEMA = ("CREATE TABLE IF NOT EXISTS fp (path TEXT PRIMARY KEY, size INTEGER, mtime INTEGER,"
          " sha TEXT, dhash TEXT, dstatus TEXT)")
DSTATUS_OK, DSTATUS_FLAT = "ok", "flat"  # fingerprint stored / image has no usable detail


class FingerprintCache:
    def __init__(self, path: str | None) -> None:
        self.warning: str | None = None
        self._db: sqlite3.Connection | None = None
        self._pending = 0
        if path:
            try:
                Path(path).parent.mkdir(parents=True, exist_ok=True)
                self._db = sqlite3.connect(path)
                self._db.execute(SCHEMA)
            except (sqlite3.Error, OSError) as exc:
                self._db = None
                self.warning = f"cache disabled: {exc}"

    @staticmethod
    def key(path: Path) -> str:
        return os.path.normcase(str(path))

    def get(self, path: Path, size: int, mtime: int) -> dict | None:
        """{"sha", "dhash" (int|None), "dstatus"} or None when absent/stale."""
        if self._db is None:
            return None
        try:
            row = self._db.execute("SELECT sha, dhash, dstatus FROM fp WHERE path=? AND size=? AND mtime=?",
                                   (self.key(path), size, mtime)).fetchone()
        except sqlite3.Error as exc:
            self._fail(exc)
            return None
        if row is None:
            return None
        return {"sha": row[0], "dhash": int(row[1], 16) if row[1] else None, "dstatus": row[2]}

    def put(self, path: Path, size: int, mtime: int, sha: str | None = None,
            dhash: int | None = None, dstatus: str | None = None) -> None:
        """Merges with the current entry when (size, mtime) still match; otherwise replaces it."""
        if self._db is None:
            return
        old = self.get(path, size, mtime) or {}
        sha = sha or old.get("sha")
        if dstatus is None:
            dhash, dstatus = old.get("dhash"), old.get("dstatus")
        dtext = None if dhash is None else format(dhash, "032x")
        try:
            self._db.execute("INSERT OR REPLACE INTO fp VALUES (?,?,?,?,?,?)",
                             (self.key(path), size, mtime, sha, dtext, dstatus))
            self._pending += 1
            if self._pending >= 200:
                self.commit()
        except sqlite3.Error as exc:
            self._fail(exc)

    def commit(self) -> None:
        if self._db is not None:
            try:
                self._db.commit()
                self._pending = 0
            except sqlite3.Error as exc:
                self._fail(exc)

    def close(self) -> None:
        self.commit()
        if self._db is not None:
            self._db.close()
            self._db = None

    def _fail(self, exc: Exception) -> None:
        self.warning = f"cache disabled: {exc}"
        self._db = None
