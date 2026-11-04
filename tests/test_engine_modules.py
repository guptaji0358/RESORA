"""Module-level tests: each engine module is exercised on its own, in-process,
plus a static check of the dependency boundaries (docs/ARCHITECTURE.md)."""
from __future__ import annotations

import ast
import io
import json
import tempfile
import unittest
from pathlib import Path

from scripts.engine.params import load_params
from scripts.engine.protocol import EXIT_INVALID_INPUT, BackendError, CancelToken, Cancelled, Emitter
from scripts.engine.registry import build_registry

ENGINE = Path(__file__).resolve().parents[1] / "scripts" / "engine"
GUI_TOOLKITS = {"PyQt5", "PyQt6", "PySide2", "PySide6", "tkinter"}


def imported_modules(path: Path) -> set[str]:
    """Imported module names; relative imports keep their leading dots."""
    found: set[str] = set()
    for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
        if isinstance(node, ast.Import):
            found.update(a.name for a in node.names)
        elif isinstance(node, ast.ImportFrom):
            found.add("." * node.level + (node.module or ""))
    return found


class Params(unittest.TestCase):
    def test_none_gives_empty(self):
        self.assertEqual(load_params(None), {})

    def test_reads_object_and_rejects_others(self):
        with tempfile.TemporaryDirectory() as tmp:
            good, bad = Path(tmp) / "g.json", Path(tmp) / "b.json"
            good.write_text('{"a": 1}', encoding="utf-8")
            bad.write_text("[1]", encoding="utf-8")
            self.assertEqual(load_params(good), {"a": 1})
            for path in (bad, Path(tmp) / "missing.json"):
                with self.assertRaises(BackendError) as cm:
                    load_params(path)
                self.assertEqual(cm.exception.exit_code, EXIT_INVALID_INPUT)


class Protocol(unittest.TestCase):
    def test_emitter_writes_one_json_line_per_envelope(self):
        out = io.StringIO()
        emit = Emitter(out)
        emit.progress(1, 2, "x")
        emit.result({"k": 1})
        lines = [json.loads(line) for line in out.getvalue().splitlines()]
        self.assertEqual([e["type"] for e in lines], ["progress", "result"])
        self.assertTrue(all(e["v"] == 1 for e in lines))

    def test_cancel_token(self):
        token = CancelToken()
        token.check()
        token.cancel()
        with self.assertRaises(Cancelled):
            token.check()


class Registry(unittest.TestCase):
    def test_registers_known_commands_with_unique_names(self):
        registry = build_registry()
        self.assertIn("selfcheck", registry)
        self.assertTrue(registry["_contract-test"].hidden)
        self.assertTrue(all(name == c.name for name, c in registry.items()))


class Boundaries(unittest.TestCase):
    def test_no_module_imports_a_gui_toolkit(self):
        for path in ENGINE.rglob("*.py"):
            top = {m.lstrip(".").split(".")[0] for m in imported_modules(path)}
            self.assertFalse(top & GUI_TOOLKITS, path)

    def test_commands_import_only_shared_modules(self):
        allowed = {"..", "..command", "..protocol"}  # relative imports only; siblings would be "." / ".x"
        logic_packages = ("..imaging.", "..reports.")
        for path in (ENGINE / "commands").glob("*.py"):
            for mod in imported_modules(path):
                if mod.startswith("."):
                    self.assertTrue(mod in allowed or mod.startswith(logic_packages), f"{path.name}: {mod}")

    def test_logic_packages_stay_separate(self):
        def top(path: Path) -> set[str]:
            return {m for m in imported_modules(path)}

        for path in (ENGINE / "imaging").glob("*.py"):
            self.assertFalse([m for m in top(path) if "reports" in m or "commands" in m or "protocol" in m], path.name)
        for path in (ENGINE / "reports").glob("*.py"):
            self.assertFalse([m for m in top(path) if "imaging" in m or "commands" in m or "PIL" in m], path.name)
        for name in ("classification", "aspect", "records", "discovery"):
            self.assertNotIn("PIL", {m.split(".")[0] for m in imported_modules(ENGINE / "imaging" / f"{name}.py")})
        for name in ("classification", "aspect"):  # pure logic: no sibling imports at all
            self.assertFalse([m for m in imported_modules(ENGINE / "imaging" / f"{name}.py") if m.startswith(".")])

    def test_core_modules_do_not_import_commands(self):
        for name in ("protocol", "command", "params", "cli"):
            mods = imported_modules(ENGINE / f"{name}.py")
            self.assertFalse([m for m in mods if "commands" in m or "registry" in m], name)

    def test_only_protocol_writes_to_stdout(self):
        for path in ENGINE.rglob("*.py"):
            if path.name == "protocol.py":
                continue
            src = path.read_text(encoding="utf-8")
            self.assertNotIn("sys.stdout", src, path)
            for node in ast.walk(ast.parse(src)):
                if isinstance(node, ast.Call) and getattr(node.func, "id", "") == "print":
                    self.assertTrue(any(k.arg == "file" for k in node.keywords), f"{path.name}: bare print()")


if __name__ == "__main__":
    unittest.main()
