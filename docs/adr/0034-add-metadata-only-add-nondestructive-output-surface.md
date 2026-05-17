# 0034 Add Metadata-Only Add Nondestructive Output Surface

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd add` は現在、inline structural kind に対して `--stdout` と `--out` を持ち、metadata-only kind である `annotation` と `provenance` は `--target sidecar` create-only apply と `--dry-run` までを扱っている。
一方で metadata-only add には non-destructive output surface がなく、source `.ssd` や sibling `.ssm` を変更せずに sidecar result を preview・stdout・別ファイル出力する経路が存在しない。

この穴が残ると、metadata-only add だけが update command 群の中で non-destructive parity から外れ、paired source を保ったまま generated sidecar を確認したり別パスへ流したりする経路が不足する。
一方で inline target、auto target、upsert、broader conflict policy まで同時に入れると、`add` / `set` / `annotate` の責務境界が広がりすぎる。

## Decision

### 1. add sidecar-only nondestructive output surfaces to metadata add

この slice では metadata-only add に限って `--stdout`、`--out <output.ssm>`、`--out <output.ssm> --dry-run` を追加してよい。

- 対象 kind は `annotation` と `provenance` に限定してよい
- `--target sidecar` 必須と create-only semantics は維持してよい
- inline target、auto target、upsert、broader conflict policy option はこの slice に含めない

### 2. stdout and out use canonical sidecar text

metadata-only add の `--stdout` と `--out` は add 後の canonical sidecar `.ssm` text を扱ってよい。

- source `.ssd` は変更しない
- source sibling `.ssm` が存在しても変更しない
- paired `.ssm` が存在しない入力でも、successful result は canonical sidecar text として返してよい

### 3. dry-run previews the same sidecar target

metadata-only add の `--dry-run` と `--out <output.ssm> --dry-run` は、apply と同じ validation を通したうえで file write を行わず preview を返してよい。

- bare `--dry-run` は sibling sidecar path を target file として扱ってよい
- `--out <output.ssm> --dry-run` は output path を target file として扱ってよい
- preview には target profile、target file、source profile、result profile、preserved source paths、kind、target id、field payload を含めてよい

### 4. alias protection stays strict

metadata-only add の `--out` は既知 source を alias してはならない。

- source `.ssd` を指す `--out` は failure とする
- paired `.ssm` を持つ入力では、その `.ssm` を alias する `--out` も failure とする
- 判定は既存 non-destructive output slices と同じ path normalization 規則を再利用してよい

### 5. option matrix stays narrow

この slice では metadata-only add の option surface を次に限定してよい。

- `ssd add annotation <file> [field=value ...] --target sidecar`
- `ssd add annotation <file> [field=value ...] --target sidecar --dry-run`
- `ssd add annotation <file> [field=value ...] --target sidecar --stdout`
- `ssd add annotation <file> [field=value ...] --target sidecar --out <output.ssm>`
- `ssd add annotation <file> [field=value ...] --target sidecar --out <output.ssm> --dry-run`
- `provenance` でも同じ surface を使ってよい

`--stdout` は `--dry-run` や `--out` と併用してはならない。
`--target` は metadata-only add でも引き続き `sidecar` のみとする。

## Consequences

- metadata-only add も sidecar profile に閉じたまま non-destructive parity を持てる
- `add` / `set` / `annotate` の create-only / update 責務境界を保ったまま output verification 経路を追加できる
- sidecar 不在入力でも、生成予定 `.ssm` を apply 前に確認できる

## Alternatives Considered

### metadata-only add に inline target や auto target を同時に導入する

却下。
sidecar ownership と command 責務境界が同時に広がり、この slice より大きくなる。

### metadata-only add の stdout / out でも canonical inline `.ssd` を返す

却下。
metadata-only add は sidecar-only target policy を維持する sliceであり、result profile まで inline へ変えると structural add surface と境界が混ざる。

### metadata-only add を out だけ追加し stdout は見送る

却下。
pipeline 接続と別ファイル出力の parity が再び崩れ、後続 slice が中途半端に残る。

## Validation

- requirements.md と requirements.llmthink.dsl を metadata-only add non-destructive output slice に同期する
- docs/cli.ebnf と add help / grammar help / root help goldens を更新する
- success manifest に metadata add `--stdout`、`--out`、`--out --dry-run` の cases を追加する
- failure manifest に `--stdout` conflict と `--out` alias conflicts を追加する
- CLI 実装で metadata-only add の sidecar stdout / out path を追加し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/add-help.stdout
- docs/examples/golden/add-help.semdl.stdout
- docs/examples/golden/help-grammar.stdout
- docs/examples/golden/help-grammar.semdl.stdout