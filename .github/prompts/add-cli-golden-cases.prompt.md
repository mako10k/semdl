---
description: "Add SEMDL CLI golden cases and manifests. Use when defining new success or failure cases for selectors, parser behavior, runner behavior, or CLI output."
name: "Add CLI Golden Cases"
argument-hint: "Describe the CLI behavior or edge case to capture"
agent: "agent"
---
Add or revise SEMDL CLI golden cases for the requested behavior.

Requirements:
- Treat [docs/test-runner-format.md](docs/test-runner-format.md) and [docs/test-runner-contract.md](docs/test-runner-contract.md) as authoritative.
- Keep [docs/examples/testcases/cli-success.json](docs/examples/testcases/cli-success.json) and [docs/examples/testcases/cli-failure.json](docs/examples/testcases/cli-failure.json) aligned with the golden files under docs/examples/golden/.
- Update [docs/requirements.md](docs/requirements.md) if the new case changes the documented acceptance surface.
- Invoke `semantics-code-reviewer` before finalizing if the new case implies a behavior or boundary decision.

Expected output:
- Updated manifest entries
- Matching golden files
- Any required requirement or ADR follow-up called out explicitly
