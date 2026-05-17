# 0028 Add Structural Add Stdout And Out

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd add` は現状、inline structural kind の in-place apply と `--dry-run`、および metadata-only add の `--target sidecar` create-only surface までを扱っている。
一方で structural add の non-destructive output surface、すなわち `--stdout` と `--out <output.ssd>` は requirements 上で後続 slice に分離されたまま残っている。

この穴が残ると、更新系 command の中で `add` だけが preview 以外の non-destructive path を持たず、`merge` / `normalize` と比較して別ファイル出力や pipeline 接続がしづらい。
ただし metadata-only add まで同じ slice で広げると、sidecar lifecycle、`--target` policy、inline metadata ownership を再度広げてしまう。

## Decision

### 1. structural add only

この slice の `--stdout` と `--out` は inline structural add に限定する。

- 対象 kind は `resource`, `segment`, `assertion`, `hypothesis`, `alternative`
- metadata-only `annotation` と `provenance` は引き続き `--target sidecar` create-only surface のままとする
- metadata-only add に `--stdout` や `--out` を与えた場合は failure とする

### 2. add `--stdout`

structural add は `--stdout` を受け付けてよい。

- `ssd add <kind> <file> [field=value ...] --stdout` は source file を変更せず、追加後の canonical inline `.ssd` を stdout へ返す
- paired `.ssd` + `.ssm` 入力でも stdout result は inline merged view とする
- `--stdout` と `--dry-run`、`--out` の併用は failure とする

### 3. add `--out <output.ssd>`

structural add は `--out <output.ssd>` を受け付けてよい。

- output は追加後の canonical inline `.ssd` とする
- source `.ssd` と paired `.ssm` は変更しない
- `--out <output.ssd> --dry-run` は preview の target file を output path に向けた non-destructive preview として扱ってよい
- この slice では sidecar output や profile selection は導入しない

### 4. alias protection stays strict

`--out` は既知 source を alias してはならない。

- source `.ssd` を指す `--out` は failure とする
- paired `.ssm` を持つ入力では、その `.ssm` を alias する `--out` も failure とする
- 判定は normalize / merge の non-destructive output slice と同じ path normalization 規則を再利用してよい

### 5. existing in-place apply remains default

option を付けない structural add は current behavior を維持する。

- bare `ssd add <structural-kind> ...` は source `.ssd` を更新する
- `--dry-run` は current preview surface を維持する
- `--target sidecar` broader policy は引き続き後続 slice に分離する

## Consequences

- `add` も update command として stdout / out parity を持てる
- metadata-only add の sidecar-only boundaryを壊さずに、structural add の non-destructive workflow を先に閉じられる
- output profile は inline only に留まり、sidecar generation や target matrix の追加設計を避けられる

## Alternatives Considered

### metadata-only add も同時に `--stdout` / `--out` へ広げる

却下。
`annotation` / `provenance` の render target、sidecar absent creation、`--target` semantics が同時に広がり、この slice より大きくなる。

### `--out` を source alias 許可付きの in-place shorthand にする

却下。
`merge` / `normalize` と異なる alias semantics になると、non-destructive output surface の一貫性が崩れる。

### `--stdout` の result を paired input では sidecar profile にする

却下。
stdout で複数ファイル surface を持ち込むと profile selection 設計を reopen してしまう。今回の slice は canonical inline result に限定する。

## Validation

- requirements.md と requirements.llmthink.dsl を structural add stdout/out slice に同期する
- docs/cli.ebnf と add help / root help goldens を更新する
- success manifest に structural add `--stdout`、`--out`、`--out --dry-run` を追加する
- failure manifest に source alias、paired sidecar alias、metadata-only add への unsupported output options を追加する
- CLI 実装で parse、preview、render/write path を追加し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/add-help.stdout
- docs/examples/golden/add-help.semdl.stdout
- docs/examples/golden/help-root.stdout
- docs/examples/golden/help-root.semdl.stdout
- docs/examples/golden/help-root-alias.semdl.stdout