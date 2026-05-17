# 0038 Add Set Type Allow-Multi

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd set` は current surface では single-target field update と explicit id-list inline update までを扱うが、kind 単位で同一 field mutation を明示する narrow multi-target surface はまだ持っていない。
一方で selector 契約では `type:<kind>` 自体は存在し、更新系では安全のため `--allow-multi` がある場合のみ許可してよいと requirements に固定されている。

ただし remove-style generic selector list や mixed selector union をそのまま set に広げると、field mapping、layer ownership、target ordering、diagnostics を同時に reopen して slice が広がりすぎる。
この段階で必要なのは generic selector language の拡張ではなく、同一 kind に解決される複数 target へ同一 field/value mutation を opt-in で適用する narrow surface だけである。

## Decision

### 1. add only `type:<kind> ... --allow-multi` to set

この slice では `ssd set type:<kind> <field> <value> --allow-multi ... <file>` だけを追加してよい。

- `type:<kind>` を使う set surface では `--allow-multi` を常に必須としてよい
- matched target 数が 1 件でも `--allow-multi` は省略不可としてよい
- single `id:`、explicit id list、`path:`、`meta:` の current surface はそのまま維持してよい
- generic selector list、mixed selector union、`doc:self` set はこの slice に含めない

### 2. keep current set field ownership and failure taxonomy

`type:<kind>` set は current set field ownership をそのまま複数 target へ lift してよい。

- field 名は command argv 側に 1 回だけ置いてよい
- matched target 全件で inline-owned field として解決できる場合だけ apply してよい
- metadata-owned field は existing set と同じ wrong-layer failure としてよい
- field が current set taxonomy 上 unresolved な target が 1 件でもあれば existing missing-target failure としてよい
- partial success や best-effort filtering はこの slice に含めない

### 3. validate the whole matched set before mutation

`type:<kind>` set は all-or-nothing validation にしてよい。

- apply、dry-run、stdout、out、out-dry-run のいずれでも mutation 前に matched target 全件を解決・検証してよい
- 1 件でも field validation failure があれば全体 failure としてよい
- mutation、stdout generation、out write は validation 完了後にだけ進んでよい

### 4. fix canonical target order

matched target order は matched id の ascending order に固定してよい。

- preview の target list はこの順序で並べてよい
- `changes` はこの順序で確定した target 数に一致してよい
- future generic selector expansion を導入するまで、別 order rule を追加しない

### 5. reuse the current set output surface

`type:<kind>` set は current set output surface をそのまま再利用してよい。

- bare apply、`--dry-run`、`--stdout`、`--out <output-file>`、`--out <output-file> --dry-run` を current set ordering contract のまま許可してよい
- result payload は canonical inline `.ssd` に固定してよい
- `--stdout` conflict、`--out` alias protection、out-dry-run preview contract は current set output slice と同じでよい

### 6. keep option order narrow and explicit

この slice での new surface は narrow order に固定してよい。

- `ssd set type:<kind> <field> <value> --allow-multi <file>`
- `ssd set type:<kind> <field> <value> --allow-multi --dry-run <file>`
- `ssd set type:<kind> <field> <value> --allow-multi --stdout <file>`
- `ssd set type:<kind> <field> <value> --allow-multi --out <output-file> <file>`
- `ssd set type:<kind> <field> <value> --allow-multi --out <output-file> --dry-run <file>`

この slice では `--allow-multi` の再配置、output-before-safety reorder、additional shorthand は導入しない。

## Consequences

- kind 単位の同一 field update を narrow opt-in でまとめて適用できる
- generic selector language を reopen せずに `set` の multi-target gap を 1 段だけ埋められる
- existing set output surface と diagnostics を再利用しつつ acceptance を閉じやすい

## Alternatives Considered

### `type:<kind>` を single-match 時だけ `--allow-multi` なしで許可する

却下。
文書内件数で safety contract が変わる stopgap になり、更新 selector の opt-in ルールと非対称になる。

### remove-style generic selector list を set に導入する

却下。
field mapping、layer ownership、mixed atom semantics が同時に広がる。

### path list や meta list を先に導入する

却下。
field path と write profile の扱いが type-based same-field update より広くなる。

## Validation

- requirements.md と requirements.llmthink.dsl を set type allow-multi slice に同期する
- docs/cli.ebnf と set help / grammar help / root help goldens を更新する
- success manifest に type allow-multi apply、dry-run、stdout、out、out-dry-run を追加する
- failure manifest に missing `--allow-multi`、wrong-layer、missing-target、invalid option ordering を追加する
- CLI 実装で matched target collection、all-or-nothing validation、resolved-profile write path を追加し、targeted validation を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/set-help.stdout
- docs/examples/golden/set-help.semdl.stdout
- docs/examples/golden/help-root.stdout
- docs/examples/golden/help-root.semdl.stdout
- docs/examples/golden/help-root-alias.semdl.stdout
- docs/examples/golden/help-grammar.stdout
- docs/examples/golden/help-grammar.semdl.stdout
- src/cli/cli_app.cpp
