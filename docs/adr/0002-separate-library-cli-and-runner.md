# 0002 Separate Library, CLI, and Runner

- Status: Accepted
- Date: 2026-05-14

## Context

SEMDL は現在、requirements、EBNF、manifest、golden files、runner contract までが先行して固定されている。
この状態で実装フェーズへ入ると、次の責務が一つの実装に混ざりやすい。

- .ssd / .ssm の意味解釈
- CLI 引数解釈とエラー整形
- golden test 実行と比較

requirements.md では、ライブラリ部と CLI 部を分離すること、runner 契約を別文書で固定することまでは決まっているが、
初期実装でどこまでを別層として扱うかはまだ明文化されていない。

## Decision

初期実装では、少なくとも次の 3 層を分離する。

### 1. core library

- .ssd / .ssm の parse
- validate
- selector 解決
- inline / sidecar の統合 view
- merge / split / normalize の意味処理の中核

この層は CLI 引数形式、stdout / stderr 整形、golden 比較に依存しない。

### 2. ssd CLI

- 引数解釈
- 入力ファイル解決
- exit code 方針
- エラーメッセージ整形
- 実ファイル更新

この層は意味解釈を自前で持たず、core library に委譲する。

### 3. test runner

- manifest discovery
- argv によるプロセス起動
- stdout / stderr / exit code の比較
- success / failure case の実行管理

この層は CLI の外側に位置し、初期段階では core library API に直接依存しない。

## Consequences

- core library を将来の組み込み利用へ流用しやすくなる
- CLI 公開契約と library 内部 API を切り分けやすくなる
- golden test は CLI 契約の回帰として維持しやすくなる
- 初期実装時に層間インターフェースを意識する必要があり、短期的には少し手数が増える

## Alternatives Considered

### CLI に parser と validator と更新処理をまとめ、runner も内部 API を呼ぶ

却下。
実装開始は早いが、CLI 公開契約と内部 API の境界が曖昧になり、golden test が何を保証しているかが崩れやすい。

### core library と CLI だけ分け、runner は library API を直接呼ぶ

却下。
runner 契約が CLI 契約ではなく library API に引きずられ、manifest と golden が外部仕様として機能しにくくなる。

## Validation

- requirements.md に 3 層の責務を追記する
- requirements.llmthink.dsl に対応判断を追記する
- 今後の実装計画では、module 分割をこの ADR に従って説明する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/test-runner-format.md
- docs/test-runner-contract.md