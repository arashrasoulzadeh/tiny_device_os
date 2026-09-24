# CLAUDE.md

This project's agent instructions live in [`AGENTS.md`](./AGENTS.md) — read that first for
build/test commands, enforced rules (TDD, warnings-as-errors, CI matrix), and repo layout.

Everything in `AGENTS.md` applies here; this file only adds Claude-Code-specific notes.

## Claude Code specifics

- Treat `scripts/tdd_check.py`'s pre-commit rejection as a real gate, not a hook to bypass
  with `--no-verify` — write the test, don't skip the check.
- `PLAN.md` is the source of truth for architecture decisions and the phased roadmap; check
  it before proposing a structural change so you're not re-deciding something already locked.
- CI (`.github/workflows/sim.yml`) is the ground truth for "does this build" across Ubuntu,
  Windows, and macOS — a local build passing on one OS is not sufficient to call a CI fix done.
