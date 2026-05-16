# 0011 Define Initial Normalize Stdout And Standalone Apply Slice

- Status: Superseded by 0015
- Date: 2026-05-15

## Context

`ssd normalize` は grammar と help には存在していたが、初期成功面は未固定だった。
requirements は normalize を「同一意味内容に対して安定した並び順と整形結果を与える」操作として位置付ける一方、
`--inline`、`--sidecar`、`--out`、`--dry-run` まで同時に実装すると書き込み経路と profile selection が広がりすぎる。

また、既存の `merge` は paired `.ssd` と `.ssm` の統合 view を返すが、normalize の固有責務は
「canonical order と formatting」を与えることにある。

## Decision

### 1. initial scope

最初の normalize 成功面は stdout を中心に始め、standalone `.ssd` に対する最小 apply を追加してよい。

- `ssd normalize <input.ssd> --stdout` の出力は canonical inline view とする
- stdout では input に paired `.ssm` が存在すれば統合して取り込む
- stdout では paired `.ssm` がなくても standalone `.ssd` を canonical inline view へ整形してよい
- bare `ssd normalize <input.ssd>` は standalone `.ssd` 入力に限って成功し、同じ `.ssd` ファイルを書き換える
- paired `.ssm` がある入力に対する bare normalize apply は sidecar lifecycle を未設計のまま持ち込まないため失敗とする

### 2. initial result shape

初期 canonical order は block kind と field order を固定する。

- block order
  - document
  - resource
  - segment
  - assertion
  - hypothesis
  - alternative
- 同一 kind 内は id 昇順
- field order は kind ごとの canonical sequence を使う

entity-level meta が存在する場合は `meta { ... }` を inline 化し、
embedding metadata が存在する場合は `meta` の内側で `embedding { ... }` を使う。

### 3. deferred surface

この slice では次を未実装のまま残す。

- `--out`
- `--inline` / `--sidecar` の profile selection
- `--dry-run`

## Consequences

- normalize の固有責務を canonical formatting に絞ったまま、standalone `.ssd` だけに限定して最小 apply を追加できる
- merge と同じく inline view を返しても、normalize は field/block order の source of truth を持つ
- paired sidecar apply と profile conversion policy は後続 slice に分離できる

## Alternatives Considered

### normalize を merge の別名として扱う

却下。
公開契約上の区別が消え、normalize 固有の canonicalization 境界が曖昧になる。

### `--out` や `--sidecar` まで同時に実装する

却下。
書き込み semantics、conflict policy、round-trip acceptance が必要になり、初期 slice が広がりすぎる。

### paired sidecar apply まで同時に実装する

却下。
`.ssd` だけを書き換えると stale `.ssm` が再合流しうるため、sidecar lifecycle policy を同時に決めない限り安全な apply 境界にならない。

## Validation

- requirements.md に stdout と standalone apply の分岐を書き分ける
- requirements.llmthink.dsl を同じ判断へそろえる
- normalize help と root help を更新する
- normalize success / failure case を manifest と golden へ追加する
- `ssd normalize --stdout` と standalone apply 実装を追加し、runner で受け入れを通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/examples/testcases/cli-success.json
- docs/examples/golden/normalize-help.stdout
- docs/examples/golden/normalize-help.semdl.stdout