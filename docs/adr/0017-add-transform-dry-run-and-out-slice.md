# 0017 Add Transform Dry-Run And Out Slice

- Status: Accepted
- Date: 2026-05-16

## Context

`ssd merge` と `ssd normalize` は paired apply lifecycle を持つようになったが、option surface はまだ bare apply と `--stdout` に偏っていた。
次の最小 slice として `--dry-run` と `--out` を開くには、merge と normalize の責務境界、preview の shape、非破壊出力時の file lifecycle を先に固定する必要がある。

特に merge は paired `.ssm` がない入力へ広げると normalize と責務が重なる。
また `--out` が source `.ssd` や paired `.ssm` を alias できると、この slice の non-destructive contract が argv だけで破れてしまう。

## Decision

### 1. shared transform option slice

`merge` と `normalize` の両方に次の option 面を追加する。

- `--dry-run`
- `--out <output.ssd>`
- `--out <output.ssd> --dry-run`

この slice の output profile は inline に固定する。
`--inline`、`--sidecar`、`--format`、conflict policy は引き続き後続 slice に分離する。

### 2. merge boundary stays paired-only

`merge` は `--stdout` だけでなく `--dry-run` と `--out` でも paired `.ssm` がある入力に限って成功する。

- standalone input に対する `merge --dry-run` は failure とする
- standalone input に対する `merge --out` も failure とする
- in-place apply は従来どおり sibling `.ssm` を削除する

### 3. normalize keeps standalone and paired support

`normalize` は `--dry-run` と `--out` でも standalone / paired の両方で成功してよい。

- in-place apply と `--dry-run` は input path を target とした canonical inline result を扱う
- paired input の in-place apply は sibling `.ssm` を削除する
- `--out` 系は source files を変更しない

### 4. dry-run preview contract

transform `--dry-run` は既存 update preview format を再利用する。

- target profile は `inline`
- in-place dry-run は target file を input `.ssd` とする
- paired in-place dry-run は remove 対象の sibling `.ssm` を preview detail に含める
- `--out <output.ssd> --dry-run` は target file を output path とし、source `.ssd` / `.ssm` を preserve する detail を含める

### 5. out-file safety

`--out` は non-destructive とし、この slice では source `.ssd` と paired `.ssm` を変更または削除しない。

- `--out` が source `.ssd` を指す場合は failure
- paired input で `--out` が sibling `.ssm` を指す場合も failure

## Consequences

- merge と normalize は inline-only のまま preview / non-destructive write surface を獲得する
- merge の paired-only boundary を `--dry-run` と `--out` にも維持できる
- runner は source preservation と output file generation を acceptance で固定できる
- profile selection と conflict policy は別 slice のまま保てる

## Alternatives Considered

### `--target` や `--format` まで同時に解禁する

却下。
profile selection と sidecar lifecycle が広がり、slice が大きくなりすぎる。

### merge の `--out` を standalone 入力でも許可する

却下。
normalize の別名に近づき、既存の責務境界を崩す。

### `--out` で source path や sibling sidecar への alias を許可する

却下。
non-destructive contract が argv の別名指定で崩れる。

## Validation

- requirements.md と requirements.llmthink.dsl を option slice に同期する
- cli.ebnf を merge / normalize の `--dry-run` と `--out` に合わせて更新する
- merge / normalize / grammar help goldens を更新する
- merge / normalize の dry-run / out success cases と alias / paired-required failure casesを追加する
- CLI 実装で non-destructive out と dry-run preview を実装し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/merge-help.stdout
- docs/examples/golden/normalize-help.stdout
- docs/examples/golden/help-grammar.stdout
