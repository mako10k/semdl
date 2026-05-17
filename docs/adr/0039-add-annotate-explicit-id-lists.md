# 0039 Add Annotate Explicit Id Lists

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd annotate` は current surface では single-target selector semantics に限定されており、同じ annotation kind と text を複数 id にまとめて適用する narrow な multi-target path がない。
一方で `ssd set` は explicit id list slice をすでに持っており、generic selector semantics を reopen せずに explicit id intent だけを複数 target へ lift する方針は確認済みである。

ただし annotate は set と違って `--target inline|sidecar|auto` routing を持つため、generic selector list や `auto` をそのまま multi-target に広げると per-id resolved profile の heterogeneity を同時に設計する必要が出る。
この slice で必要なのは annotate target matrix 全体の再設計ではなく、existing explicit target contract を複数 explicit id にだけ lift する narrow multi-target surface である。

## Decision

### 1. add id-list-only surface to annotate

この slice では `ssd annotate` に explicit id list only surface を追加してよい。

- syntax は `ssd annotate id:<id>,id:<id>,... <kind> <text> --target inline|sidecar ... <file>` に限定してよい
- list atom は `id:` に限定してよい
- `type:`、`path:`、`meta:`、`doc:self`、mixed selector list はこの slice に含めない
- single selector の current surface はそのまま維持してよい

### 2. keep current single-target ownership

id list annotate は current single-target annotate ownership をそのまま複数 target へ lift してよい。

- `--target sidecar` では current sidecar annotate と同じ metadata write profile を使ってよい
- `--target inline` では current inline annotate と同じ standalone-only / supported-kind-only 制約を保ってよい
- `--target auto` はこの slice に含めず failure のままとしてよい
- result payload は existing annotate output surface をそのまま再利用してよい

### 3. validate the whole list before mutation

id list annotate は all-or-nothing validation にしてよい。

- apply / stdout / out / dry-run のいずれでも mutation 前に全 id を解決・検証してよい
- 1 件でも missing target、inline unsupported、standalone requirement violation、unsupported selector atom があれば全体 failure としてよい
- partial success や per-id fallback はこの slice に含めない

### 4. deduplicate ids in stable order

id list 内の duplicate id は stable first-seen order で dedup してよい。

- preview と apply は dedup 後 target set に対して 1 回ずつ annotate してよい
- `changes` は dedup 後の target 数に一致してよい
- duplicate id を dedicated warning にする必要はない

### 5. reuse existing annotate output surface without widening it

id list annotate は current annotate output surface をそのまま使ってよい。

- bare apply、`--dry-run`、`--stdout`、`--out <output-file>`、`--out <output-file> --dry-run` を current ordering contract のまま許可してよい
- `--stdout` conflict、`--out` alias protection、out-dry-run preview contract は current annotate output slice と同じでよい
- `--target inline` の result payload は canonical inline `.ssd`、`--target sidecar` は canonical sidecar `.ssm` でよい

### 6. keep scope narrow

この slice では explicit id list annotate だけを追加してよい。

- generic selector list は導入しない
- `type:` multi-target annotate は導入しない
- `--allow-multi` は導入しない
- `--target auto` の multi-target routing policy は導入しない
- mixed target profile resolution は導入しない

## Consequences

- same-kind same-text annotate を複数 explicit id にまとめて適用できる
- annotate の current target ownership を崩さずに narrow な multi-target surface を追加できる
- generic selector expansion や heterogeneous routing を reopen せずに tractable な hard slice を進められる

## Alternatives Considered

### generic selector list をそのまま annotate に持ち込む

却下。
selector language 全体と target routing policy を同時に reopen してしまう。

### `--target auto` を id list annotate に含める

却下。
standalone input で per-id resolved profile が割れ得るため、この slice の責務を越える heterogeneous routing policy が必要になる。

### type-based multi-target annotate を先に導入する

却下。
matched-set ordering、safety opt-in、routing policy を同時に設計する必要があり広すぎる。

## Validation

- requirements.md と requirements.llmthink.dsl を annotate explicit id list slice に同期する
- docs/cli.ebnf を更新し、annotate help / grammar help / root help goldensを同期する
- success manifest に sidecar / inline の id-list annotate cases を追加する
- failure manifest に non-id selector、`--target auto`、missing target、inline unsupported の id-list cases を追加する
- CLI 実装で current annotate validation / apply path を list-aware に拡張し、targeted validation を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/annotate-help.stdout
- docs/examples/golden/annotate-help.semdl.stdout
- docs/examples/golden/help-root.stdout
- docs/examples/golden/help-root.semdl.stdout
- docs/examples/golden/help-root-alias.semdl.stdout
- docs/examples/golden/help-grammar.stdout
- docs/examples/golden/help-grammar.semdl.stdout
- src/cli/cli_app.cpp
