# 0020 Expand .ssq Where Filters With Range And Homogeneous Logic

- Status: Accepted
- Date: 2026-05-16

## Context

ADR 0007 と requirements の初期 `.ssq` profile では、`where` を presence check と単一 equality check に限定していた。
その境界は query layer の最初の固定には有効だったが、現状では次の不足が残っている。

- sidecar 数値 field を使った threshold filtering を query 側で表現できない
- 複数の sample-backed 条件を `and` / `or` で結合できず、実用 query が不自然に分解される
- parser / validator / evaluator は単一 `where` string をすでに共有しており、flat composition と numeric comparison を追加する局所性が高い

一方で、括弧や function call まで同時に導入すると precedence と evaluation 契約が広がりすぎる。

## Decision

### 1. range comparisons

`.ssq` の `where` は numeric range comparison を受け付けてよい。

- supported operator は `>`, `>=`, `<`, `<=`
- range comparison の右辺は number に限定する
- 左辺 field が欠損または非数値の candidate は failure ではなく no-match とする

### 2. homogeneous logical chains

`.ssq` の `where` は flat な homogeneous logical chain を受け付けてよい。

- `and` のみで連結された chain は受理する
- `or` のみで連結された chain は受理する
- mixed `and` / `or` はこの slice では failure とする
- parentheses はこの slice では導入しない

### 3. equality semantics stay stable

既存 `=` は token equality のまま維持する。

- quoted string、number、boolean は既存どおり lexical equality として扱う
- 数値右辺の `=` を range comparison と同じ数値比較へ暗黙昇格しない

### 4. grammar boundary

primary `.ssq` grammar artifact は homogeneous chain を文法として表現する。

- `filter-expr` は single term、and-chain、or-chain を受け付ける
- chain の各 term は presence、equality、range comparison のいずれかに限定する
- mixed precedence を validator の事後規則へ押し込まず、grammar と validator の両方で拒否する

## Consequences

- merged inline / sidecar view に対する threshold filtering を read-only query layer のまま追加できる
- simple multi-condition search を full expression grammar なしで前進できる
- mixed precedence、parentheses、function call は引き続き別 ADR / slice へ分離される
- `=` と range operator の意味差は残るが、既存 query の互換性を保てる

## Alternatives Considered

### full boolean expression grammar を同時に導入する

却下。
precedence、parentheses、error reporting、future semql alignment を同時に固定する必要があり、sample-backed slice として大きすぎる。

### 数値右辺の `=` も数値比較に揃える

却下。
既存 equality の token semantics を変えると互換性と説明コストが増える。

### 非数値 field に対する range comparison を hard failure にする

却下。
merged view 上の heterogeneous field presence で query 全体が不安定になり、read-only filter としては no-match の方が自然である。

## Validation

- requirements.md と requirements.llmthink.dsl を filter slice に同期する
- docs/ssq.ebnf を homogeneous chain と range comparison に更新する
- search help と root help の capability summary を更新する
- success manifest に range / logical success cases と non-numeric no-match case を追加する
- failure manifest に mixed logical operator と invalid range rhs case を追加する
- core query validator / evaluator を更新し、runner で受け入れを通す

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
