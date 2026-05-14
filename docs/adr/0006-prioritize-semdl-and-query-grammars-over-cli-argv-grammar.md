# 0006 Prioritize SEMDL And Query Grammars Over CLI Argv Grammar

- Status: Accepted
- Date: 2026-05-14

## Context

初期仕様では、CLI help を正本にし、CLI 引数仕様に整合する EBNF も formal notation artifact として維持してきた。

しかし grammar の優先順位として見ると、利用者や将来実装が本当に強く必要とするのは
SEMDL 本体 `.ssd` と、検索言語 `.ssq` および将来の semql 系である。

CLI の argv 形は運用上必要だが、SEMDL 自体の意味構造や query layer の grammar より上位の
規範対象として扱うと、help と仕様の重心が操作断片へ寄りすぎる。

今回の要求では、grammar の主対象を CLI ではなく SEMDL / query language 側へ寄せる必要がある。

## Decision

### 1. primary grammar concerns

SEMDL の primary grammar concern は次とする。

- `.ssd` の記述 grammar
- `.ssq` の query grammar
- 将来導入する場合の semql family grammar

### 2. CLI argv grammar role

CLI argv grammar は secondary operational artifact とする。

- `ssd` の help と subcommand help は user-facing operational guidance を提供する
- `docs/cli.ebnf` は CLI argv の整合確認や test 資産の補助 artifact として残してよい
- ただし CLI argv grammar を、SEMDL や query language 自体より上位の grammar 対象として扱わない

### 3. help grammar topic role

`ssd help grammar` は language grammar の正本ではなく、CLI を使うための operational syntax summary とする。

- help entrypoint の形
- topic targeting の境界
- selector / option / value form の操作上必要な要約
- command-specific usage への導線

これにより detailed command usage は reference help で補い、language grammar work は `.ssd` と `.ssq` / semql 側へ寄せる。

### 4. impact on existing ADRs

この ADR は 0004 と 0005 の help surface / rendering decision 自体は維持する。
ただし、grammar の重心を CLI 側へ寄せる読み方はこの ADR で否定し、CLI help は operational guide として扱う。

## Consequences

- grammar work の優先順位が `.ssd` と query language 側へ移る
- CLI help は引き続き user-facing entrypoint だが、language grammar の主戦場ではなくなる
- `docs/cli.ebnf` は残せるが、secondary artifact として扱う必要がある
- requirements、help wording、plans の優先順位表現を揃える必要がある

## Alternatives Considered

### CLI argv grammar を primary concern のまま維持する

却下。
操作の説明は必要だが、SEMDL 自体や query language より重く扱うと仕様の焦点がずれる。

### CLI argv grammar を完全に削除する

却下。
運用 guidance と test 資産の補助 artifact としては有用であり、完全削除までは不要である。

## Validation

- requirements.md で grammar の優先順位と CLI help の役割を更新する
- requirements.llmthink.dsl を同じ判断に揃える
- docs/cli.ebnf を secondary operational artifact として位置付ける
- help wording と related plans の grammar 重心を揃える

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/adr/0004-structure-cli-help-and-entrypoints.md
- docs/adr/0005-add-opt-in-semdl-help-rendering.md
