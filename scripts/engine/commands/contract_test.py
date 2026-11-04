"""Hidden diagnostic command exercising every protocol path (progress, large
output, failure, slow/cancellable work). Contains no image logic."""
from __future__ import annotations

import argparse
import json
import time
from pathlib import Path
from typing import Any

from ..protocol import EXIT_INVALID_INPUT, BackendError
from ..command import Command, Context

MODES = ("ok", "progress", "large", "fail", "invalid", "crash")


def add_arguments(p: argparse.ArgumentParser) -> None:
    p.add_argument("mode", choices=MODES)
    p.add_argument("--steps", type=int, default=3, help="progress: number of steps")
    p.add_argument("--delay", type=float, default=0.0, help="progress: seconds per step")
    p.add_argument("--output", help="large: write the payload to this file")
    p.add_argument("--items", type=int, default=100000, help="large: number of items")


def run(args: argparse.Namespace, ctx: Context) -> Any:
    if args.mode == "ok":
        return {"params": ctx.params}
    if args.mode == "progress":
        for i in range(args.steps):
            ctx.cancel.check()
            time.sleep(args.delay)
            ctx.emit.progress(i + 1, args.steps, f"step {i + 1}")
        return {"steps": args.steps}
    if args.mode == "large":
        # Large results go to a file; stdout only carries the small reference.
        if not args.output:
            raise BackendError("invalid_input", "large requires --output", EXIT_INVALID_INPUT)
        Path(args.output).write_text(json.dumps([{"i": i} for i in range(args.items)]), encoding="utf-8")
        return {"output": str(Path(args.output).resolve()), "items": args.items}
    if args.mode == "fail":
        raise BackendError("test_failure", "deliberate failure")
    if args.mode == "invalid":
        raise BackendError("invalid_input", "deliberate invalid input", EXIT_INVALID_INPUT)
    raise RuntimeError("deliberate crash")


COMMAND = Command(
    name="_contract-test",
    help="protocol diagnostics",
    run=run,
    add_arguments=add_arguments,
    hidden=True,
)
