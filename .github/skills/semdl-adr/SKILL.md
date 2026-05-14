---
name: semdl-adr
description: 'Create or update SEMDL ADRs. Use for architecture-impacting changes such as format boundaries, CLI contracts, selector rules, runner contracts, or layer responsibility changes.'
argument-hint: 'Describe the architecture decision to record'
---
# SEMDL ADR Workflow

## When to Use
- A change may alter .ssd, .ssm, or .ssq responsibilities
- A CLI public contract may change
- Selector resolution or safety rules may change
- Test runner contract may change
- Layer boundaries between library, CLI, and runner may change

## Procedure
1. Read [docs/adr/README.md](../../../docs/adr/README.md) and the latest accepted ADRs under [docs/adr](../../../docs/adr).
2. Check the requirements impact in [docs/requirements.md](../../../docs/requirements.md).
3. If the request is broad or under-specified, invoke the `todo-intake-auditor` subagent before drafting.
4. Draft or update an ADR using the template in [adr-template.md](./assets/adr-template.md).
5. Link affected docs such as [docs/cli.ebnf](../../../docs/cli.ebnf), [docs/test-runner-format.md](../../../docs/test-runner-format.md), and [docs/test-runner-contract.md](../../../docs/test-runner-contract.md).
6. Invoke the `semantics-code-reviewer` subagent before finalizing the ADR.
7. If the ADR is accepted, ensure the related requirements or golden artifacts are updated in the same change.

## Output
- Updated ADR file under docs/adr/
- Short note listing required follow-up files
