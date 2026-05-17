# 0035 Add Annotate Nondestructive Output Surface

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd annotate` は ADR 0018 で `--target inline|sidecar|auto` の routing と `--dry-run` preview までは固定されたが、non-destructive output surface は未実装のまま残っている。
そのため annotate だけが update command 群の中で `--stdout` と `--out` parity から外れており、source `.ssd` や sibling `.ssm` を変更せずに resolved target result を確認したり別ファイルへ流したりする経路が不足している。

一方で annotate の target matrix 自体は既に固定済みであり、この slice で inline/sidecar ownership や create-only/update responsibility を広げる必要はない。
必要なのは、既存 target resolution を前提にした narrow な output surface だけである。

## Decision

### 1. add resolved-profile stdout and out to annotate

この slice では `ssd annotate` に `--stdout`、`--out <output-file>`、`--out <output-file> --dry-run` を追加してよい。

- existing `--target inline|sidecar|auto` matrix はそのまま維持してよい
- `--dry-run` は既存 surface を維持してよい
- `--stdout` は `--dry-run` や `--out` と併用してはならない

### 2. resolved target decides the result profile

annotate の non-destructive result は resolved target profile に従ってよい。

- resolved target が `inline` のとき、`--stdout` と `--out` は canonical inline `.ssd` text を扱ってよい
- resolved target が `sidecar` のとき、`--stdout` と `--out` は canonical sidecar `.ssm` text を扱ってよい
- `--target auto` でも、result profile は resolved target に従ってよい

### 3. out-dry-run reuses the current preview contract

`ssd annotate ... --out <output-file> --dry-run` は既存 annotate preview format を再利用してよい。

- target file だけを output path に向けてよい
- resolved target profile は current target matrix のままにしてよい
- source file mutation や output file write は行わない

### 4. alias protection stays strict

annotate の `--out` は既知 source を alias してはならない。

- source `.ssd` を指す `--out` は failure とする
- resolved target が `sidecar` のときは source sibling `.ssm` も failure とする
- source sibling `.ssm` の保護は paired input だけでなく standalone input でも適用してよい

### 5. scope stays narrow

この slice では annotate の output surface だけを追加してよい。

- new `--format` option は導入しない
- multi-target annotate や selector semantics の拡張は行わない
- annotate target matrix 自体は ADR 0018 のままとする

## Consequences

- annotate も inline / sidecar 両 profile で non-destructive verification path を持てる
- `set`、`remove`、metadata add と同じ方向で output parity を揃えられる
- existing annotate routing contract を保ったまま output-only verification を追加できる

## Alternatives Considered

### annotate stdout/out を inline canonical `.ssd` に固定する

却下。
annotate は target matrix によって inline と sidecar の両方を書き分ける command であり、resolved sidecar result まで inline 化すると ownership boundary が崩れる。

### annotate out だけを追加し stdout を見送る

却下。
pipeline 接続と別ファイル出力の parity が再び崩れ、follow-up slice が半端に残る。

### annotate output と target matrix 拡張を同時に行う

却下。
ADR 0018 で固定した routing rule とは別の論点が混ざり、この slice の責務が広がりすぎる。

## Validation

- requirements.md と requirements.llmthink.dsl を annotate non-destructive output slice に同期する
- docs/cli.ebnf と annotate help / grammar help / root help goldens を更新する
- success manifest に annotate `--stdout`、`--out`、`--out --dry-run` の cases を追加する
- failure manifest に `--stdout` conflict と `--out` alias conflict cases を追加する
- CLI 実装で annotate の resolved-profile stdout / out path を追加し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/annotate-help.stdout
- docs/examples/golden/annotate-help.semdl.stdout
- docs/examples/golden/help-root.stdout
- docs/examples/golden/help-root.semdl.stdout
- docs/examples/golden/help-root-alias.semdl.stdout
- docs/examples/golden/help-grammar.stdout
- docs/examples/golden/help-grammar.semdl.stdout
