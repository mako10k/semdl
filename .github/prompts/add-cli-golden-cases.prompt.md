---
description: "Add SEMDL CLI golden cases and manifests. Use when defining success or failure regressions for selectors, parser behavior, runner behavior, argv cases, stdout/stderr output, or CLI edge cases."
name: "Add CLI Golden Cases"
argument-hint: "Describe the CLI behavior or edge case to capture"
agent: "agent"
---
Add or revise SEMDL CLI golden cases for the requested behavior.

Use this checklist:
- Source of truth: [docs/test-runner-format.md](../../docs/test-runner-format.md) and [docs/test-runner-contract.md](../../docs/test-runner-contract.md).
- Keep [docs/examples/testcases/cli-success.json](../../docs/examples/testcases/cli-success.json) and [docs/examples/testcases/cli-failure.json](../../docs/examples/testcases/cli-failure.json) aligned with [docs/examples/golden/](../../docs/examples/golden/).
- Update [docs/requirements.md](../../docs/requirements.md) only when the new case changes the documented acceptance surface.
- Run `semantics-code-reviewer` before finalizing if stdout or stderr expectations change, a new CLI argument or selector boundary is introduced, or the docs explicitly permit more than one valid behavior.

Expected output:
- Updated manifest entries
- Matching golden files
- Any required requirement or ADR follow-up called out explicitly
