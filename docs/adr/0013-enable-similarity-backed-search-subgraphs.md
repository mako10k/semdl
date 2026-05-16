# 0013 Enable Similarity-Backed Search Subgraphs

- Status: Accepted
- Date: 2026-05-16

## Context

ADR 0010 では `ssd search` の最初の `return: subgraph` を structural query に限定し、
`similar + return: subgraph` は明示未実装として残した。

一方で実装側では、`similar` の anchor 解決、score 付き match 順序、
matched node からの outbound one-hop context expansion は既に別々に成立している。
未実装のまま残している主要な差分は、grouped stdout contract を similarity 付きでも固定することだった。

この状態を続けると、

- `return: matches` と `return: subgraph` で similarity metadata の出力契約が分岐したままになる
- grouped structural context を必要とする similarity query を failure でしか扱えない
- 既存の subgraph 形を再利用できるのに、help と requirements が古い制約を残し続ける

という問題がある。

## Decision

### 1. supported combination

`ssd search` は `similar` と `return: subgraph` の組み合わせを受け付ける。

- anchor / candidate 解決は existing similarity-backed `return: matches` と同じ統合ビュー規則を使う
- anchor 自身は結果から除外する
- result group の順序は score 降順と既存 stable tie-break に従う

### 2. group shape

similarity-backed subgraph result も、ADR 0010 と同じく match ごとの grouped result とする。

各 group は少なくとも以下を持つ。

- matched node の `file`
- matched node の `id`
- matched node の `kind`
- matched node の `score`
- context node list

context expansion の範囲と順序は ADR 0010 の outbound one-hop 規則をそのまま使う。

### 3. stdout contract

`similar + return: subgraph` の stdout には、group list に加えて similarity metadata を top-level に出す。

- `query_file`
- `mode: subgraph`
- `inputs`
- `anchor`
- `metric`
- `model`
- `dimensions`
- `subgraphs`

各 group には `match_file`、`match_id`、`match_kind`、`score`、`context_nodes` を出す。
各 context node は `file`、`id`、`kind` を持つ。

`metric`、`model`、`dimensions` は similarity-backed result では常に出力する。

## Consequences

- grouped subgraph contract を similarity query まで一貫して拡張できる
- `return: matches` と `return: subgraph` の双方で similarity metadata の対称性を保てる
- structural-only failure case を撤去し、search help と root help の drift を解消できる

## Alternatives Considered

### `similar + return: subgraph` を引き続き failure のままにする

却下。
既存実装の再利用余地が大きく、未実装のままにする理由が薄い。

### similarity-backed subgraph では `model` を省略可能にする

却下。
既存 `return: matches` と `ssd similarity` の result contract が `metric`、`model`、`dimensions` を持っており、
subgraph だけ省略可能にすると再現性と対称性が落ちる。

## Validation

- requirements.md と requirements.llmthink.dsl を similarity-backed grouped subgraph に更新する
- search help と root help から未実装表現を外す
- failure manifest の unsupported case を success manifest へ移す
- golden stdout に top-level similarity metadata と per-group score を固定する
- `ssd search` 実装を更新し、runner で受け入れを通す

## Related Artifacts

- docs/adr/0010-define-initial-search-subgraph-result-shape.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/search-help.stdout
- docs/examples/golden/search-help.semdl.stdout