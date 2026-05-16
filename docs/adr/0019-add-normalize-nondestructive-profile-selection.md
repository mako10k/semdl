# 0019 Add Normalize Nondestructive Profile Selection

- Status: Accepted
- Date: 2026-05-16

## Context

transform 系では `merge` と `normalize` に dry-run / out が追加されたが、result profile は inline に固定されたままだった。
一方で requirements は `--format inline|sidecar` を更新系の共通 option 候補として持ち、help surface でも help render 用の `--format semdl` と command content/profile 用の `--format inline|sidecar` を区別できることを要求している。

ただし profile selection を transform 全体へ一度に広げると、`merge` と `split` の command responsibility が崩れやすい。
さらに merge / sidecar conflict policy はまだ未固定で、in-place lifecycle まで同時に広げると slice が大きくなりすぎる。

## Decision

### 1. profile selection stays normalize-only and non-destructive

この slice で `--format inline|sidecar` を受けるのは `normalize` の non-destructive surface だけに限定する。

- `ssd normalize <input.ssd> --dry-run --format inline|sidecar`
- `ssd normalize <input.ssd> --out <output.ssd> --format inline|sidecar`

`--format` を省略した場合の既定値は inline とする。

### 2. bare apply and stdout stay inline-only

次の surface は従来のまま inline-only とする。

- `ssd normalize <input.ssd>`
- `ssd normalize <input.ssd> --stdout`

この slice ではこれらに `--format` を足さない。

### 3. sidecar output contract

`normalize --format sidecar` は non-destructive output pair を生成または preview する。

- `--out <output.ssd> --format sidecar` は `<output.ssd>` と sibling `<output.ssm>` を生成する
- `--dry-run --format sidecar` は input base を target とした sidecar pair preview を返す
- source `.ssd` と source `.ssm` は変更しない
- output `.ssd` と derived sibling `.ssm` は source files を alias してはならない

### 4. split and merge responsibilities remain fixed

- `split` は sidecar-oriented command のままとする
- `merge` は inline-oriented command のままとする
- この slice では split / merge に profile selection を追加しない

### 5. conflict policy remains deferred

merge / sidecar conflict policy、priority selection、warning mode は引き続き後続 slice に分離する。
この slice は profile selection だけを固定し、暗黙の優先規則を新設しない。

## Consequences

- help surface で `--format semdl` と `--format inline|sidecar` の役割差を具体化できる
- normalize にだけ profile selection を開くことで、merge / split の責務境界を壊さずに option surface を一段進められる
- sidecar profile は non-destructive output に閉じるため、paired apply lifecycle や conflict policy を同時に設計せずに済む

## Alternatives Considered

### merge と split にも同時に `--format` を追加する

却下。
command matrix が一気に広がり、responsibility の説明と acceptance が複雑になる。

### bare normalize apply に `--format sidecar` を追加する

却下。
in-place sidecar lifecycle と conflict policy まで同時に固定する必要がある。

### profile selection と conflict policy を同時に扱う

却下。
現状の transform implementation には conflict policy primitive がなく、slice が過大になる。

## Validation

- requirements.md と requirements.llmthink.dsl を normalize non-destructive profile slice に同期する
- cli.ebnf と normalize help / grammar help を更新する
- normalize sidecar dry-run / out success cases と invalid format-surface failure casesを追加する
- CLI 実装で normalize `--format inline|sidecar` を追加し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/normalize-help.stdout
- docs/examples/golden/help-grammar.stdout
