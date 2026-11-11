# User preferences (hard rules)

Source: [U] = user's message of 2026-11-06 (memory-system request) or CLAUDE.md written for the user; [V] = also present in CLAUDE.md.

## Process
- Work **milestone by milestone**; user reviews Claude's implementation reports. **Do not start the next milestone before approval.** [U]
- **Never commit or push automatically.** Commit only on explicit permission; `run.bat /commit` and `/push` are explicit user actions only. [U][V]
- After each meaningful task, write a concise conventional-commit block in `commit.txt` describing only work actually done. [V]
- **Do not modify `main.py`** (legacy Tkinter prototype, reference only). **Do not restore `image_analyzer.py`.** [U][V]
- Preserve existing functionality. [U]
- Avoid unnecessary tests and repeated builds; don't repeat a successful verification without reason. [U][V]
- **Never claim visual verification** unless the running app/render was actually inspected; report automated and manual verification separately. [U][V]
- Practical implementation reports (what changed, what verified, what not). [U]
- Ask only when essential info is missing; allow creative ownership where the user explicitly grants it. [U]
- Don't implement features without explicit instruction. [V]
- Never modify/delete analyzed user images; don't kill unrelated Python/Qt processes. [V]
- Never discard user modifications; never force-push. [V]
- New docs only when genuinely needed. [V]

## Design
- Custom RESORA identity, dark-only; no stock Windows/Fluent-style look (see DESIGN_MEMORY.md). [U][V]

## Memory maintenance [U, 2026-11-06]
- Memory updates after meaningful work are mandatory and automatic; report which memory files were updated. Never invent the user's manual testing. See WORKFLOW.md.

## Memory/credit
- Goal: reduce token/credit use by consulting `AgentsMemory/` before work instead of re-exploring. Do not claim a verified reduction (e.g. "20x") without measurement. [U]
