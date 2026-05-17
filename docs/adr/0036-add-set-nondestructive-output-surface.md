# 0036 Add Set Nondestructive Output Surface

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd set` は現在、single-target field update に対して apply と `--dry-run` preview を持つが、non-destructive output surface は未実装のままである。
そのため set だけが update command 群の中で `--stdout` と `--out` parity から外れており、source `.ssd` や sibling `.ssm` を変更せずに update result を確認したり別ファイルへ流したりする経路が不足している。

一方で set の update ownership 自体はすでに固定されている。`path:` と current `id:` field update は inline `.ssd` を書き換え、`meta:` update は sidecar `.ssm` を書き換える。
この slice で必要なのは target boundary の再設計ではなく、既存 write profile に従った narrow な output surface だけである。

## Decision

### 1. add stdout and out to set

この slice では `ssd set` に `--stdout`、`--out <output-file>`、`--out <output-file> --dry-run` を追加してよい。

- existing single-target selector semantics はそのまま維持してよい
- existing `--dry-run` preview surface はそのまま維持してよい
- `--stdout` は `--dry-run` や `--out` と併用してはならない

### 2. resolved write profile decides the result payload

set の non-destructive result は既存 update ownership に従ってよい。

- inline write profile のとき、`--stdout` と `--out` は canonical inline `.ssd` text を扱ってよい
- sidecar write profile のとき、`--stdout` と `--out` は canonical sidecar `.ssm` text を扱ってよい
- `id:` selector は current field-layer resolution が inline write を許す範囲では inline payload を返してよい

### 3. out-dry-run reuses the current preview contract

`ssd set ... --out <output-file> --dry-run` は existing set preview format を再利用してよい。

- target profile は current write profile のままとする
- target file だけを output path に向けてよい
- source file mutation や output file write は行わない

### 4. alias protection stays strict

set の `--out` は既知 source を alias してはならない。

- source `.ssd` を指す `--out` は failure とする
- sidecar write profile のときは source sibling `.ssm` も failure とする
- source sibling `.ssm` の保護は paired input だけでなく standalone input でも適用してよい

### 5. scope stays narrow

この slice では set の output surface だけを追加してよい。

- selector language の拡張は行わない
- multi-target set は導入しない
- `--target` や `--format` は導入しない
- id/path/meta の current responsibility split は維持する

## Consequences

- set も inline / sidecar の両 profile で non-destructive verification path を持てる
- add、annotate、remove と同じ方向で output parity を揃えられる
- existing field-layer ownership を保ったまま output-only verification を追加できる

## Alternatives Considered

### set stdout/out を canonical inline `.ssd` に固定する

却下。
meta update の result まで inline 化すると、set の inline / sidecar ownership boundary が曖昧になる。

### set out だけを追加し stdout を見送る

却下。
pipeline 接続と別ファイル出力の parity が再び崩れる。

### set output と selector semantics 拡張を同時に行う

却下。
current single-target contract とは別論点が混ざり、slice が広がりすぎる。

## Validation

- requirements.md と requirements.llmthink.dsl を set non-destructive output slice に同期する
- docs/cli.ebnf と set help / grammar help / root help goldens を更新する
- success manifest に set inline stdout、inline out-dry-run、sidecar stdout、sidecar out、sidecar out-dry-run の cases を追加する
- failure manifest に `--stdout` conflict と `--out` alias conflict cases を追加する
- CLI 実装で set の resolved-profile stdout / out path を追加し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/set-help.stdout
- docs/examples/golden/set-help.semdl.stdout
- docs/examples/golden/help-root.stdout
- docs/examples/golden/help-root.semdl.stdout
- docs/examples/golden/help-root-alias.semdl.stdout
- docs/examples/golden/help-grammar.stdout
- docs/examples/golden/help-grammar.semdl.stdout
