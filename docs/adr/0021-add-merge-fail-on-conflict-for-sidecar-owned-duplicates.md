# 0021 Add Merge Fail-On-Conflict For Sidecar-Owned Duplicates

- Status: Accepted
- Date: 2026-05-16

## Context

`merge` は paired `.ssd` と `.ssm` を inline view へ統合するが、現状の実装では input `.ssd` を先に parse し、paired `.ssm` を後から同じ metadata map へ重ねている。
そのため、同じ target id と field key が inline source と sidecar の両方に存在し、値が異なる場合でも、現在は sidecar 値が暗黙に残る。

この implicit behavior 自体は既存 output と一致しているが、conflict policy が未文書化のままだと `merge` の safety option を acceptance で固定できない。
一方で warning mode や preferred source selection まで同時に導入すると option surface と lifecycle が広がりすぎる。

## Decision

### 1. add merge-only `--fail-on-conflict`

この slice では `ssd merge` に限って trailing `--fail-on-conflict` を追加してよい。

- `ssd merge <input.ssd> --stdout --fail-on-conflict`
- `ssd merge <input.ssd> --dry-run --fail-on-conflict`
- `ssd merge <input.ssd> --out <output.ssd> [--dry-run] --fail-on-conflict`
- bare `ssd merge <input.ssd> --fail-on-conflict`

flag なしの surface と paired-input requirement は既存のまま維持する。

### 2. default precedence stays explicit and stable

`--fail-on-conflict` を付けない `merge` は、既存 behavior を明示契約として固定する。

- same target id の duplicate sidecar-owned field が inline source と paired `.ssm` の両方にある場合、paired sidecar を優先する
- same-value duplicate は conflict とみなさない
- priority source を暗黙に切り替えない

### 3. conflict scope is source-aware and narrow

conflict detection は merged view ではなく pre-merge source comparison で判定する。

- document body の `version` / `generator`
- inline `meta {}` と paired `meta <id> {}` に現れる field
- paired `document_meta <id> {}` と対応する inline source field
- `embedding.*` は leaf field 単位で比較する

この slice では arbitrary structural field や preferred-source selector は対象に含めない。

### 4. failure happens before output or mutation

`--fail-on-conflict` 付き `merge` で differing duplicate が見つかった場合、CLI は merged stdout を返さず、output file を書かず、source `.ssd` を書き換えず、paired `.ssm` を削除しない。

## Consequences

- current merged output compatibility を保ったまま、conflict-sensitive workflow に opt-in failure を追加できる
- conflict check は parse order の副作用ではなく、source-aware な比較として固定される
- warning mode、preferred-source selection、broader merge policy matrix は引き続き後続 slice へ分離される

## Alternatives Considered

### default を error-first に切り替える

却下。
既存 merge output と acceptance を不必要に壊す。

### warning mode と preferred-source selection も同時に導入する

却下。
CLI contract と acceptance surface が一気に広がり、今回の slice より大きい。

### merged document を render 後に比較する

却下。
source origin が失われ、どちらの source が conflict を作ったか不明瞭になる。

## Validation

- requirements.md と requirements.llmthink.dsl を merge conflict slice に同期する
- docs/cli.ebnf と merge help を更新する
- success manifest に standalone stdout boundary と same-value duplicate success case を追加する
- failure manifest に `--fail-on-conflict` の stdout / dry-run / out / apply conflict cases を追加する
- core / CLI 実装で pre-merge source comparison を追加し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/merge-help.stdout
- docs/examples/golden/merge-help.semdl.stdout