---
description: "Plan a SEMDL specification change in order across requirements, ADRs, EBNF, manifests, and golden files. Use for test-first planning when changing CLI behavior, selectors, sidecar semantics, runner behavior, or implementation boundaries."
name: "Plan SEMDL Spec Change"
argument-hint: "Describe the spec change to plan"
agent: "agent"
---
Plan the requested SEMDL specification change, ensuring the architecture review and test-first planning steps are explicitly covered.

Requirements, in order:
- Use [docs/requirements.md](../../docs/requirements.md) as the primary requirements source.
- Check whether the change requires an ADR under [docs/adr/README.md](../../docs/adr/README.md).
- Use [docs/cli.ebnf](../../docs/cli.ebnf) for CLI syntax questions.
- Use [docs/test-runner-format.md](../../docs/test-runner-format.md) and [docs/test-runner-contract.md](../../docs/test-runner-contract.md) for test expectations.
- Explicitly rank decision drivers as: user instruction, authoritative docs / accepted ADRs, architectural correctness, spec-derived test-first acceptance design, then local implementation convenience.
- Do not choose a proposal because it is the shortest path if that path depends on implementation-derived expectations, unresolved spec ambiguity, or lower-precedence artifacts.
- Invoke `todo-intake-auditor` before locking the task list.
- Invoke `semantics-code-reviewer` before finalizing the proposed change set.

Expected output:
- A concrete change plan listing which docs, manifests, golden files, and ADRs must change.
- Explicit note whether user confirmation is required for any unresolved ambiguity.
- If requirements or documentation conflict, a short summary of the conflict and a proposed resolution strategy.
- A brief note when the shortest apparent implementation path was rejected in favor of a higher-priority spec, architecture, or test-first concern.
