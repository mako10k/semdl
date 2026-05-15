# 0010 Define Initial Search Subgraph Result Shape

- Status: Accepted
- Date: 2026-05-15

## Context

`.ssq` grammar は当初から `return: subgraph` を許容していたが、
`ssd search` の初期成功面は `return: matches` に限定されており、
subgraph の公開結果契約は未固定だった。

この状態のまま subgraph を実装すると、

- match ごとの部分グラフなのか、全体 union なのかが曖昧になる
- context node の範囲が kind ごとにぶれやすい
- `similar` と `subgraph` の組み合わせまで同時に固定され、slice が広がる

という問題がある。

## Decision

### 1. initial scope

最初の `return: subgraph` は structural query に限定する。

- `select` と optional single `where` を使う通常検索でサポートする
- `similar` と `return: subgraph` の組み合わせは、この slice では未実装として明示 failure にする

### 2. result grouping

subgraph result は global union ではなく、match ごとの grouped result とする。

各 result group は少なくとも以下を持つ。

- matched node の file
- matched node の id
- matched node の kind
- context node list

### 3. neighborhood rule

初期 slice の context node は matched node から outbound に 1 hop で参照される structural node に限定する。

- `segment`
  - `source` が指す `resource`
- `assertion`
  - `subject` が指す `resource`
  - `source_segment` が指す `segment`
- `hypothesis`
  - `about` が指す `assertion`
- `resource`、`alternative`、`document`
  - 追加の context expansion を行わない

同一 id が重複しても context list 内では 1 回だけ出す。

### 4. ordering

result group の順序は match 解決順に従う。
初期 structural search では file path 昇順、id 昇順の既存安定順を使う。

context node の順序は kind ごとの outbound field 順を source of truth とする。

- assertion: `subject`、`source_segment`
- segment: `source`
- hypothesis: `about`

### 5. stdout contract

`ssd search` の `return: subgraph` 出力は少なくとも以下を持つ。

- `query_file`
- `mode: subgraph`
- `inputs`
- `subgraphs`
- 各 group の `match_file`、`match_id`、`match_kind`
- 各 group の `context_nodes`
- 各 context node の `file`、`id`、`kind`

`role` のような追加ラベルは初期 slice に含めない。

## Consequences

- `return: subgraph` を `matches` と区別された公開契約として受け入れ固定できる
- neighborhood を pure outbound one-hop に絞るため、過剰な graph expansion を避けられる
- `similar + subgraph` は明示未実装として残るため、埋め込み検索の結果 grouping を急いで固定せずに済む

## Alternatives Considered

### global union subgraph を返す

却下。
複数 match の context 所属が見えにくくなり、existing match-mode の result-per-match 形とも非対称になる。

### hypothesis から alternative_group の alternatives まで広げる

却下。
direct reference ではなく group expansion になり、initial boundary として広すぎる。

### `similar + subgraph` も同時に実装する

却下。
structural neighborhood と similarity ranking の組み合わせ contract が増え、初期 slice を不必要に広げる。

## Validation

- requirements.md に structural-only subgraph と grouped result shape を追記する
- requirements.llmthink.dsl を同じ判断へそろえる
- search help と root help を更新する
- success case と unsupported-combination failure case を manifest と golden へ追加する
- `ssd search` 実装を更新し、runner で受け入れを通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/search-help.stdout
- docs/examples/golden/search-help.semdl.stdout