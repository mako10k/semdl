---
name: semdl-spec-change
description: 'Plan or execute a SEMDL specification change in order: requirements, ADR need, grammar or runner references, then coordinated artifact updates. Use for coordinated, test-first updates across requirements, ADRs, CLI grammar, manifests, golden files, and implementation-boundary decisions.'
argument-hint: 'Describe the SEMDL spec change'
---
# SEMDL Spec Change Workflow

## When to Use
- CLI behavior changes
- Selector semantics change
- Sidecar or inline responsibilities change
- Acceptance examples, manifests, or golden files must be updated together

## Procedure
Follow these steps in order: identify governing requirements first, decide whether ADR handling is required, then expand to grammar, test, and review work only where applicable.
1. Read [docs/requirements.md](../../../docs/requirements.md).
2. Check whether the change affects format boundaries, CLI contracts, selector rules, runner contracts, or layer responsibilities using [docs/adr/README.md](../../../docs/adr/README.md).
3. Use the checklist in [spec-change-checklist.md](./assets/spec-change-checklist.md).
4. Invoke `todo-intake-auditor` before fixing the final task list.
5. If syntax or parser behavior changes, inspect [docs/cli.ebnf](../../../docs/cli.ebnf).
6. If test surface changes, inspect [docs/test-runner-format.md](../../../docs/test-runner-format.md) and [docs/test-runner-contract.md](../../../docs/test-runner-contract.md).
7. Invoke `semantics-code-reviewer` before finalizing the change set.

## Output
- Concrete list of files to change
- Note whether an ADR is required
- Explicit acceptance and failure cases to add or update
