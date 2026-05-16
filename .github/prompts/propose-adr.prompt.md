---
description: "Propose an ADR or architecture decision record for a SEMDL architecture change. Use when format boundaries, CLI contracts, selector rules, runner contracts, implementation layering, or layer responsibilities may change."
name: "Propose ADR"
argument-hint: "Describe the architecture change or decision to evaluate"
agent: "agent"
---
Create or update an ADR proposal for the described SEMDL architecture change.

Requirements, in order:
- Treat [docs/adr/README.md](../../docs/adr/README.md) as the operating rule.
- Reuse [docs/adr/0001-use-architecture-decision-records.md](../../docs/adr/0001-use-architecture-decision-records.md) as the style baseline.
- Link to [docs/requirements.md](../../docs/requirements.md), [docs/cli.ebnf](../../docs/cli.ebnf), [docs/test-runner-format.md](../../docs/test-runner-format.md), and [docs/test-runner-contract.md](../../docs/test-runner-contract.md) only when they directly apply to the proposed decision.
- Before finalizing the plan, use the `todo-intake-auditor` subagent if the requested change affects more than one architectural surface named above or leaves key requirements undefined.
- Before finalizing the ADR contents, use the `semantics-code-reviewer` subagent to challenge requirement fit, complexity, and symmetry.

Output target:
- Prefer creating or updating a file under docs/adr/.
- If the change is not architecture-impacting, explain why and stop instead of forcing an ADR.
