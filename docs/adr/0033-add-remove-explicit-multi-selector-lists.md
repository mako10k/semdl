# 0033 Add Remove Explicit Multi-Selector Lists

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd remove` は現在、single-target selector、`type:<kind> --allow-multi`、`type:<kind> --allow-multi --cascade`、および non-destructive output surface までを扱っている。
このため multi-target structural remove は一部扱えるが、複数の明示 target を 1 command にまとめる surface はまだ存在しない。

この穴が残ると、利用者は id/path で明示的に複数 target を指定して同時削除したくても、type selector に還元できる場合しか一括 remove を行えない。
一方で core selector language 全体を一般化すると query syntax、other command、cross-layer mixing まで広がりすぎる。

## Decision

### 1. add a remove-only explicit selector list surface

この slice では `ssd remove` に限って、既存 selector atom を comma-separated list として並べられる explicit multi-target surface を追加してよい。

- syntax は `<selector>,<selector>...` に限定してよい
- scope は remove CLI に閉じ、core selector language 全体や他 subcommand へは広げない
- target count が 1 の current single-selector surface はそのまま維持する

### 2. structural selector atoms only

explicit selector list は structural selector atom に限定してよい。

- allowed atom kinds は `id:<id>`、`path:<id>.<field>`、`type:<kind>` とする
- `meta:<id>.<field>` list はこの slice に含めない
- `doc:self` list もこの slice に含めない
- list 内で meta と structural を混在させる surface は導入しない

### 3. explicit list is itself the multi-target intent

comma-separated selector list は、それ自体で explicit multi-target intent として扱ってよい。

- list 内の `id:` / `path:` atom は `--allow-multi` なしで複数 target remove に使ってよい
- list 内の `type:<kind>` atom が複数 target に展開される場合だけ、既存 safety control として `--allow-multi` を要求してよい
- duplicate target ids は union set に正規化して 1 回だけ remove 対象に含めてよい

### 4. existing cascade and output surfaces lift to the union set

explicit selector list の structural remove は、resolved root id set の union として扱ってよい。

- `--cascade` は current single-target / type allow-multi slice と同じ edge listを union set へ適用してよい
- `--dry-run`、`--stdout`、`--out <output.ssd> [--dry-run]` は current remove output surface をそのまま使ってよい
- reference safety check は union removal set の外に dependent が残るかどうかで判定してよい

### 5. option ordering stays narrow

この slice でも remove の option ordering は current contract を維持してよい。

- safety options は selector の後に続く
- output options は safety options の後に続く
- output-before-safety reorder は failure のままとしてよい

## Consequences

- `type:<kind>` へ還元できない explicit multi-target structural remove が 1 command で扱える
- current remove cascade と non-destructive output implementation を再利用できる
- core selector parser や query syntax を reopen せずに remove-only の practical gap を埋められる

## Alternatives Considered

### core selector language 全体へ selector list を導入する

却下。
remove を越えて explain、set、annotate、query grammar まで境界が広がりすぎる。

### meta selector list も同時に導入する

却下。
cross-layer mixing と sidecar-only mutation semantics が同時に広がる。

### explicit selector list でも `--allow-multi` を常に必須にする

却下。
利用者が明示的に複数 atom を列挙しているため、`id:` / `path:` union にまで extra opt-in を要求すると surface が冗長になる。

## Validation

- requirements.md と requirements.llmthink.dsl を remove explicit multi-selector list slice に同期する
- docs/cli.ebnf と remove help / grammar help / root help goldens を更新する
- success manifest に explicit id/path multi-remove、cascade、stdout/out/dry-run、type atom 混在 list の cases を追加する
- failure manifest に meta list rejection、`type:<kind>` without `--allow-multi` inside list、output-before-safety reorder、unsafe dependent leftover を追加する
- CLI 実装で remove-only selector-list parse と union resolution を追加し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/remove-help.stdout
- docs/examples/golden/remove-help.semdl.stdout
- docs/examples/golden/help-grammar.stdout
- docs/examples/golden/help-grammar.semdl.stdout