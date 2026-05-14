# 0007 Introduce Primary Grammar Artifacts For SSD And SSQ

- Status: Accepted
- Date: 2026-05-14

## Context

0006 により、grammar の主対象は CLI argv ではなく `.ssd` と `.ssq`、および将来の semql family へ寄せると決めた。

しかし現状 repo で concrete な notation artifact として存在するのは `docs/cli.ebnf` だけであり、
primary concern とした `.ssd` / `.ssq` 側には対応する grammar artifact がない。

`.ssd` には `docs/examples/minimal.ssd` などの concrete sample があるため、
初期 grammar artifact を直接起こせる。

一方 `.ssq` は requirements 上の責務と能力はあるが、workspace に concrete sample がなく、
完全な query syntax をここで広く発明するのは過剰である。

## Decision

### 1. primary grammar artifacts

以下の artifact を導入する。

- `docs/ssd.ebnf`
- `docs/ssq.ebnf`

### 2. `.ssd` artifact scope

`docs/ssd.ebnf` は、初期仕様で受け入れ面に現れている `.ssd` / `.ssm` の concrete surface を formalize する。

- block-oriented 構造
- document / resource / segment / assertion / hypothesis / alternative
- sidecar の document_meta / meta / embedding block
- scalar 値、identifier、quoted-string、boolean、number の基本字句

### 3. `.ssq` artifact scope

`docs/ssq.ebnf` は、初期仕様の query responsibility を formalize するための starting artifact とする。

- 読み取り専用であること
- query block を 1 つ持つこと
- selector / filter / similarity condition のための最小 slot を持つこと
- result mode を示せること

初期 filter profile は、sample-backed な最小 contract に限定する。

- presence check
- scalar equality check

ここでの scalar equality の右辺は、quoted string、number、boolean に限定する。

range 条件、論理結合、比較演算子、関数呼び出しは、この ADR の範囲では固定しない。

ただし、workspace に concrete `.ssq` sample がまだないため、この初期 artifact は
requirements 上の能力境界を固定する minimal profile として扱う。
高度な query composition や semql 互換までをここで決めない。

### 4. relation to help and CLI

`ssd help` は引き続き operational guidance の正本であり、language grammar の主戦場ではない。

`.ssd` / `.ssq` grammar artifact は requirements と aligned である必要があるが、
CLI help にそれらの完全内容を複製することは要求しない。

## Consequences

- grammar の primary concern と concrete artifact が一致する
- `.ssd` は existing sample から比較的低リスクに formalize できる
- `.ssq` は minimal profile から始めるため、今後 sample と acceptance を追加しながら拡張する必要がある
- `docs/cli.ebnf` は secondary operational artifact として残る

## Alternatives Considered

### `.ssd` / `.ssq` grammar artifact を後回しにして requirements のみで運用する

却下。
0006 の priority shift を concrete artifact に落とせず、CLI EBNF だけが実体として残ってしまう。

### `.ssq` syntax をこの場で fully-featured に決める

却下。
sample や acceptance の裏付けなしに query syntax を広く固定すると、過剰な憶測になる。

## Validation

- requirements.md と requirements.llmthink.dsl に `.ssd` / `.ssq` grammar artifact を追記する
- `docs/ssd.ebnf` と `docs/ssq.ebnf` を追加する
- concrete sample が欠ける `.ssq` には最小 sample を追加し、artifact の読み方を anchor する
- `.ssq` の filter profile を sample-backed な presence / equality に限定する
- semantics review で 0006 と矛盾しないことを確認する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/ssd.ebnf
- docs/ssq.ebnf
- docs/examples/minimal.ssd
- docs/examples/minimal.ssm
- docs/query/example.ssq
- docs/query/equality-filter.ssq
- docs/query/string-equality-filter.ssq
- docs/query/number-equality-filter.ssq
