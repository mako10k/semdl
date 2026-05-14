# 0009 Add Ollama-Backed Embedding Generation To Extract

- Status: Accepted
- Date: 2026-05-14

## Context

ADR 0008 では、埋め込み生成を explicit write path の責務に置き、
`search` / `similarity` / `.ssq` evaluation では暗黙生成しないことだけを固定した。

一方で、最初にどの command が埋め込み生成を担うか、
どの provider contract を最初の受け入れ面に置くか、
どの kind / field を埋め込み入力に使うかは未固定だった。

このまま実装を始めると、

- dedicated command を早期に増やす
- raw input からの semantic extraction と embedding generation を同時に固定する
- read-only path に provider 依存を漏らす

のいずれかに寄りやすい。

## Decision

### 1. first owner command

最初の埋め込み生成 write path は `ssd extract` に置く。

ただし初期 slice の `extract` は raw input extraction 全体を解かず、
既存 `.ssd` を semantic source として受け取り、
paired `.ssm` に埋め込みメタデータを生成する enrichment-oriented extract に限定する。

### 2. provider contract

初期 provider は local Ollama に限定する。

- public option は `--embed-provider ollama` と `--embed-model <model>` を持つ
- `--embed-provider` を指定した場合は `--embed-model` を必須とする
- `--embed-model` 単独指定も不正とし、両方がそろったときだけ埋め込み生成を有効化する
- provider 未対応や command 実行失敗は extract failure として返す

実装は既定で `ollama` command を使うが、
受け入れテストでは environment override により deterministic fixture command へ差し替えてよい。

### 3. embeddable targets and source field

初期 slice の embeddable text は kind ごとに canonical field を 1 つだけ持つ。

- `segment.text_quote`
- `assertion.object`
- `hypothesis.summary`
- `alternative.label`
- `resource.label`

対応 field を持たない target は skip し、failure にしない。

### 4. persistence shape

初期 slice の generated embedding record は `.ssm` の `meta <id> { embedding { ... } }` に inline で保存する。

最低限以下を保持する。

- `model`
- `dimensions`
- `generated_at`
- `provider`
- `source_field`
- `vector`

`.ssd` 本体には `embedding_presence: true` を反映してよい。

### 5. initial CLI surface

最初の成功面は以下に限定する。

- `ssd extract --stdout --embed-provider ollama --embed-model <model> <input.ssd>`
  - generated `.ssm` profile を stdout に出す
- `ssd extract --out <output.ssd> --embed-provider ollama --embed-model <model> <input.ssd>`
  - `.ssd` 本体を出力先へ書き、paired `.ssm` を生成する

複数 input、raw text extraction、provider auto-selection、search-time auto-embedding は後続 slice とする。

## Consequences

- ADR 0008 の read-only / write-path 境界を保ったまま、最初の実行可能な embedding generation path を得られる
- raw extraction semantics を未固定のままでも、既存 semantic source に対する埋め込み生成を先行実装できる
- sidecar に vector を置くため、本体可読性は保てるが `.ssm` は大きくなりうる
- vector externalization は後続 slice で `vector_ref` へ移行または併用判断できる

## Alternatives Considered

### dedicated embedding command を先に新設する

却下。
command family を広げる設計判断が先に必要になり、初回 slice としては重い。

### raw input からの semantic extraction と embedding generation を同時に固定する

却下。
semantic extraction の判断軸と provider / persistence の判断軸が混ざる。

### vector を常に external file に書く

却下。
最初の成功面に追加の file layout contract が増え、受け入れ面が重くなる。

## Validation

- requirements.md に extract-time Ollama generation の boundary を追記する
- requirements.llmthink.dsl を同じ判断へ揃える
- docs/cli.ebnf の extract surface を更新する
- extract help と acceptance case に Ollama option surface を追加する
- success / failure E2E を fixture Ollama command で固定する

## Related Artifacts

- docs/adr/0008-decide-embedding-persistence-and-similarity-resolution.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json