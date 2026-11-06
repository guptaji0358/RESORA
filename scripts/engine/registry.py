"""Command registry. Commands are registered by explicit import (no dynamic
discovery) so the engine stays packaging-friendly."""
from __future__ import annotations

from .command import Command


def build_registry() -> dict[str, Command]:
    from .commands import analytics, contract_test, duplicates, properties, scan, selfcheck

    commands = [selfcheck.COMMAND, scan.COMMAND, properties.COMMAND, analytics.COMMAND, duplicates.COMMAND,
                contract_test.COMMAND]
    registry: dict[str, Command] = {}
    for command in commands:
        if command.name in registry:
            raise RuntimeError(f"duplicate command name: {command.name}")
        registry[command.name] = command
    return registry
