"""A/B benchmark: run tasks through `claude -p` with LIAS on vs off, record real usage.

  python .claude/lias/bench/harness.py [--reps 3] [--tasks id,id] [--arms lias,baseline]

Arms alternate within each rep to spread drift. Each run records turns, tool calls,
tokens and cost as reported by Claude Code, wall time, and a regex correctness check.
Uses a throwaway knowledge DB (LIAS_DB) so the real store is never touched.
Every run spends real subscription usage.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
LIAS = HERE.parent
ROOT = LIAS.parents[1]
sys.path.insert(0, str(LIAS))

from core import store  # noqa: E402

COMMON = "Glob,Grep,Read"
ARMS = {
    "lias": {"tools": f"{COMMON},Bash(python .claude/lias/lias.py *)", "flags": [], "env": {}},
    # Only the lias skill is blocked: --disable-slash-commands would also drop every other
    # skill description and shrink the prompt, which confounds the comparison.
    "baseline": {"tools": f"{COMMON},Bash", "flags": ["--disallowedTools", "Skill(lias)"],
                 "env": {"LIAS_DISABLE": "1"}},
}


def parse_stream(lines: list[str]) -> dict:
    """Extract metrics from `--output-format stream-json` lines."""
    tool_calls = skill_calls = 0
    result: dict = {}
    for raw in lines:
        try:
            e = json.loads(raw)
        except json.JSONDecodeError:
            continue
        if e.get("type") == "assistant":
            for c in e.get("message", {}).get("content", []):
                if c.get("type") == "tool_use":
                    tool_calls += 1
                    skill_calls += c.get("name") == "Skill"
        elif e.get("type") == "result":
            result = e
    u = result.get("usage", {})
    return {
        "turns": result.get("num_turns"), "tool_calls": tool_calls, "skill_calls": skill_calls,
        "cost_usd": result.get("total_cost_usd"), "output_tokens": u.get("output_tokens"),
        "input_tokens_total": sum(u.get(k, 0) or 0 for k in
                                  ("input_tokens", "cache_read_input_tokens", "cache_creation_input_tokens")),
        "cache_write_tokens": u.get("cache_creation_input_tokens"),
        "api_ms": result.get("duration_api_ms"), "is_error": result.get("is_error", True),
        "text": result.get("result", ""),
    }


def check(text: str, expect: list[str]) -> bool:
    return all(re.search(p, text, re.I) for p in expect)


def run_one(arm: str, task: dict, db: Path) -> dict:
    a = ARMS[arm]
    cmd = ["claude", "-p", task["prompt"], "--output-format", "stream-json", "--verbose",
           "--max-turns", "12", "--allowedTools", a["tools"], *a["flags"]]
    env = {**os.environ, "LIAS_DB": str(db), **a["env"]}
    t0 = time.perf_counter()
    p = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True,
                       encoding="utf-8", timeout=600)
    m = parse_stream(p.stdout.splitlines())
    m.update(arm=arm, task=task["id"], wall_s=round(time.perf_counter() - t0, 1),
             correct=check(m.pop("text"), task["expect"]) and not m["is_error"])
    return m


def summarize(rows: list[dict]) -> dict:
    out: dict = {}
    for (task, arm) in sorted({(r["task"], r["arm"]) for r in rows}):
        g = [r for r in rows if r["task"] == task and r["arm"] == arm]
        s: dict = {"n": len(g), "correct": sum(r["correct"] for r in g)}
        for k in ("cost_usd", "turns", "tool_calls", "output_tokens", "input_tokens_total",
                  "cache_write_tokens", "wall_s"):
            v = [r[k] for r in g if r.get(k) is not None]
            if v:
                s[k] = {"mean": round(statistics.mean(v), 4), "min": min(v), "max": max(v)}
        out[f"{task}/{arm}"] = s
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--tasks")
    ap.add_argument("--arms", default="lias,baseline")
    ns = ap.parse_args()
    spec = json.loads((HERE / "tasks.json").read_text(encoding="utf-8"))
    tasks = [t for t in spec["tasks"] if not ns.tasks or t["id"] in ns.tasks.split(",")]
    arms = ns.arms.split(",")
    with tempfile.TemporaryDirectory() as tmp:
        db = Path(tmp) / "bench.db"
        con = store.connect(db)
        for s in spec["seed"]:
            store.add(con, db_path=db, **s)
        con.close()
        rows = []
        results = HERE / "results"
        results.mkdir(exist_ok=True)
        out = results / f"{time.strftime('%Y%m%d-%H%M%S')}.jsonl"
        for rep in range(ns.reps):
            for task in tasks:
                for arm in (arms if rep % 2 == 0 else arms[::-1]):
                    r = run_one(arm, task, db)
                    r["rep"] = rep
                    rows.append(r)
                    with out.open("a", encoding="utf-8") as fh:
                        fh.write(json.dumps(r) + "\n")
                    print(f"rep{rep} {task['id']:<18}{arm:<9} cost={r['cost_usd']} turns={r['turns']} "
                          f"tools={r['tool_calls']} ok={r['correct']}", file=sys.stderr)
    print(json.dumps(summarize(rows), indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
