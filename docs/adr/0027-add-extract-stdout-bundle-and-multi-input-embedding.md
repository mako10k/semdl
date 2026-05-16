# 0027 Add Extract Stdout Bundle And Multi-Input Embedding

- Status: Accepted
- Date: 2026-05-17

## Context

ADR 0026 では embedding-enabled `ssd extract --stdout` に single existing `.ssd` input 向け `--format inline|sidecar` を追加し、omitted `--format` の既定値は sidecar のまま固定した。
その結果、single-input stdout で downstream profile を選ぶ問題は解けたが、extract の deferred bucket にはまだ次が残っていた。

- embedding-enabled `extract --stdout` は multi-input existing `.ssd` を受け付けない
- single command で canonical inline `.ssd` と generated `.ssm` を同時に受け取りたい場合の stdout multiplexing contract がない
- 既存 `--format` option は shared token として `inline|sidecar` だけを表しており、extract 専用 stdout bundle をどこで開くかが未定義

一方で multi-input `extract --out` はすでに resolved merged view と deterministic rebasing を持っている。
そのため、multi-input embedding-enabled stdout も新しい merge primitive を増やす必要はなく、既存 aggregate/rebase path と single-input stdout renderer を再利用するのが最小である。

## Decision

### 1. keep ADR 0026 single-input default unchanged

single existing `.ssd` input に対する embedding-enabled `ssd extract --stdout` では、`--format` omitted の既定値を引き続き sidecar とする。

- `ssd extract --stdout --embed-provider <provider> --embed-model <model> <input.ssd>` は generated `.ssm` text を返す
- `ssd extract --stdout --embed-provider <provider> --embed-model <model> --format sidecar <input.ssd>` は omitted と同じ payload を返す
- ADR 0026 の single-input default decision は reopen しない

### 2. add command-local `bundle` only to embedding-enabled extract stdout

この slice では `bundle` を shared format option の新値としては扱わず、embedding-enabled `extract --stdout` 専用の command-local profile token として導入する。

- `ssd extract --stdout --embed-provider <provider> --embed-model <model> --format bundle <input.ssd>`
- `ssd extract --stdout --embed-provider <provider> --embed-model <model> --format bundle <input.ssd> <input.ssd>...`
- no-provider `extract --stdout`、`extract --out`、`normalize` など他 surface では `--format bundle` を受け付けない

### 3. open multi-input embedding stdout behind explicit format selection

embedding-enabled `extract --stdout` は 1 件以上の existing `.ssd` input を受け付けてよい。
ただし multi-input のとき omitted `--format` は許可せず、`inline|sidecar|bundle` のいずれかを明示必須とする。

- `--format inline` は rebased merged view を canonical inline `.ssd` として返す
- `--format sidecar` は rebased merged IDs を使う generated `.ssm` text を返す
- `--format bundle` は同じ rebased merged view から inline `.ssd` と generated `.ssm` の両方を返す
- raw `.txt` を含む embedding-enabled stdout は single/multi ともに引き続き failure とする

### 4. define bundle framing as line-preserving plain text

`--format bundle` は plain-text framing を返す。payload 自体は escaped せず、各 payload line の先頭に `|` prefix を付けて section boundary を固定する。

Canonical shape:

```text
stdout_profile: bundle
inline_profile: inline
sidecar_profile: sidecar

inline_document:
| <line>
| <line>
|

sidecar_document:
| <line>
| <line>
```

- header 順序は `stdout_profile`、`inline_profile`、`sidecar_profile` に固定する
- `inline_document:` と `sidecar_document:` section 名は固定する
- payload line は非空行なら `| ` + original line、空行なら bare `|` を使う
- bundle 全体は 1 つの exact stdout contract として golden 化する

### 5. reuse existing aggregate and inline-attachment behavior

- multi-input document aggregation と deterministic rebasing は existing `extract --out` path を再利用する
- `--format inline` と bundle 内 inline payload は ADR 0026 で定義した single-file `.ssd` embedding attachment contract をそのまま使う
- `assertion` / `hypothesis` は entity-local inline `meta`、other embeddable kind は top-level `meta <id>` block を使ってよい

## Consequences

- multi-input embedding-enabled stdout を、single-input default compatibility を壊さずに追加できる
- stdout multiplexing は hidden default ではなく explicit profile として説明できる
- shared `--format` vocabulary を repo 全体で拡張せず、extract 局所の contract 変更に留められる
- help、grammar、manifest、golden では extract 専用 `bundle` と help render 用 `--format semdl` の違いをより明示する必要がある

## Alternatives Considered

### single-input omitted default も bundle に切り替える

却下。
ADR 0026 の accepted default を壊し、既存 downstream の sidecar stdout 期待を破壊する。

### omitted `--format` を multi-input でも bundle default にする

却下。
omitted が hidden third profile になり、profile contract が非対称になる。

### `bundle` を shared `format-opt` の新値として repo-wide に追加する

却下。
normalize など他 command にも surface が漏れ、change radius が過大になる。

## Validation

- requirements.md と requirements.llmthink.dsl を extract stdout bundle / multi-input embedding stdout contract に同期する
- docs/cli.ebnf を command-local extract stdout `bundle` surface に更新する
- success manifest に single-input bundle、multi-input inline、multi-input sidecar、multi-input bundle success を追加する
- failure manifest に multi-input omitted `--format`、no-provider `--format bundle`、`extract --out --format bundle` rejection を追加する
- CLI help / root help goldens と extract-specific help goldens を更新する
- CLI 実装で multi-input embedding stdout と bundle renderer を追加し、runner を通す

## Related Artifacts

- docs/adr/0023-add-multi-input-extract-out-aggregation.md
- docs/adr/0025-add-inline-multi-input-extract-stdout.md
- docs/adr/0026-add-single-input-extract-stdout-profile-selection.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/extract-help.stdout
- docs/examples/golden/extract-help.semdl.stdout
- docs/examples/golden/help-root.stdout
- docs/examples/golden/help-root.semdl.stdout
- docs/examples/golden/help-root-alias.semdl.stdout