# 0004 Structure CLI Help and Entry Points

- Status: Accepted
- Date: 2026-05-14

## Context

SEMDL の CLI は check、explain、split、merge、更新系コマンドの表面は見え始めているが、
利用者がどこから仕様へ到達すべきかはまだ固定されていない。

このままでは、usage 断片、EBNF、golden examples、requirements の関係が利用者に見えず、
unknown option、missing argument、未実装 subcommand、explain 失敗時の導線もばらけやすい。

今回の要求では、help を先に実装し、少なくとも次を構造的に配置することが求められている。

- 概要
- 目次
- 文法
- リファレンス
- 逆引きリファレンス
- サンプル
- 注意点、既知バグ、報告先

また、error から help への導線も public CLI contract として固定する必要がある。

## Decision

SEMDL の help 正本は CLI とする。

### 1. root help

- `ssd help`
- `ssd --help`

これらは同じ root help を表示し、少なくとも次の 7 節をこの順に含む。

1. Overview
2. Table of Contents
3. Grammar
4. Reference
5. Reverse Lookup
6. Samples
7. Cautions, Known Bugs, Reporting

### 2. topic help

`ssd help <topic>` をサポートし、初期 topic は少なくとも次を持つ。

- `overview`
- `toc`
- `grammar`
- `reference`
- `recipes`
- `samples`
- `troubleshooting`

`reference` と `recipes` は追加の対象指定を許容してよい。

- `ssd help reference check`
- `ssd help recipes wrong-layer`

### 3. subcommand help

少なくとも `ssd check --help`、`ssd explain --help`、`ssd set --help`、`ssd annotate --help` は、
対応する reference topic へ到達できる short help を表示する。

### 4. error guidance

少なくとも次の入口は help への案内を含む。

- unknown option
- missing required argument
- unknown help topic
- unimplemented subcommand
- explain target not found

error message は完全な全文を root help に複製せず、該当 topic への導線を出す。

### 5. related artifacts

help は CLI の user-facing operational guidance に関する正本であり、
CLI を使うための syntax summary と導線の説明責務を CLI 側に置く。
ただし `.ssd` や `.ssq` / semql family 自体の primary grammar work は 0006 に従う。
次の資産は aligned artifact として保持してよいが、help から利用者をそれらの参照へ導いてはならない。

- `docs/cli.ebnf`
- `docs/examples/testcases/*.json`
- `docs/examples/golden/*`
- `docs/requirements.md`

## Consequences

- 利用者は `ssd help` を入口として CLI 契約全体へ到達しやすくなる
- parser/lexer failure と docs の関係が CLI から辿れる
- error message が単独断片ではなく、次の行き先を持つようになる
- root help は長くなるため、topic help と subcommand help を併設して分割閲覧を支える必要がある

## Alternatives Considered

### docs を正本にして CLI help は短い usage のみに留める

却下。
repo は spec-first だが、利用者の直接入口は CLI であり、error からの導線要件にも合いにくい。

### root help を短くし、7 節を docs にのみ置く

却下。
今回の要求は help 自体に情報設計を持たせることであり、単なる docs 参照一覧では不足する。

## Validation

- requirements.md に help 構造と導線要件を追記する
- requirements.llmthink.dsl に対応 decision を追加する
- docs/cli.ebnf に help command と `--help` 導線を追記する
- manifest と golden に help success/failure cases を追加する
- CLI 実装で `ssd help`、`ssd --help`、subcommand `--help`、error guidance を実装する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json