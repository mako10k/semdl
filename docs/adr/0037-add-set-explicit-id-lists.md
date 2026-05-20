# 0037 Add Set Explicit Id Lists

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd set` は current surface では single-target selector semantics に限定されており、id selector も 1 件ずつしか更新できない。
一方で `ssd remove` は explicit multi-target intent として comma-separated selector list をすでに持っており、複数 id に同じ mutation intent を明示する narrow surface の有用性は確認できている。

ただし remove の generic structural selector list をそのまま set に移植すると slice が広がりすぎる。
set は id selector では field 名を argv 側に持ち、path / meta selector では field path を selector 自体に埋め込むため、generic list 化は mixed atom syntax、field ownership、wrong-layer、atomicity を同時に reopen してしまう。
この slice で必要なのは selector language 全体の拡張ではなく、current id-set contract を複数 explicit id にだけ lift する narrow な multi-target surface である。

## Decision

### 1. add id-list-only surface to set

この slice では `ssd set` に explicit id list only surface を追加してよい。

- syntax は `ssd set id:<id>,id:<id>,... <field> <value> ... <file>` に限定してよい
- list atom は `id:` に限定してよい
- `path:`、`meta:`、`type:`、`doc:self`、mixed selector list はこの slice に含めない
- single `id:` selector の current surface はそのまま維持してよい

### 2. keep current id-set ownership

id list set は current id-selector semantics をそのまま複数 target へ lift してよい。

- field 名は command argv 側に 1 回だけ置いてよい
- current id set が inline ownership を持つ field だけを対象にしてよい
- metadata-owned field は current single-id set と同じく wrong-layer failure としてよい
- result payload は current id set と同じ Solo mode を使ってよい

### 3. validate the whole list before mutation

id list set は all-or-nothing validation にしてよい。

- apply / stdout / out / dry-run のいずれでも mutation 前に全 id を解決・検証してよい
- 1 件でも target_not_found、wrong_layer、unsupported field があれば全体 failure としてよい
- partial success や best-effort apply はこの slice に含めない

### 4. deduplicate ids in stable order

id list 内の duplicate id は stable first-seen order で dedup してよい。

- preview と apply は dedup 後の target set に対して 1 回ずつ update してよい
- `changes` は dedup 後の target 数に一致してよい
- duplicate id を dedicated warning にする必要はない

### 5. reuse existing output surface without widening it

id list set は current set output surface をそのまま使ってよい。

- bare apply、`--dry-run`、`--stdout`、`--out <output-file>`、`--out <output-file> --dry-run` を current ordering contract のまま許可してよい
- `--stdout` conflict、`--out` alias protection、out-dry-run preview contract は current set output slice と同じでよい
- output payload は canonical inline `.ssd` に固定してよい

### 6. keep scope narrow

この slice では id list set だけを追加してよい。

- generic selector list は導入しない
- `path:` list と `meta:` list は導入しない
- `type:` multi-target set や `--allow-multi` は導入しない
- selector list ごとの heterogeneous field mapping は導入しない

## Consequences

- same-field same-value update を複数 explicit id にまとめて適用できる
- set の single-id semantics を崩さずに narrow な multi-target surface を追加できる
- generic selector expansion と multi-match safety を reopen せずに tractable な hard slice を進められる

## Alternatives Considered

### remove-style generic selector listsをそのまま set に持ち込む

却下。
field の持ち方と layer ownership が remove と対称ではなく、mixed atom semantics が別設計になる。

### type-based multi-target setを先に導入する

却下。
多重解決 safety、field availability、atomicity を同時に reopen するため広すぎる。

### path listを先に導入する

却下。
selector 自体に field path が埋め込まれており、複数 atom 間で同一 field contract を固定しにくい。

## Validation

- requirements.md と requirements.llmthink.dsl を set explicit id list slice に同期する
- docs/cli.ebnf を更新し、set help / grammar help goldensを同期する
- success manifest に id list apply、dry-run、stdout、out、out-dry-run を追加する
- failure manifest に non-id list selector、missing target、wrong-layer の id-list cases を追加する
- CLI 実装で current id set validation / apply path を list-aware に拡張し、targeted validation を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/set-help.stdout
- docs/examples/golden/set-help.semdl.stdout
- docs/examples/golden/help-grammar.stdout
- docs/examples/golden/help-grammar.semdl.stdout
- src/cli/cli_app.cpp
