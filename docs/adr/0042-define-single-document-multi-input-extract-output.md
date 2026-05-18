# 0042 Define Single-Document Multi-Input Extract Output

- Status: Accepted
- Date: 2026-05-18

## Context

multi-input `ssd extract` は [0023](0023-add-multi-input-extract-out-aggregation.md)、
[0025](0025-add-inline-multi-input-extract-stdout.md)、
[0027](0027-add-extract-stdout-bundle-and-multi-input-embedding.md) で
`canonical inline .ssd` / `canonical inline document` を返す contract として受理されている。

一方で current aggregate 実装は source ごとの `document` entity を rebasing してそのまま保持し、
multi-input output に複数 top-level `document` block を出している。
これは [docs/ssd.ebnf](../ssd.ebnf) の primary grammar artifact とも、
accepted extract ADR 群の single merged canonical inline `.ssd` intent とも一致しない。

さらに current artifacts には次の未決着が残っている。

- 複数 source document の `title` / `source_ref` を single canonical output にどう反映するか
- document-scoped `version` / `generator` と paired `document_meta` を multi-input aggregate でどう扱うか
- single-document contract を守りつつ、source order と rebased entity graph をどう維持するか

この未決着を implementation convenience で埋めると、lower-precedence fixture と output snapshot が
再び de facto contract になってしまう。

## Decision

### 1. multi-input extract inline outputs always render one top-level document

multi-input `extract` が inline `.ssd` を返す surface は、常に 1 つの top-level `document` block を返す。

- `ssd extract --out <output.ssd> <input> <input>...`
- no-provider `ssd extract --stdout <input> <input>...`
- embedding-enabled `ssd extract --stdout --format inline <input.ssd> <input.ssd>...`
- embedding-enabled `ssd extract --stdout --format bundle ...` の inline payload

この aggregate document id は canonical inline output で `D1` に固定してよい。

### 2. non-document entities still use deterministic rebasing

resource / segment / assertion / hypothesis / alternative は current extract contract どおり、
source order を保った deterministic rebasing を続けてよい。

- source 間 ID collision は rebasing で解決する
- `alternative_group` / `alternative.group` token も source ごとに rebased する
- entity graph と reference rewriting は current aggregate path を再利用してよい

### 3. aggregate document-body fields use same-value intersection only

multi-input aggregate の `document D1` body は、source document ごとに same-value で一致する field だけを保持してよい。

- `title`
- `source_ref`

1 つでも source 間で値が異なる、または存在有無が揃わない場合、その field は aggregate inline output から省略する。

この slice では source-precedence selection や source list encoding を導入しない。

### 4. document-scoped metadata also uses same-value intersection only

document-scoped `version` / `generator` と paired `document_meta` 由来 field も、
source document ごとに same-value で一致する field だけを aggregate result に保持してよい。

- same-value で一致する field は aggregate `D1` の document-scoped metadata として保持してよい
- 値が異なる、または存在有無が揃わない field は aggregate output から省略する
- per-source document metadata を別 id や synthetic block へ退避する新 surface はこの sliceに含めない

### 5. ambiguous document-level data is dropped, not guessed

multi-input aggregate では、document-level field の曖昧さを implementation convenience で埋めてはならない。

- first-source precedence を暗黙 default にしない
- synthetic joined string や ad hoc list encoding を作らない
- lower-precedence fixture shape を根拠に multiple `document` blocks を許可しない

### 6. validation must cover both output shape and loader invariants

core loader / validator は `.ssd` input の `document` cardinality を single-document contract に合わせて扱う。

- missing `document` block は invalid
- multiple top-level `document` block も invalid

これにより generated aggregate output と core validation の contract を揃える。

## Consequences

- multi-input extract の inline outputs は primary grammar artifact と accepted ADR intent に整合する
- per-source entity graph と source order は維持しつつ、document-level ambiguity だけ narrow に解決できる
- first-source precedence や new metadata transport を導入せずに current drift を修復できる
- source document ごとの non-consensus `title` / `source_ref` / `version` / `generator` / `document_meta` は aggregate inline output では失われうる
- richer source-document provenance transport は必要なら後続 ADR で明示的に追加する

## Alternatives Considered

### allow multiple top-level document blocks in `.ssd`

却下。
primary grammar artifact と accepted extract ADR 群の single canonical inline document intent を lower artifact に合わせて崩すことになる。

### default to first-source precedence for aggregate document fields

却下。
実装は簡単だが、precedence policy が未文書化のまま hidden default になる。

### invent a new source-list or synthetic document-metadata surface

却下。
format responsibility と parser / renderer / acceptance surface が広がり、今回の drift repair を超える change になる。

## Validation

- requirements.md と requirements.llmthink.dsl を single-document multi-input extract policy に同期する
- multi-input extract fixtures / goldens を single `document D1` shape に更新する
- core / CLI 実装で aggregate output と loader validation を修正する
- runner を通し、multi-input extract と new invalid multi-document check case を確認する

## Related Artifacts

- docs/adr/0023-add-multi-input-extract-out-aggregation.md
- docs/adr/0025-add-inline-multi-input-extract-stdout.md
- docs/adr/0027-add-extract-stdout-bundle-and-multi-input-embedding.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/ssd.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json