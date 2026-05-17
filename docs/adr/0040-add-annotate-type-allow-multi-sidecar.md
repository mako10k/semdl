# 0040 Add Annotate Type Allow-Multi Sidecar

- Status: Accepted
- Date: 2026-05-18

## Context

`ssd annotate` は current surface では single-target selector semantics と explicit id-list multi-target までを扱うが、同じ annotation kind / text を kind 単位の matched set へ明示的に適用する narrow multi-target surface はまだ持っていない。
一方で `ssd set` は `type:<kind> ... --allow-multi` を既に持ち、kind selector を opt-in multi-target intent として使う safety contract は update command 群の中で実績がある。

ただし annotate は `--target inline|sidecar|auto` routing を持つため、type-based multi-target を full matrix のまま導入すると inline standalone 制約、supported-kind 制約、`auto` の heterogeneous routing policy を同時に reopen してしまう。
この slice で必要なのは annotate target matrix 全体の拡張ではなく、current sidecar annotate ownership を `type:<kind>` matched set にだけ lift する narrow sidecar-only surface である。

## Decision

### 1. add only `type:<kind> ... --target sidecar --allow-multi` to annotate

この slice では `ssd annotate type:<kind> <kind> <text> --target sidecar --allow-multi ... <file>` だけを追加してよい。

- `type:<kind>` を使う annotate surface では `--allow-multi` を常に必須としてよい
- matched target 数が 1 件でも `--allow-multi` は省略不可としてよい
- current single selector と explicit id-list surface はそのまま維持してよい
- generic selector list、mixed selector union、inline type multi-target annotate、`--target auto` multi-target routing はこの slice に含めない

### 2. keep current sidecar annotate ownership

`type:<kind>` annotate は current sidecar annotate ownership をそのまま複数 target へ lift してよい。

- target profile は `sidecar` のみに固定してよい
- paired input でも standalone input でも current sidecar annotate と同じ sibling `.ssm` write profile を使ってよい
- annotation kind の allowed set、metadata shape、result payload は current annotate sidecar surface をそのまま再利用してよい
- `--target inline` と `--target auto` はこの slice では failure としてよい

### 3. validate the whole matched set before mutation

`type:<kind>` annotate は all-or-nothing validation にしてよい。

- apply、dry-run、stdout、out、out-dry-run のいずれでも mutation 前に matched target 全件を解決してよい
- matched set が空なら existing missing-target failure としてよい
- partial success、per-id fallback、best-effort filtering はこの slice に含めない

### 4. fix canonical target order

matched target order は matched id の ascending order に固定してよい。

- preview / apply / output の `changes` はこの順序で確定した target 数に一致してよい
- dedicated reorder warning や alternate ordering rule は導入しない

### 5. reuse the current annotate sidecar output surface

`type:<kind>` annotate は current annotate sidecar output surface をそのまま再利用してよい。

- bare apply、`--dry-run`、`--stdout`、`--out <output-file>`、`--out <output-file> --dry-run` を current annotate ordering contract のまま許可してよい
- result payload は canonical sidecar `.ssm` に固定してよい
- `--stdout` conflict、`--out` alias protection、out-dry-run preview contract は current annotate output slice と同じでよい

### 6. keep option order narrow and explicit

この slice での new surface は narrow order に固定してよい。

- `ssd annotate type:<kind> <annotation-kind> <text> --target sidecar --allow-multi <file>`
- `ssd annotate type:<kind> <annotation-kind> <text> --target sidecar --allow-multi --dry-run <file>`
- `ssd annotate type:<kind> <annotation-kind> <text> --target sidecar --allow-multi --stdout <file>`
- `ssd annotate type:<kind> <annotation-kind> <text> --target sidecar --allow-multi --out <output-file> <file>`
- `ssd annotate type:<kind> <annotation-kind> <text> --target sidecar --allow-multi --out <output-file> --dry-run <file>`

この slice では `--allow-multi` の再配置、output-before-safety reorder、additional shorthand は導入しない。

## Consequences

- kind 単位の same-kind same-text annotate を narrow opt-in で sidecar metadata へまとめて適用できる
- annotate の current inline/auto routing を reopen せずに type-based multi-target gap を 1 段だけ埋められる
- current annotate sidecar diagnostics と output surface を再利用しつつ acceptance を閉じやすい

## Alternatives Considered

### `type:<kind>` annotate を single-match 時だけ `--allow-multi` なしで許可する

却下。
文書内件数で safety contract が変わる stopgap になり、set / remove の opt-in multi-target rule と非対称になる。

### `type:<kind>` annotate に `--target inline` も含める

却下。
standalone requirement と supported-kind validation を multi-target matched set 全体へ同時に広げる必要があり広すぎる。

### `type:<kind>` annotate に `--target auto` も含める

却下。
standalone input で per-id resolved profile が割れ得るため heterogeneous routing policy が必要になる。

## Validation

- requirements.md と requirements.llmthink.dsl を annotate type allow-multi sidecar slice に同期する
- docs/cli.ebnf を更新し、annotate help / grammar help / root help goldensを同期する
- success manifest に type allow-multi sidecar apply、dry-run、stdout、out、out-dry-run を追加する
- failure manifest に missing `--allow-multi`、invalid target、invalid option ordering、missing-target、out-alias を追加する
- CLI 実装で matched target collection、all-or-nothing validation、sidecar write path を追加し、targeted validation を通す

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