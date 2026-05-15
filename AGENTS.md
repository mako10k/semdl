# Project Guidelines

## Scope
This repository is currently specification-first. Default to updating or reading the design documents before proposing implementation work.

## Authoritative Docs
- Requirements and core model: [docs/requirements.md](docs/requirements.md)
- ADR process: [docs/adr/README.md](docs/adr/README.md)
- Accepted ADRs: [docs/adr/0001-use-architecture-decision-records.md](docs/adr/0001-use-architecture-decision-records.md)
- CLI grammar: [docs/cli.ebnf](docs/cli.ebnf)
- Test runner manifest: [docs/test-runner-format.md](docs/test-runner-format.md)
- Test runner execution contract: [docs/test-runner-contract.md](docs/test-runner-contract.md)

## Working Rules
- Treat architecture-impacting changes as ADR work first. If format boundaries, CLI contracts, selector rules, runner contracts, or layer responsibilities change, update or add an ADR before implementation.
- Follow the repo's test-first policy. For new CLI or format behavior, define or update acceptance examples, golden files, and manifests before proposing code.
- Keep requirements, ADRs, EBNF, manifests, and golden files aligned in the same change when behavior changes.
- Do not invent CLI syntax, runner behavior, or selector semantics when the answer exists in the docs above.
- For non-trivial multi-artifact work, consult [docs/pitfall-prevention.md](docs/pitfall-prevention.md) before finalizing the change. Prefer `search_pitfall_precautions` and related pitfall tools when available; if the tool domain is unavailable, use the repo-local fallback documented there.
- When a meaningful failure exposes an authoritative-artifact update miss, record it as a pitfall. Prefer the pitfall tools; if unavailable, add or refine a local case in [docs/pitfall-prevention.md](docs/pitfall-prevention.md) and keep the concrete fix anchored in existing authoritative docs, manifests, and goldens.

## Delegation Rules
- Before locking in a ToDo list for a non-trivial user request, invoke the `todo-intake-auditor` subagent to check for ambiguity, shallow task breakdowns, hidden assumptions, and whether user confirmation is needed.
- Before finalizing substantial design, implementation, or spec updates, invoke the `semantics-code-reviewer` subagent to challenge fit-to-requirements, complexity, symmetry, consistency, and stopgap design.
- If either subagent reports blocking issues and they can be resolved locally, revise and re-run it once. If user confirmation is required, ask the user instead of guessing.

## Prompt And Skill Entry Points
- Use the workspace prompts under `.github/prompts/` for recurring entry points: ADR proposal, spec-change planning, golden-case updates, ToDo intake audit, and SEMDL review.
- Use the workspace skills under `.github/skills/` when the work spans multiple artifacts and needs a repeatable workflow: ADR work, coordinated spec changes, and golden-test updates.
- Prefer prompts for single focused tasks and skills for multi-step workflows.

## Current Repo Shape
- The repo currently centers on docs and golden assets rather than implementation code.
- Sidecar semantics, CLI behavior, and runner behavior are specified explicitly; prefer linking those docs over restating them in chat.
