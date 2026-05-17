# Documentation Pairing Workflow

## Status

- This workflow is a pilot.
- `.md` remains the authoritative editing format unless a later ADR says otherwise.
- `.ssd` companions are maintained to test SEMDL expressiveness and to surface where the current model loses information.

## Scope

This pilot applies to spec-grade documentation changed from this point forward.

- workflow and process docs under `docs/`
- requirements and other contract docs when they are touched in a change
- ADRs when the author decides the `.ssd` mirror adds signal

It does not require a one-shot conversion of every existing `.md` file in the repo.

## Pairing Rule

When a pilot-scoped document is created or substantively updated, create or update a paired `.ssd` document with the same stem when practical.

Examples:

- `docs/documentation-pairing-workflow.md`
- `docs/documentation-pairing-workflow.ssd`

Recommended pairing convention:

- keep the files in the same directory
- keep the same stem unless there is a strong reason not to
- record any intentional non-1:1 mapping inside the `.ssd` companion rather than leaving it implicit

## Authoring Order

For pilot-scoped docs, use this order.

1. Draft or revise the `.md` document first.
2. Add or update the paired `.ssd` document.
3. Run the expressiveness check below.
4. If the `.ssd` version cannot represent something cleanly, record the gap explicitly and keep moving.

## Expressiveness Check

Each paired update should answer these questions.

1. Can the `.ssd` document represent the main decisions, constraints, deferred boundaries, and examples without silently dropping meaning?
2. If structure had to change, is that change visible and justified in the `.ssd` document?
3. Are losses or awkward encodings recorded in an explicit note instead of being hidden?
4. Does the pair still make drift obvious to a reviewer?

Minimum expected contents for the `.ssd` companion:

- one `document` block identifying the source `.md`
- enough structural entities to carry the main decisions or rules
- at least one explicit note when the `.ssd` form is approximate, lossy, or intentionally compressed

## Review Gate

Before finalizing a pilot-scoped docs change, confirm all of the following.

- the `.md` file and `.ssd` companion were both considered
- the companion still reflects the current `.md` content
- any known expressiveness gap is written down in the pair
- authoritative status is still clear, with `.md` remaining primary during the pilot

## Rollout Plan

Start small and expand only if the pilot stays useful.

1. Keep this workflow document paired in both `.md` and `.ssd`.
2. For future spec changes, pair the touched process or contract doc if it is in pilot scope.
3. Reassess after a few slices whether requirements and ADRs need stricter pairing rules.

## Current Pilot Note

The repo already has structured mirrors such as `docs/requirements.llmthink.dsl`, but this workflow is specifically about trying document companions in `.ssd` form.
That experiment should stay explicit instead of being conflated with the existing DSL mirror.