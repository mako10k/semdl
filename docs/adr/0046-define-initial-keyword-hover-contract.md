# 0046 Define Initial Keyword Hover Contract

- Status: Accepted
- Date: 2026-05-21

## Context

SEMDL の current LSP は provider boundary、richer diagnostics、grammar-derived keyword completion まで進んでいる。
accepted ADR 0045 では、hover は source-of-truth を固定する前に導入してはならず、requirements、grammar artifact、CLI / core behavior のどこから hover text を導出するかを先に決める必要がある。

一方で local identifier completion や cross-file feature は、参照可能位置や stable symbol identity の契約がまだ弱い。
この段階で広い entity hover や explanation hover を導入すると、LSP 側 convenience で文面や対象範囲を先に固定しやすい。

今回必要なのは、keyword completion と同じ grammar-derived surface に閉じた initial hover contract を定義し、実装が editor-only explanation に流れないようにすることである。

## Decision

### 1. limit the first hover slice to grammar-derived keyword hover

initial hover は keyword hover に限定してよい。

- top-level block keyword
- allowed nested block keyword
- query header keyword
- query entry keyword

identifier token、field name、field value、cross-file symbol は initial hover の対象に含めない。

### 2. derive naming and syntax from authoritative artifacts

initial hover content は次の source-of-truth から導出しなければならない。

- format 名と表示名: requirements
- keyword の canonical syntax shape: primary grammar artifact

hover 実装は runtime convenience のための projection を持ってよいが、hover text の意味内容を Node や LSP 側で独自定義してはならない。

### 3. keep the first hover shape minimal and document-local

initial hover は document-local context だけで判定してよい。

- hover は token がどの keyword role にあるかだけを見てよい
- identifier explanation、field-level inference、cross-file lookup、binary explain delegationは初回に含めない
- 同一 keyword が複数 role を持つ場合は、現在文脈の grammar role を優先してよい

### 4. use hover as an adapter surface, not as an explanation authoring surface

initial hover の役割は、keyword の format surface と grammar shape を editor 上で参照しやすくすることに留める。

- prose-heavy explanation は初回に含めない
- explanation-rich hover が必要なら、後続 slice で source-of-truth と provider boundary を別途定義する

## Consequences

- hover を keyword completion と同じ grammar-derived surface に閉じられる
- runtime hover text が Node / LSP convenience で肥大化しにくくなる
- identifier hover や explain-backed hover を後続 slice へ安全に分離できる

## Alternatives Considered

### implement entity hover directly from local analyzer heuristics

却下。
identifier explanation と field-level inference の正本が未定義であり、editor-only semantics を増やしやすい。

### use a prose-only hover catalog authored inside the LSP implementation

却下。
hover 内容の正本を implementation convenience 側へ寄せることになり、ADR 0045 の source-of-truth 制約に反する。

### wait for explain-backed hover before adding any hover support

却下。
keyword hover は requirements と primary grammar artifact から閉じて導出でき、広い explanation hover を待つ必然はない。

## Validation

- requirements.md と requirements.llmthink.dsl に initial hover scope と source-of-truth 制約を同期する
- requirements.ssd companion に touched editor-integration decisions を反映する
- TypeScript LSP に keyword hover を追加し、keyword completion と同じ grammar-derived対象だけを返す
- hover tests で top-level block、nested block、query header、query entry の context-specific hover を検証する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/requirements.ssd
- docs/adr/0045-sequence-lsp-expansion-after-initial-slice.md
- docs/ssd.ebnf
- docs/ssq.ebnf