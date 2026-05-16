# 0016 Define Create-Only Metadata Add

- Status: Accepted
- Date: 2026-05-16

## Context

初期 `ssd add` は inline structural kind の新規作成だけを扱っていた。
残っていた `annotation` と `provenance` を add 面へ広げるには、既存の `ssd set` と `ssd annotate` と責務が衝突しないように、
metadata add が新規作成なのか update/upsert なのかを先に固定する必要があった。

また、metadata-only add を paired `.ssm` がまだ存在しない入力でどう扱うかが未固定だった。
sidecar がない入力でも add を許可しないと、minimal `.ssd` から metadata を起こす初期経路を閉じてしまう。
一方で既存 field を上書きまで許すと、`add` と `set` と `annotate` の境界が曖昧になる。

## Decision

### 1. metadata-only add kinds

後続 add slice では metadata-only kind として `annotation` と `provenance` を追加する。

- `annotation` required field は `id`, `kind`, `text`
- `provenance` required field は `id`, `provenance_kind`
- `provenance` は `confidence` と `rationale` を optional field として受け付けてよい

### 2. sidecar-targeted only

metadata-only add はこの slice では `--target sidecar` を必須とする。

- bare inline add は structural kind にだけ使う
- metadata-only add に `--target sidecar` がない場合は failure とする
- metadata-only add に未知の target value を与えた場合も failure とする

### 3. create-only semantics

metadata-only add は create-only とする。

- target `id` は既存 semantic entity を指していなければならない
- requested metadata field が既に存在する場合は conflict failure とする
- successful metadata-only add は missing field だけを sidecar metadata として作成する
- 既存 field の更新責務は `ssd set` または `ssd annotate` に残す

### 4. sidecar creation lifecycle

paired `.ssm` が存在しない入力でも、successful metadata-only add は sibling `.ssm` を新規作成してよい。

- `.ssd` 本体は変更しない
- output は sidecar renderer の canonical field order に従う

### 5. annotate との責務差分

`ssd annotate` は add の別名ではなく、annotation kind を shorthand で扱う command とする。

- `annotate` は selector + kind + text の専用 argv を持つ
- `add annotation` は field map から create-only に metadata を作る経路とする
- append/history semantics はこの slice では導入しない

## Consequences

- `add` は structural entity 新規作成と metadata field 新規作成の両方を扱うが、どちらも create-only で責務が揃う
- `set` と `annotate` は既存 metadata field 更新の責務を維持できる
- paired `.ssm` がない入力でも metadata sidecar を起こせる
- `--out`、`--stdout`、broader target policy、conflict policy option は引き続き後続 slice に分離される

## Alternatives Considered

### metadata add を upsert にする

却下。
既存 field 更新まで許すと `add` と `set` と `annotate` の責務差分が崩れる。

### metadata add でも paired `.ssm` がない入力を失敗にする

却下。
sidecar をまだ持たない minimal `.ssd` から metadata-only add を始められず、初期更新経路として不自然になる。

### annotate を append/history semantics へ拡張する

却下。
この slice のデータモデルと renderer は単一 field value を前提としており、配列や履歴の導入は別設計が必要である。

## Validation

- requirements.md と requirements.llmthink.dsl を metadata-only add slice に同期する
- cli.ebnf を add kind と `--target sidecar` 受理に合わせて更新する
- add / annotate / grammar / root help goldens を更新する
- metadata-only add の success と failure acceptance を追加する
- CLI 実装で create-only conflict と absent sidecar creation を実装し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/add-help.stdout
- docs/examples/golden/annotate-help.stdout
- docs/examples/golden/help-grammar.stdout
- docs/examples/golden/help-root.stdout
