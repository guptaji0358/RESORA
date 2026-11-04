"""Verifies the engine environment (interpreter and dependencies)."""
from __future__ import annotations

import argparse
import sys
from typing import Any

from .. import __version__
from ..command import Command, Context


def run(args: argparse.Namespace, ctx: Context) -> dict[str, Any]:
    import PIL

    return {"ok": True, "backend": __version__, "python": sys.version.split()[0], "pillow": PIL.__version__}


COMMAND = Command(name="selfcheck", help="verify the backend environment", run=run)
