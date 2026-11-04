"""Engine orchestration: parse, run one command, emit exactly one result/error.

Protocol: docs/BACKEND_CONTRACT.md. No GUI imports allowed here.
"""
from __future__ import annotations

import sys
import traceback

from .cli import UsageError, build_parser
from .command import Context
from .params import load_params
from .protocol import (
    EXIT_CANCELLED,
    EXIT_FAILURE,
    EXIT_OK,
    EXIT_USAGE,
    BackendError,
    CancelToken,
    Cancelled,
    Emitter,
    watch_stdin,
)
from .registry import build_registry


def main(argv: list[str] | None = None) -> int:
    emit = Emitter()
    registry = build_registry()
    argv = list(sys.argv[1:] if argv is None else argv)
    if argv == ["--selfcheck"]:  # legacy alias used by run.bat
        argv = ["selfcheck"]
    token = CancelToken()
    try:
        args = build_parser(registry).parse_args(argv)
        if not args.command:
            names = ", ".join(n for n, c in registry.items() if not c.hidden)
            raise UsageError(f"a command is required: {names}")
        if args.cancel_on_stdin:
            watch_stdin(token)
        ctx = Context(emit=emit, cancel=token, params=load_params(args.params_file))
        emit.result(registry[args.command].run(args, ctx))
        return EXIT_OK
    except UsageError as exc:
        print(f"usage error: {exc}", file=sys.stderr)
        emit.error("usage", str(exc))
        return EXIT_USAGE
    except Cancelled:
        print("cancelled", file=sys.stderr)
        emit.error("cancelled", "cancelled")
        return EXIT_CANCELLED
    except BackendError as exc:
        print(f"{exc.code}: {exc.message}", file=sys.stderr)
        emit.error(exc.code, exc.message)
        return exc.exit_code
    except Exception as exc:  # report, never swallow
        traceback.print_exc(file=sys.stderr)
        emit.error("internal", f"{type(exc).__name__}: {exc}")
        return EXIT_FAILURE
