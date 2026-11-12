from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

LIAS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(LIAS))

from core import guard, runner, store  # noqa: E402


class StoreTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.db = Path(self.tmp.name) / "k.db"
        self.con = store.connect(self.db)

    def tearDown(self):
        self.con.close()
        self.tmp.cleanup()

    def add(self, value="Room 204, second floor", **kw):
        args = dict(kind="fact", category="schedule", key="mathematics lecture room",
                    value=value, source="user", db_path=self.db)
        args.update(kw)
        return store.add(self.con, **args)

    def test_add_and_retrieve(self):
        self.add()
        hits = store.search(self.con, "Where is my mathematics lecture?")
        self.assertEqual(len(hits), 1)
        self.assertIn("204", hits[0]["value"])

    def test_no_match_returns_nothing(self):
        self.add()
        self.assertEqual(store.search(self.con, "What is the wifi password for physics?"), [])

    def test_duplicate(self):
        self.add()
        self.assertEqual(self.add()["result"], "duplicate")

    def test_contradiction_supersedes_and_keeps_history(self):
        first = self.add()["id"]
        r = self.add("Room 310")
        self.assertEqual(r["superseded"], [first])
        hits = store.search(self.con, "mathematics lecture room")
        self.assertEqual([h["value"] for h in hits], ["Room 310"])
        row = self.con.execute("SELECT kind,status FROM knowledge WHERE id=?", (first,)).fetchone()
        self.assertEqual((row["kind"], row["status"]), ("historical", "superseded"))

    def test_unverified_cannot_override_confirmed(self):
        self.add()
        self.assertEqual(self.add("Room 999", status="unverified")["result"], "conflict_rejected")

    def test_expiry(self):
        self.add(ttl_days=-1)
        self.assertEqual(store.search(self.con, "mathematics lecture"), [])

    def test_secret_refused(self):
        with self.assertRaises(ValueError):
            self.add("api_key = abcdef1234567890")

    def test_bad_kind(self):
        with self.assertRaises(ValueError):
            self.add(kind="rumor")

    def test_forget_is_soft(self):
        i = self.add()["id"]
        store.forget(self.con, i, self.db)
        self.assertEqual(store.search(self.con, "mathematics lecture"), [])
        self.assertEqual(self.con.execute("SELECT COUNT(*) FROM knowledge").fetchone()[0], 1)

    def test_persistence_across_connections(self):
        self.add()
        self.con.close()
        self.con = store.connect(self.db)
        self.assertEqual(len(store.search(self.con, "mathematics lecture")), 1)

    def test_corrupt_db_recovers_from_backup(self):
        self.add()
        self.con.close()
        self.db.write_bytes(b"not a database" * 100)
        self.con = store.connect(self.db)
        self.assertEqual(len(store.search(self.con, "mathematics lecture")), 1)
        self.assertTrue(list(self.db.parent.glob("k.corrupt-*")))

    def test_fts_special_chars_safe(self):
        self.add()
        self.assertEqual(store.search(self.con, '"; DROP TABLE knowledge; -- OR *'), [])


class GuardTests(unittest.TestCase):
    def test_escape_refused(self):
        with self.assertRaises(ValueError):
            guard.resolve_inside("..")
        with self.assertRaises(ValueError):
            guard.resolve_inside("C:\\Windows")

    def test_inside_ok(self):
        self.assertEqual(guard.resolve_inside("."), guard.PROJECT_ROOT.resolve())


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        t = Path(self.tmp.name)
        self.root, self.cache, self.logs = t / "a", t / "c", t / "l"
        self.root.mkdir()

    def tearDown(self):
        self.tmp.cleanup()

    def make(self, name, body, **manifest):
        m = {"name": name, "args": {}, "timeout_s": 5, **manifest}
        (self.root / f"{name}.json").write_text(json.dumps(m))
        (self.root / f"{name}.py").write_text(body)

    def run_(self, name, args=None, **kw):
        return runner.run(name, args, root=self.root, cache=self.cache, logs=self.logs, **kw)

    def test_success_and_log(self):
        self.make("echo", "import json,sys;print(json.dumps({'x':json.load(sys.stdin)}))")
        r = self.run_("echo")
        self.assertEqual((r["status"], r["data"]), ("ok", {"x": {}}))
        self.assertTrue((self.logs / "events.jsonl").exists())

    def test_failure(self):
        self.make("bad", "import sys;sys.stderr.write('boom');sys.exit(3)")
        r = self.run_("bad")
        self.assertEqual((r["status"], r["exit_code"]), ("failed", 3))

    def test_non_json_output_fails_verification(self):
        self.make("txt", "print('hello')")
        self.assertEqual(self.run_("txt")["status"], "failed")

    def test_timeout(self):
        self.make("slow", "import time;time.sleep(5)", timeout_s=1)
        self.assertEqual(self.run_("slow")["status"], "timeout")

    def test_missing_dependency(self):
        self.make("dep", "print('{}')", deps=["no_such_module_xyz"])
        self.assertEqual(self.run_("dep")["status"], "missing_dependency")

    def test_invalid_args(self):
        self.make("a", "print('{}')", args={"n": {"type": "int", "required": True, "max": 5}})
        self.assertEqual(self.run_("a", {})["status"], "invalid_input")
        self.assertEqual(self.run_("a", {"n": "x"})["status"], "invalid_input")
        self.assertEqual(self.run_("a", {"n": 9})["status"], "invalid_input")
        self.assertEqual(self.run_("a", {"n": 1, "zzz": 1})["status"], "invalid_input")
        self.assertEqual(self.run_("a", {"n": True})["status"], "invalid_input")
        self.assertEqual(self.run_("a", {"n": 1})["status"], "ok")

    def test_path_arg_confined(self):
        self.make("p", "print('{}')", args={"p": {"type": "path", "required": True}})
        self.assertEqual(self.run_("p", {"p": "../.."})["status"], "invalid_input")

    def test_unknown_and_traversal_name(self):
        self.assertEqual(self.run_("nope")["status"], "error")
        self.assertEqual(self.run_("../x")["status"], "error")

    def test_destructive_needs_confirm(self):
        self.make("d", "print('{}')", destructive=True)
        self.assertEqual(self.run_("d")["status"], "needs_confirmation")
        self.assertEqual(self.run_("d", confirm=True)["status"], "ok")

    def test_cache_hit_and_bypass(self):
        self.make("c", "import time,json;print(json.dumps({'t':time.time()}))", cache_ttl_s=60)
        a, b = self.run_("c"), self.run_("c")
        self.assertFalse(a["cached"])
        self.assertTrue(b["cached"])
        self.assertEqual(a["data"], b["data"])
        self.assertFalse(self.run_("c", use_cache=False)["cached"])

    def test_large_output_truncated(self):
        self.make("big", "import json;print(json.dumps({'s':'x'*50000}))")
        r = self.run_("big")
        self.assertTrue(r.get("truncated"))


class IntegrationTests(unittest.TestCase):
    def cli(self, *args):
        p = subprocess.run([sys.executable, str(LIAS / "lias.py"), *args],
                           capture_output=True, text=True, encoding="utf-8")
        return p.returncode, json.loads(p.stdout)

    def test_source_sizes_real(self):
        code, out = self.cli("run", "source_sizes", "--json", '{"root":".claude/lias","exts":"py","top":3}')
        self.assertEqual((code, out["status"]), (0, "ok"))
        self.assertGreater(out["data"]["files"], 3)
        self.assertLessEqual(len(out["data"]["largest"]), 3)

    def test_source_sizes_escape_rejected(self):
        code, out = self.cli("run", "source_sizes", "--json", '{"root":".."}')
        self.assertEqual((code, out["status"]), (1, "invalid_input"))

    def test_doctor(self):
        code, out = self.cli("doctor")
        self.assertTrue(out["data"]["fts5"])

    def test_hook_silent_without_match_and_survives_garbage(self):
        hook = LIAS / "hooks" / "prompt_retrieve.py"
        for stdin in ("not json", json.dumps({"prompt": "zzqx unmatched quux"}), json.dumps({"prompt": "/help me"})):
            p = subprocess.run([sys.executable, str(hook)], input=stdin, capture_output=True, text=True)
            self.assertEqual((p.returncode, p.stdout), (0, ""))


if __name__ == "__main__":
    unittest.main()
