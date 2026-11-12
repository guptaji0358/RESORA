"""Security helpers: project-root path confinement and secret detection."""
from __future__ import annotations

import re
from pathlib import Path

LIAS_DIR = Path(__file__).resolve().parents[1]
PROJECT_ROOT = LIAS_DIR.parents[1]

_SECRET_PATTERNS = [
    re.compile(r"(?i)\b(api[_-]?key|secret|token|passwd|password)\b\s*[:=]\s*\S{6,}"),
    re.compile(r"\b(sk|pk|ghp|gho|xox[abp])[-_][A-Za-z0-9_-]{16,}"),
    re.compile(r"AKIA[0-9A-Z]{16}"),
    re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----"),
    re.compile(r"\b\d{3}-\d{2}-\d{4}\b"),
    re.compile(r"\b(?:\d[ -]?){13,19}\b"),
]


def resolve_inside(path: str, root: Path = PROJECT_ROOT) -> Path:
    """Resolve `path` (relative to root) and refuse anything that escapes root."""
    p = Path(path)
    full = (p if p.is_absolute() else root / p).resolve()
    try:
        full.relative_to(root.resolve())
    except ValueError:
        raise ValueError(f"path escapes project root: {path}") from None
    return full


def contains_secret(text: str) -> bool:
    return any(p.search(text) for p in _SECRET_PATTERNS)
