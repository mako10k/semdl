# 0029 Add Merge Preferred Source Selection

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd merge` は現在、paired `.ssd` と `.ssm` の sidecar-owned duplicate field に対して paired sidecar を既定優先元として扱う。
これは [0021](0021-add-merge-fail-on-conflict-for-sidecar-owned-duplicates.md) で explicit contract として固定され、`--fail-on-conflict` による error-first 入口も追加済みである。

ただし merge policy matrix には、まだ「失敗せずにどちらを採るかを明示したい」需要が残っている。
このとき warning mode まで同時に入れると、stderr contract、success-with-warning の runner 契約、golden 面が一気に広がる。
そのため次の slice は warning ではなく、merged inline view の precedence だけを opt-in で切り替える最小 contract に絞る。

## Decision

### 1. add merge-only `--prefer-source <inline|sidecar>`

この slice では `ssd merge` に限って trailing `--prefer-source inline|sidecar` を追加してよい。

- `ssd merge <input.ssd> --stdout --prefer-source inline`
- `ssd merge <input.ssd> --dry-run --prefer-source inline`
- `ssd merge <input.ssd> --out <output.ssd> [--dry-run] --prefer-source inline`
- `ssd merge <input.ssd> --prefer-source inline`

`sidecar` は current default の explicit spelling として使ってよい。

### 2. precedence selection stays narrow and source-aware

`--prefer-source` は sidecar-owned duplicate field に対する merged view の precedence だけを切り替える。

- `inline` は inline source を優先する
- `sidecar` は paired `.ssm` を優先する
- duplicate でない field は current merged result のままとする
- scope は [0021](0021-add-merge-fail-on-conflict-for-sidecar-owned-duplicates.md) と同じく、document body の `version` / `generator`、inline `meta {}`、paired `meta`、paired `document_meta`、および `embedding.*` leaf field に限定する
- arbitrary structural field や new selector syntax はこの slice に含めない

### 3. `--fail-on-conflict` remains stronger than preference

`--fail-on-conflict` と `--prefer-source` を同時に指定してよいが、differing duplicate があれば failure を優先する。

- conflict がある場合は precedence selection に進む前に失敗する
- same-value duplicate は従来どおり success としてよい
- warning mode はこの slice でも導入しない

### 4. paired-input requirement stays explicit

`--prefer-source` は paired `.ssm` がある merge input にのみ意味を持つ。

- `--stdout --prefer-source ...` も paired input を要求してよい
- `--dry-run`、`--out`、bare apply は既存どおり paired input を要求する
- default merge stdout は standalone input を引き続き受け付けてよい

### 5. all merge surfaces stay aligned

preferred-source selection は stdout / dry-run / out / apply のすべてで同じ merged inline view を作る。

- `--stdout` は selected precedence の merged inline `.ssd` を返す
- `--dry-run` は selected precedence で apply したときの preview を返す
- `--out` は selected precedence の merged inline `.ssd` を non-destructive に書く
- bare apply は selected precedence の merged inline `.ssd` を source file へ書き、paired `.ssm` を削除する

## Consequences

- merge policy matrix を warning mode まで広げずに、explicit precedence selection を追加できる
- default behavior と `--fail-on-conflict` の既存 acceptance を壊さずに、review / distribution 用 merge output を opt-in で切り替えられる
- source-aware conflict scope を再利用するため、new field ownership ルールを導入せずに済む

## Alternatives Considered

### warning mode を先に入れる

却下。
success-with-warning の stderr contract と runner 面が一気に広がり、この slice より大きい。

### `--prefer-source` を merge default の置換にする

却下。
既存 output compatibility が崩れ、0021 の explicit default contract と衝突する。

### arbitrary field selector で precedence を切り替える

却下。
policy option ではなく selector language 拡張になり、責務と acceptance が広がりすぎる。

## Validation

- requirements.md と requirements.llmthink.dsl を merge preferred-source slice に同期する
- docs/cli.ebnf、merge help、root help を更新する
- success manifest に inline precedence の stdout / dry-run / out / apply と explicit sidecar precedence case を追加する
- failure manifest に standalone `--prefer-source`、invalid preferred source value、`--fail-on-conflict` 併用 conflict case を追加または更新する
- CLI / core 実装で source_views から precedence-selected merged document を構築し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/merge-help.stdout
- docs/examples/golden/merge-help.semdl.stdout