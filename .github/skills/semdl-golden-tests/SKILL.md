---
name: semdl-golden-tests
description: 'Add or revise SEMDL golden tests. Use for CLI success cases, failure cases, manifest updates, selector boundary cases, argv-based runner cases, and exact stdout or stderr expectations.'
argument-hint: 'Describe the behavior or edge case to capture'
---
# SEMDL Golden Test Workflow

## When to Use
- Adding CLI success cases
- Adding CLI failure cases
- Extending selector edge coverage
- Updating expected stdout or stderr outputs
- Keeping manifests and golden files aligned

## Procedure
1. Read [docs/test-runner-format.md](../../../docs/test-runner-format.md) and [docs/test-runner-contract.md](../../../docs/test-runner-contract.md).
2. Determine whether the case belongs in [docs/examples/testcases/cli-success.json](../../../docs/examples/testcases/cli-success.json) or [docs/examples/testcases/cli-failure.json](../../../docs/examples/testcases/cli-failure.json).
3. Use the checklist in [golden-case-checklist.md](./assets/golden-case-checklist.md).
4. If the new case implies a behavior decision or boundary change, invoke `semantics-code-reviewer` before finalizing.
5. If the requested case is ambiguous or under-scoped, invoke `todo-intake-auditor` before editing.
6. Update [docs/requirements.md](../../../docs/requirements.md) when the documented acceptance surface changes.

## Output
- Manifest updates
- Matching golden files
- Any required requirement or ADR follow-up
