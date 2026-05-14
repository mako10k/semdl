# 0003 Establish Initial Implementation Layout

- Status: Accepted
- Date: 2026-05-14

## Context

ADR 0002 で core library、ssd CLI、test runner の 3 層分離は決まったが、
repository 内でどのような配置と依存方向を採るかはまだ固定されていない。

この段階で配置先を曖昧にしたまま実装へ入ると、core library と CLI のコードが混在し、
test runner も内部 API に寄りかかる構成になりやすい。

また、最初の implementation change plan をどの入口から起こすかを固定しておかないと、
新しく追加した prompt / skill が実務で使われず、運用が形骸化しやすい。

## Decision

初期実装の repository layout は少なくとも次を満たす。

### 1. core library

- 配置先: `src/core/`
- 役割: parse、validate、selector resolve、inline / sidecar 統合 view

### 2. ssd CLI

- 配置先: `src/cli/`
- 役割: 引数解釈、ファイル I/O、exit code、エラー整形、core library 呼び出し

### 3. test runner

- 配置先: `src/runner/`
- 役割: manifest discovery、argv 実行、stdout / stderr / exit code 比較

### 4. tests and fixtures

- golden fixtures と manifest は既存の `docs/examples/` を正本として維持する
- 実装テストはこれらの docs 資産を参照し、別の期待値を複製しない

依存方向は次を守る。

- `src/core/` は `src/cli/` と `src/runner/` に依存しない
- `src/cli/` は `src/core/` を利用してよい
- `src/runner/` は `src/core/` を直接呼ばず、CLI を外部プロセスとして扱う

## Consequences

- 早い段階でディレクトリ境界と依存方向を固定できる
- docs の golden assets を実装の正本として再利用しやすい
- runner が CLI 契約の回帰確認として機能しやすい
- 初期段階では `src/runner/` が薄く見えるが、契約分離を保つために独立配置を維持する必要がある

## Alternatives Considered

### `src/` 配下に全実装をまとめ、後から分割する

却下。
初期速度は出るが、parse、CLI、runner の責務が混ざりやすく、後から分離するコストが高い。

### runner を `tests/` 側の補助コードとして置く

却下。
runner contract は公開された実行契約であり、単なる内部テスト補助として扱うと責務が薄まる。

## Validation

- requirements.md に初期実装骨格と plan 文書の参照を追加する
- requirements.llmthink.dsl に対応判断を追加する
- 最初の implementation change plan はこの ADR の配置前提を使って作る

## Related Artifacts

- docs/adr/0002-separate-library-cli-and-runner.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/test-runner-format.md
- docs/test-runner-contract.md