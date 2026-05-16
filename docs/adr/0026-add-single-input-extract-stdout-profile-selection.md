# 0026 Add Single-Input Extract Stdout Profile Selection

- Status: Accepted
- Date: 2026-05-17

## Context

ADR 0025 では no-provider の `ssd extract --stdout` だけを 1 件以上の input へ広げ、embedding-enabled stdout は single existing `.ssd` で generated `.ssm` text を返す narrow surface のまま据え置いた。
その判断は multiplexed stdout framing を避けるには妥当だったが、現在は次の歪みが残っている。

- embedding-enabled `extract --stdout` は single-input でも output profile を選べず、generated embeddings を 1 本の `.ssd` text として downstream へ渡せない
- `--stdout` の profile selection と stdout multiplexing が同じ deferred bucket に残り、より小さい follow-up slice を acceptance で固定できない
- 一方で multi-input embedding-enabled stdout まで同時に広げると、inline `.ssd` と generated `.ssm` を stdout へどう multiplex するかまで決める必要がある

また `.ssd` primary grammar では inline `meta { ... }` を entity block 内に持てるのは assertion / hypothesis に限られる。
resource / segment / alternative などの generated embedding metadata を single-file `.ssd` output へ載せるには、既存 top-level `meta <id>` block を再利用する必要がある。

## Decision

### 1. open `--format inline|sidecar` only for embedding-enabled stdout

この slice で `--format inline|sidecar` を受けるのは single existing `.ssd` input に対する embedding-enabled `ssd extract --stdout` だけに限定する。

- `ssd extract --stdout --embed-provider <provider> --embed-model <model> [--format inline|sidecar] <input.ssd>`
- `--format` 省略時の既定値は現行互換の `sidecar` とする
- no-provider `extract --stdout` は引き続き inline-only とし、`--format` を受け付けない
- `extract --out` はこの slice では `--format` を受け付けない

### 2. define inline stdout as a single `.ssd` payload without multiplexing

`--format inline` は generated embeddings を 1 本の `.ssd` text として返す。

- assertion / hypothesis の generated embedding metadata は existing inline `meta { embedding { ... } }` shape へ載せてよい
- inline `meta` を持てない embeddable kind の generated metadata は same stdout payload 内の trailing top-level `meta <id>` block として載せる
- 新しい stdout bundle framing や多重パート境界は導入しない

### 3. sidecar stdout remains the default and explicit surface

- `--format sidecar` は current behavior と同じ generated `.ssm` profile text を返す
- `--format` omitted と `--format sidecar` は同じ payload を返す
- raw `.txt` + embedding + `--stdout` rejection は維持する

### 4. multiplexing and multi-input embedding stdout remain deferred

この slice では次を導入しない。

- embedding-enabled `extract --stdout` の multi-input surface
- inline `.ssd` と generated `.ssm` を同時に載せる multiplexed stdout framing
- `--target`

## Consequences

- extract stdout の deferred bucket から profile selection だけを切り出して固定できる
- single-input embedding-enabled stdout で `.ssd` / `.ssm` のどちらを downstream へ渡すかを明示できる
- `.ssd` grammar を広げずに、non-inlineable kind の metadata は existing top-level `meta <id>` block へ退避できる
- help / grammar / acceptance では `--format semdl` と command content profile 用 `--format inline|sidecar` の役割差をより明確に説明する必要がある

## Alternatives Considered

### multi-input embedding-enabled stdout まで同時に追加する

却下。
stdout multiplexing contract まで同時に設計する必要があり、slice が大きくなりすぎる。

### no-provider stdout と extract --out にも同時に `--format` を追加する

却下。
no-provider stdout はすでに canonical inline `.ssd` に固定されており、`extract --out` は non-destructive paired write path で責務が異なる。

### `--format inline` では全 embeddable kind に entity-local inline meta を許す

却下。
`.ssd` primary grammar の block entry surface を広げる別の grammar slice が必要になる。

## Validation

- requirements.md と requirements.llmthink.dsl を single-input embedding-enabled stdout profile selection slice に同期する
- docs/cli.ebnf と extract help / root help を embedding-enabled stdout `--format inline|sidecar` surface に更新する
- success manifest に explicit `--format sidecar` と `--format inline` success を追加する
- failure manifest に no-provider stdout と extract --out での `--format` rejection を追加する
- CLI 実装で embedding-enabled stdout profile selection を追加し、runner を通す

## Related Artifacts

- docs/adr/0009-add-ollama-backed-embedding-generation-to-extract.md
- docs/adr/0025-add-inline-multi-input-extract-stdout.md
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