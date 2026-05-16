# 0015 Define Paired Transform Apply And Runner Absent-File Assertions

- Status: Accepted
- Date: 2026-05-16

## Context

`ssd merge` は stdout のみ、`ssd normalize` は stdout と standalone `.ssd` への bare apply のみを持っていた。
paired `.ssd` + `.ssm` に対する bare apply を解禁するには、成功後の sibling `.ssm` をどう扱うかを fixed contract にしないと、
stale sidecar metadata が再合流して意味的に不安定になる。

同時に、既存 runner contract は file content 比較だけを持ち、successful transform 後に file が消えることを検証できなかった。
paired transform apply を acceptance で固定するには、sandbox 内の absent file assertion が必要である。

## Decision

### 1. paired transform apply lifecycle

- bare `ssd merge <input.ssd>` は paired `.ssm` が存在する入力に限って成功する
- successful merge apply は `<input>.ssd` を merged inline output へ書き換え、paired `<input>.ssm` を削除する
- bare `ssd normalize <input.ssd>` は standalone と paired の両方で成功してよい
- successful normalize apply は `<input>.ssd` を canonical inline output へ書き換える
- normalize apply が paired input で成功した場合は、paired `<input>.ssm` を削除する
- paired sidecar がない入力に対する bare merge apply は失敗する

### 2. preserved normalize result shape

normalize の canonical order と inline result shape は ADR 0011 の決定を引き継ぐ。

- block order は document, resource, segment, assertion, hypothesis, alternative
- 同一 kind 内は id 昇順
- field order は kind ごとの canonical sequence を使う
- entity-level meta と embedding metadata の inline rendering ルールも維持する

### 3. runner absent-file assertion

acceptance manifest は file removal を表現するために `expected_absent_files` を持ってよい。

- 値は runtime relative path の array とする
- runner は sandbox 内で各 path が存在しないことを比較する
- file removal を含む transform success や failure no-write case の検証に使う

## Consequences

- merge と normalize の paired apply は、successful write 後に stale `.ssm` を残さない
- merge write path を導入しても normalize の canonicalization 責務は維持できる
- runner は sidecar deletion を acceptance contract として表現できる
- `--out`、profile selection、conflict policy、dry-run は引き続き後続 slice に分離される

## Alternatives Considered

### paired apply でも `.ssm` を残す

却下。
bare apply 後に旧 sidecar が auto-discovery で再読込され、transform の結果が不安定になる。

### merge apply を standalone 入力でも成功させる

却下。
sidecar を伴わない apply では normalize との差分が薄くなり、最小 slice での責務境界が曖昧になる。

### runner で file removal を検証しない

却下。
paired apply の主張の中核が acceptance で固定されず、contract drift を招く。

## Validation

- requirements.md と requirements.llmthink.dsl を paired apply lifecycle へ同期する
- cli.ebnf を merge bare apply success に合わせて更新する
- merge / normalize help と grammar help を更新する
- test-runner-format.md と test-runner-contract.md に `expected_absent_files` を追加する
- merge paired apply success、normalize paired apply success、merge standalone apply failure の acceptance を追加する
- runner が absent file assertion を比較できるように実装し、suite を通す

## Related Artifacts

- docs/adr/0011-define-initial-normalize-stdout-slice.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/test-runner-format.md
- docs/test-runner-contract.md
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json