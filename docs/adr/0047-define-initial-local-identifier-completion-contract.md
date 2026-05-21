# 0047 Define Initial Local Identifier Completion Contract

- Status: Accepted
- Date: 2026-05-21

## Context

accepted ADR 0045 では、completion を grammar-derived keyword completion と local identifier completion に分離し、identifier completion は source-of-truth、単一文書内 scope、参照可能条件を先に固定してから導入すると決めた。

keyword completion と keyword hover までは grammar artifact と requirements から機械的に導出しやすかったが、identifier completion は「どの identifier を列挙するか」と「どの位置で identifier 候補を出してよいか」を曖昧にすると、editor-only な field 推論や cross-file 想定を先行させやすい。

一方で current requirements には、document-local structural relation として少なくとも次が既に固定されている。

- `segment.source` は `resource` を参照する
- `assertion.subject` は `resource` を参照する
- `assertion.source_segment` は `segment` を参照する
- `hypothesis.about` は `assertion` を参照する

このため initial local identifier completion は、既存 requirements にある relation だけを使う narrow slice として定義できる。

## Decision

### 1. use current `.ssd` top-level block declarations as the initial identifier source of truth

initial local identifier completion の候補 source-of-truth は、current `.ssd` document 内で宣言済みの top-level block identifier に限定する。

- `document`、`resource`、`segment`、`assertion`、`hypothesis`、`alternative`、`document_meta`、`meta` の top-level declaration から候補を抽出してよい
- cross-file symbol、workspace index、paired file lookup、binary audit source はこの slice に含めない
- TypeScript analyzer fallback は候補抽出の実装に使ってよいが、candidate ownership 自体は requirements 既存 relation を超えて拡張してはならない

### 2. restrict eligibility to explicit reference fields already fixed by requirements

initial local identifier completion を出してよい位置は、requirements 既存 relation で参照先 kind が決まっている field value だけに限定する。

- `segment.source` では current document 内の `resource` identifier だけを候補にしてよい
- `assertion.subject` では current document 内の `resource` identifier だけを候補にしてよい
- `assertion.source_segment` では current document 内の `segment` identifier だけを候補にしてよい
- `hypothesis.about` では current document 内の `assertion` identifier だけを候補にしてよい

### 3. keep the initial slice document-local and non-inferential

この slice では、field-level semantics を requirements 既存 relation 以上に拡張してはならない。

- `predicate`、`kind`、`group`、free scalar field、quoted string position では identifier completion を出してはならない
- `object` のように current requirements だけでは reference eligibility が固定されていない field では completion を出してはならない
- current line の prefix と current block kind / field 名だけで判定してよく、broader semantic analysis や cross-file resolution を要求してはならない

## Consequences

- initial identifier completion を current requirements relation だけで閉じられる
- current `.ssd` document を超える symbol search、stable symbol identity、rename / references contract を持ち込まずに済む
- `object` や `group` のような ambiguous field は deferred のまま保てる

## Alternatives Considered

### offer identifier completion for every scalar identifier position

却下。
grammar 上 scalar-value が identifier を許していても、`predicate`、`kind`、`group` などは structural reference と限らず、editor-only 推論が広がる。

### include cross-file identifiers from paired documents or workspace files

却下。
workspace-wide index、cross-file audit source、stable symbol identity が未定義のままで、current deferred boundary を破る。

### include `object` in the first identifier completion slice

却下。
examples では quoted string としての usage が広く、current requirements だけでは reference eligibility を固定できない。

## Validation

- requirements.md と requirements.llmthink.dsl に identifier completion の source-of-truth、document-local scope、eligible field を同期する
- requirements.ssd companion に touched editor-integration decision を同期する
- TypeScript LSP analyzer / provider / server に document-local identifier completion を追加する
- analyzer / provider tests で eligible field と ineligible field を固定する
- semantics-code-reviewer で contract の narrowness と deferred boundary を確認する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/requirements.ssd
- docs/adr/0045-sequence-lsp-expansion-after-initial-slice.md
- editors/vscode/server/src/analyzer.ts
- editors/vscode/server/src/analysisProvider.ts