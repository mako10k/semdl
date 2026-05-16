# 0024 Add Unary Not To .ssq Where

- Status: Accepted
- Date: 2026-05-16

## Context

ADR 0022 で `.ssq` の `where` は mixed `and` / `or` precedence と parenthesized grouping まで広がったが、unary `not` は明示的に deferred のまま残っている。
そのため、sample-backed query でも次の不足が残る。

- 単純な否定条件を range や equality の組み合わせへ展開しないと書けない
- grouped expression 全体の否定を 1 本の `where` で自然に書けない
- current expression tree は clause / conjunction / disjunction に限られ、既存 boolean slice の直近延長が未完了である

一方で function call、new predicate、line-spanning expression grammar まで同時に導入すると、この slice の境界が再び広がりすぎる。

## Decision

### 1. unary `not` is now allowed

`.ssq` の `where` は unary `not` を受け付けてよい。

- `not` は filter term または parenthesized group の前に置いてよい
- repeated unary `not` を受け付けてよい
- `not` は `and` より高く、grouping より低い通常の unary precedence とする

### 2. leaf term kinds stay unchanged

`not` が掛かる leaf term の種類はこの slice でも増やさない。

- presence check
- scalar equality check
- numeric range comparison

`=` の lexical equality semantics、range rhs の number 制約、missing / nonnumeric lhs が range で no-match になることは既存のまま維持する。

### 3. validation stays explicit

`not` は operand を必須とする。

- dangling `not` は validation failure とする
- `not )` や `not` の直後に expression が欠ける形は validation failure とする
- quoted string 中の `not` は keyword として解釈しない
- current `where` operand position では `not` を reserved keyword とし、field name `not` はこの profile では受け付けない

### 4. explicit deferrals remain

この slice では以下を導入しない。

- function call
- regex / contains のような新 predicate
- line-spanning `where` grammar の一般化

## Consequences

- `.ssq` の `where` は sample-backed な否定条件を自然に書ける
- parser / validator / evaluator は existing boolean expression tree に unary negation node を 1 段追加する
- help、grammar、acceptance は unary precedence と dangling `not` failure を同じ境界で固定する必要がある
- function call と新 predicate は別 slice として残る

## Alternatives Considered

### function call と同時に導入する

却下。
関数名空間、引数、戻り値、evaluation failure surface まで同時に固定する必要があり、slice が大きすぎる。

### `not` を parenthesized group にだけ限定する

却下。
`not field` のような自然な unary form を禁止すると、最小の否定条件でも不自然な括弧が必要になる。

### repeated `not` を禁止する

却下。
parser / evaluator の実装を複雑化させずに unary recursion で自然に扱えるため、制限する理由が薄い。

## Validation

- requirements.md と requirements.llmthink.dsl を unary `not` slice に同期する
- docs/ssq.ebnf を unary precedence-aware grammar へ更新する
- search help と root help の capability summary を更新する
- success manifest に unary `not` success、grouped `not` success、double `not` success を追加する
- failure manifest に dangling `not` failure を追加する
- core query validator / evaluator を unary negation node 対応に更新し、runner を通す

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