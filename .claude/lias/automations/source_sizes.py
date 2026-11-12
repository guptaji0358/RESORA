"""Read-only source size report. stdin: {root, exts, top}; stdout: one JSON object."""
from __future__ import annotations

import json
import os
import sys
from pathlib import Path

SKIP = {".git", ".venv", "build", "logs", "__pycache__", "node_modules", "cache", "backups"}


def main() -> None:
    a = json.load(sys.stdin)
    root = Path(a["root"])
    exts = {"." + e.strip().lstrip(".").lower() for e in a["exts"].split(",") if e.strip()}
    files: list[tuple[int, int, str]] = []
    by_ext: dict[str, dict[str, int]] = {}
    for dp, dirs, names in os.walk(root):
        dirs[:] = [d for d in dirs if d not in SKIP]
        for n in names:
            p = Path(dp, n)
            ext = p.suffix.lower()
            if ext not in exts:
                continue
            try:
                data = p.read_bytes()
            except OSError:
                continue
            lines = data.count(b"\n") + (1 if data and not data.endswith(b"\n") else 0)
            files.append((len(data), lines, p.relative_to(root).as_posix()))
            e = by_ext.setdefault(ext, {"files": 0, "bytes": 0, "lines": 0})
            e["files"] += 1
            e["bytes"] += len(data)
            e["lines"] += lines
    files.sort(reverse=True)
    print(json.dumps({
        "files": len(files),
        "bytes": sum(f[0] for f in files),
        "lines": sum(f[1] for f in files),
        "by_ext": dict(sorted(by_ext.items(), key=lambda kv: -kv[1]["bytes"])),
        "largest": [{"path": f[2], "bytes": f[0], "lines": f[1]} for f in files[: a["top"]]],
    }))


main()
