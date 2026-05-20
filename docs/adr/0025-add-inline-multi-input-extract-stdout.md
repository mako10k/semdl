# 0025 Add Inline Multi-Input Extract Stdout

- Status: Accepted
- Date: 2026-05-17

## Context

ADR 0023 では multi-input extract を `--out` に限定し、`--stdout` の multi-input surface は後続 slice に分離した。
その判断は aggregation、rebasing、alias safety をまず non-destructive write path に閉じるには妥当だったが、現在は次の歪みが残っている。

- no-provider の `ssd extract --stdout` は raw `.txt` だけ成功し、existing `.ssd` は single-input でも失敗する
- 0023 で実装済みの aggregation / rebasing path を stdout の canonical inline view に再利用できない
- multi-input stdout と profile selection / stdout multiplexing が同じ deferred bucket に入っており、より小さい follow-up slice を acceptance で固定できない

一方で embedding-enabled stdout を multi-input まで広げると、generated `.ssm` と inline `.ssd` を stdout へどう載せるかという multiplexing contract まで同時に決める必要がある。

## Decision

### 1. extend only no-provider `extract --stdout`

この slice では no-provider の `ssd extract --stdout <input.ssd|input.txt>...` だけを広げる。

- trailing input は 1 件以上を受け付ける
- result は 1 本の canonical inline `.ssd` text とする
- single existing `.ssd` without provider もこの surface で成功してよい
- single raw `.txt` stdout success は既存どおり維持する

### 2. reuse the existing aggregate-and-rebase path

multi-input no-provider stdout は、0023 で導入した resolved-view aggregation と deterministic rebasing を再利用する。

- existing `.ssd` は paired `.ssm` が存在すれば resolved view として読む
- plain `.txt` は skeletal document view へ変換する
- source order、entity ID rebasing、reference rewriting、`alternative_group` / `alternative.group` token rebasing は `extract --out` と同じ契約に従う
- stdout result は rebased merged view の canonical inline render とする

### 3. embedding-enabled stdout stays narrow

`--embed-provider` と `--embed-model` を伴う `extract --stdout` は引き続き single existing `.ssd` に限定する。

- generated output は existing contract どおり generated `.ssm` profile text とする
- raw `.txt` + embedding + `--stdout` rejection は維持する
- multi-input + embedding + `--stdout` は引き続き failure とする

### 4. profile selection and stdout multiplexing remain deferred

この slice では次を導入しない。

- `--target`
- stdout での Solo mode / Companion mode selection
- inline `.ssd` と generated `.ssm` の multiplexed stdout framing

## Consequences

- no-provider stdout を canonical inline `.ssd` read surface として統一できる
- 0023 の aggregation 実装を stdout にも再利用できる
- embedding-enabled stdout を single-input `.ssm` text に据え置くことで output framing を増やさずに済む
- `extract --stdout` は no-provider path と embedding path で surface が分かれるため、grammar / help / acceptance で境界を明示する必要がある

## Alternatives Considered

### multi-input stdout でも embedding-enabled `.ssm` を返す

却下。
stdout 上で inline `.ssd` と generated `.ssm` をどう共存させるかという別 slice の contract が必要になる。

### no-provider stdout を raw `.txt` の single-input に据え置く

却下。
existing `.ssd` と multi-input aggregation path の両方が不要に取り残される。

### single existing `.ssd` no-provider stdout だけ先に追加する

却下。
multi-input aggregation と同じ canonical inline contractへ自然に接続できず、後続 slice でもう一度 stdout surface を開き直すことになる。

## Validation

- requirements.md と requirements.llmthink.dsl を no-provider stdout 1+ input aggregate slice に同期する
- docs/cli.ebnf と extract help / root help を no-provider stdout aggregation と embedding-enabled single-input stdout の境界に更新する
- success manifest に single standalone `.ssd` stdout、single paired `.ssd` stdout、multi-input existing `.ssd` stdout、mixed `.ssd` + `.txt` stdout を追加する
- failure manifest で embedding-enabled multi-input stdout rejection を維持する
- CLI 実装で no-provider stdout branch を aggregate path へ接続し、runner を通す

## Related Artifacts

- docs/adr/0023-add-multi-input-extract-out-aggregation.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/extract-help.stdout
- docs/examples/golden/extract-help.semdl.stdout
- docs/examples/golden/help-root.stdout
- docs/examples/golden/help-root.semdl.stdout
- docs/examples/golden/help-root-alias.semdl.stdout