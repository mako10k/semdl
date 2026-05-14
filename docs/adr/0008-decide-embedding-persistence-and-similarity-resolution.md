# 0008 Decide Embedding Persistence And Similarity Resolution

- Status: Accepted
- Date: 2026-05-14

## Context

SEMDL は埋め込みベクトルを類似度計算や意味検索に使うことを要求しているが、
初期仕様では以下が未固定だった。

- 埋め込みをいつ計算するか
- ベクトル本体と関連メタデータをどこへ保存するか
- `ssd similarity` と `.ssq` の `similar:` がどの入力を基準に解決するか

この境界を曖昧にしたまま `search` や `similarity` を実装すると、
read-only query path で暗黙に埋め込み生成や永続化が走り、`.ssq` の参照専用責務と衝突しやすい。

また、埋め込みは可読性を損なう大きな補助データであり、`.ssd` 本体へ常時保持する前提にすると
サイドカープロファイルの意味が薄れる。

## Decision

### 1. embedding generation timing

埋め込み生成は read path ではなく explicit write path の責務とする。

- `ssd search`
- `ssd similarity`
- `.ssq` の query evaluation

は、既存の埋め込みを読み取って使うだけに留め、暗黙の再計算や自動永続化を行わない。

初期仕様で埋め込みを生成できる主体は、少なくとも以下のいずれかとする。

- `ssd extract` の明示オプション付き出力
- 将来の dedicated embedding / enrich / derive 系 write command
- 外部前処理によって生成され、`.ssm` または外部参照に書き込まれた結果

### 2. persistence boundary

埋め込みの persist 境界は以下とする。

- `.ssd` 本体には `embedding_presence` のような存在可視化だけを残してよい
- ベクトル本体と詳細メタデータは `.ssm` か外部参照へ置く
- merge / distribution profile では必要に応じて inline 化してよいが、既定の編集・検索前提は sidecar / external reference とする

埋め込みレコードは少なくとも以下を保持できること。

- 対象 id
- model
- dimensions
- generated_at
- generator または provider
- vector body または vector_ref

検索結果順位、再計算可能な近傍一覧、一時 index は derived data とし、persist 必須にはしない。

### 3. similarity resolution

pairwise similarity と search-time similarity は分けて扱う。

- `ssd similarity <target1> <target2>`
  - 既存の 2 target を比較する pairwise command
  - 両 target に既存埋め込みが必要
  - model と dimensions が一致しない場合は既定で失敗する
- `.ssq` の `similar:`
  - 既存 target を基準に近傍候補を探す read-only query slot
  - free-text query や暗黙再埋め込みは初期仕様に含めない
  - 候補は同一 model / dimensions の既存埋め込みから解決する

埋め込みが存在しない target は、初期仕様では暗黙補完せず、pairwise similarity では failure、search-time similarity では failure または no-match を返すポリシーを明示する方向で後続 slice に固定する。

### 4. model discipline

初期仕様の類似度比較は cross-model を既定で許可しない。

- 同じ model
- 同じ dimensions
- 明示された similarity metric

の組だけを比較対象とする。

similarity metric の source of truth は embedding persistence record ではなく execution policy とする。
初期仕様では cosine を既定値としてよいが、実行結果には使用した metric を必ず明示する。

model 正規化や cross-model alignment は後続 slice とする。

## Consequences

- read-only query path が副作用を持たず、`.ssq` の責務と整合する
- `.ssd` 本体の可読性を維持しつつ、embedding existence は可視化できる
- 初期 similarity は precomputed embedding 前提になるため、運用では先に embedding generation path を用意する必要がある
- free-text semantic search や cross-model search は別 slice で sample-backed に決める必要がある

## Alternatives Considered

### search / similarity 実行時に埋め込みを自動生成してそのまま使う

却下。
read-only query path に副作用が混入し、provider 設定、保存先、再現性、キャッシュ失効の責務が曖昧になる。

### 埋め込み本体を常に `.ssd` へ inline で保持する

却下。
可読性を損ない、既存の sidecar 方針と相性が悪い。

### `.ssq` の `similar:` を free-text query と既存 target query の両方で許可する

却下。
query slot の意味が広がりすぎ、text-to-vector 生成責務まで同時に固定する必要が生じる。

## Validation

- requirements.md に embedding generation timing、persist 境界、similarity resolution を追記する
- requirements.llmthink.dsl を同じ判断に揃える
- existing `.ssm` sample に embedding persistence の anchor field を足す
- `.ssq` sample に target-based `similar:` を置き、result mode と合わせて anchor する
- semantics review で `.ssq` の read-only boundary と矛盾しないことを確認する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/ssq.ebnf
- docs/examples/minimal.ssm
- docs/query/valid-similar-slot.ssq