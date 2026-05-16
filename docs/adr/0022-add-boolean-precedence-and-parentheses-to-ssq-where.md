# 0022 Add Boolean Precedence And Parentheses To .ssq Where

- Status: Accepted
- Date: 2026-05-16

## Context

ADR 0020 で `.ssq` の `where` は numeric range comparison と homogeneous logical chain まで広がったが、mixed `and` / `or` と parenthesized grouping は依然として別 slice に分離されている。
そのため、実用 query では次の不足が残る。

- mixed precedence を使う自然な filter を 1 本の `where` に書けない
- grouping を明示できず、sample-backed query が不自然に分割される
- current parser は logical keyword の flat split に依存しており、`and` / `or` の優先順位を構文として表せない

一方で `not`、function call、comparison 以外の predicate、multi-line expression grammar まで同時に導入すると、slice が大きくなりすぎる。

## Decision

### 1. mixed boolean expressions are now allowed

`.ssq` の `where` は mixed `and` / `or` expression を受け付けてよい。

- `and` と `or` を同一 expression に含めてよい
- precedence は `and` が `or` より高い

### 2. parenthesized grouping is allowed

`where` は `(` と `)` による grouping を受け付けてよい。

- parentheses は precedence override に使ってよい
- nested parentheses を受け付けてよい
- unmatched parenthesis や empty grouping は validation failure とする

### 3. filter terms stay unchanged

leaf term の種類はこの slice では広げない。

- presence check
- scalar equality check
- numeric range comparison

`=` の lexical equality semantics、range rhs が number に限定されること、missing / nonnumeric lhs が range で no-match になることは既存のまま維持する。

### 4. explicit deferrals remain

この slice では以下を導入しない。

- unary `not`
- function call
- regex / contains のような新 predicate
- line-spanning `where` grammar の一般化

## Consequences

- `.ssq` の `where` は sample-backed 実用 filter をより自然に表現できる
- parser / validator / evaluator は flat clause list から boolean expression tree へ進化する
- help、grammar、acceptance は precedence と grouping を同じ境界で固定する必要がある
- `not` と function call は別 slice として残る

## Alternatives Considered

### homogeneous chain のまま据え置く

却下。
mixed precedence と grouping を欠いたままだと query の表現力が不自然なまま止まる。

### full expression language を一度に導入する

却下。
`not`、function call、new predicate、error surface まで同時に固定すると過大になる。

### precedence を持たず parentheses 必須にする

却下。
simple mixed expression まで括弧必須にすると usability が悪く、一般的な boolean expectation から外れる。

## Validation

- requirements.md と requirements.llmthink.dsl を boolean `where` slice に同期する
- docs/ssq.ebnf を precedence-aware grammar へ更新する
- search help と root help の capability summary を更新する
- success manifest に mixed precedence success、parenthesized precedence override success、quoted parenthesis shielding case を追加する
- failure manifest に unmatched parenthesis、empty grouping、group adjacency / missing operator failure を追加する
- core query validator / evaluator を boolean expression tree へ更新し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/ssq.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/search-help.stdout
- docs/examples/golden/search-help.semdl.stdout
- docs/examples/golden/help-root.stdout
- docs/examples/golden/help-root.semdl.stdout
- docs/examples/golden/help-root-alias.semdl.stdout