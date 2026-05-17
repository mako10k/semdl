# 0030 Add Merge Warn-On-Conflict

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd merge` は現在、default sidecar precedence、`--fail-on-conflict`、`--prefer-source inline|sidecar` まで acceptance を固定している。
これにより merge policy matrix のうち failure-first と precedence selection は扱えるが、conflict を可視化しつつ merge 自体は継続したい surface がまだ残っている。

一方で warning mode を broad reporting や new selector syntax まで広げると、runner 契約、stderr golden、help 面が一気に膨らむ。
そのためこの slice では、既存 merge conflict scope を再利用しながら merge-only trailing policy として narrow に追加する。

## Decision

### 1. add merge-only `--warn-on-conflict`

この slice では `ssd merge` に限って trailing `--warn-on-conflict` を追加してよい。

- `ssd merge <input.ssd> --stdout --warn-on-conflict`
- `ssd merge <input.ssd> --dry-run --warn-on-conflict`
- `ssd merge <input.ssd> --out <output.ssd> [--dry-run] --warn-on-conflict`
- bare `ssd merge <input.ssd> --warn-on-conflict`
- `ssd merge <input.ssd> [--stdout|--dry-run|--out <output.ssd> [--dry-run]] --prefer-source inline|sidecar --warn-on-conflict`

### 2. warning mode preserves the selected merge result

`--warn-on-conflict` は merge result を変えない。

- flag なしでは current default sidecar precedence を維持する
- `--prefer-source` 併用時は selected precedence の merged inline view を維持する
- warning は success result に追加情報を付けるだけで、stdout preview や file output の本体を変えない

### 3. warning scope reuses the existing source-aware conflict boundary

warning 判定は [0021](0021-add-merge-fail-on-conflict-for-sidecar-owned-duplicates.md) と [0029](0029-add-merge-preferred-source-selection.md) の conflict scope をそのまま再利用する。

- merged view ではなく pre-merge source comparison に基づく
- document body の `version` / `generator`
- inline `meta {}` と paired `meta <id> {}`
- paired `document_meta <id> {}` と対応する inline source field
- `embedding.*` は leaf field 単位で比較する
- same-value duplicate は warning にしない

この slice では arbitrary structural field や broader diagnostics collection は含めない。
warning report は first differing duplicate 1 件だけを返してよい。

### 4. warning is emitted on stderr while command still succeeds

`--warn-on-conflict` 付き merge で differing duplicate が見つかった場合、CLI は exit code 0 のまま成功し、warning text を stderr に出してよい。

- stdout surface では merged inline `.ssd` を stdout に出しつつ、warning を stderr に出す
- dry-run surface では current preview stdout を維持しつつ、warning を stderr に出す
- `--out` surface では current success stdout を維持しつつ、warning を stderr に出す
- bare apply では current apply success stdout を維持しつつ、warning を stderr に出す
- conflict が無い場合は stderr は空のままでよい

### 5. `--fail-on-conflict` remains stronger than warning mode

`--fail-on-conflict` と `--warn-on-conflict` を同時に指定してよいが、differing duplicate があれば failure を優先する。

- failure は warning より先に判定してよい
- failure 時は warning stderr を追加しない
- same-value duplicate だけなら success のままでよい

## Consequences

- merge policy matrix に success-with-warning を追加しつつ、default compatibility を壊さずに済む
- runner は success case の stderr golden を比較対象として扱う必要があるが、既存 contract の範囲で収まる
- warning report を first-conflict 1 件に絞ることで、diagnostics expansion を別 slice に分離できる

## Alternatives Considered

### default merge を warning 付きに切り替える

却下。
既存 success stderr が空である contract を壊す。

### conflict warning を全件列挙する

却下。
diagnostics contract が広がり、golden 変化量が今回の slice より大きい。

### warning を stdout preview の一部に混ぜる

却下。
merged document / preview の stdout 契約が壊れ、stderr が持つ補助 diagnostics の責務から外れる。

## Validation

- requirements.md と requirements.llmthink.dsl を merge warning slice に同期する
- docs/cli.ebnf、merge help、root help を更新する
- success manifest に warning stdout / dry-run / out / apply と no-conflict stderr-empty case を追加または更新する
- failure manifest に `--warn-on-conflict` 非 trailing など boundary case を追加する
- CLI 実装で source-aware first-conflict warning を stderr へ載せ、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/merge-help.stdout
- docs/examples/golden/merge-help.semdl.stdout