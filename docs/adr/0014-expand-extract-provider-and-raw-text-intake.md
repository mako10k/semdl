# 0014 Expand Extract Provider And Raw Text Intake

- Status: Accepted
- Date: 2026-05-16

## Context

ADR 0009 では `ssd extract` の最初の成功面を既存 `.ssd` への Ollama-backed enrichment に限定し、
raw input extraction と non-Ollama provider は後続 slice へ分離した。

その境界は初期 slice としては有効だったが、現状では次の不足が残っている。

- `extract` help と root help が示す raw input extraction を実際には実行できない
- provider surface が `ollama` だけに固定され、explicit provider adapter 境界を試せない
- raw input と embedding generation の併用契約が未固定で、stdout / file output の扱いが曖昧なまま残る

## Decision

### 1. explicit providers

`ssd extract` の embedding provider は explicit adapter surface として `ollama` と `openai` を受け付ける。

- public option は継続して `--embed-provider <provider>` と `--embed-model <model>` を使う
- 初期 supported provider は `ollama` と `openai` の 2 つに限定する
- unsupported provider は failure にする
- 実装は provider ごとの adapter で command invocation を分離し、互換 SDK を前提にしない
- 受け入れテストでは environment override により deterministic fixture command へ差し替えてよい

### 2. raw text intake

`ssd extract` は plain `.txt` input を受け付け、skeletal `.ssd` を生成する。

初期 raw extraction は semantic inference を行わず、少なくとも以下を生成する。

- `document D1`
- `resource R1`
- non-empty line ごとの `segment S<n>`

`document.title` と `resource.label` は input file の stem から導出し、
`document.source_ref` は command line で指定された input path を保持する。

segment は input の non-empty line を順番通りに `text_quote` へ写す。
この slice では assertion / hypothesis / alternative を推論しない。

### 3. raw stdout and embedding contract

raw `.txt` input に対する `--stdout` は generated `.ssd` のみを返す。

- raw `.txt` input で `--stdout` を使う場合、embedding option は併用不可とする
- raw `.txt` input で embedding generation を行う場合は `--out <output.ssd>` を必須とする
- raw `.txt` input の paired `.ssm` は `--out` 成功時のみ書き出す

この制約により、single stdout channel に `.ssd` と `.ssm` を同時に載せる新契約は導入しない。

### 4. raw embeddable targets

raw `.txt` から生成した document に embedding generation を適用する場合、初期対象は次に限定する。

- `resource R1` の `label`
- 各 `segment S<n>` の `text_quote`

`document D1` は embeddable target に含めない。
generated `.ssm` の record shape は ADR 0009 の embedding metadata contract をそのまま使う。

## Consequences

- `extract` の help surface と実装を raw intake まで整合させられる
- provider surface を explicit adapter boundary で拡張しつつ、read path への漏れを避けられる
- raw `--stdout` と embedding generation の競合を file-output 限定で解消できる

## Alternatives Considered

### raw `--stdout` で `.ssd` と `.ssm` の両方をまとめて返す

却下。
single stdout の既存 contract を崩し、追加の multiplexing 仕様が必要になる。

### raw input から assertion / hypothesis まで自動推論する

却下。
semantic extraction と provider / persistence の判断軸を同時に広げ過ぎる。

### provider surface を generic compatibility SDK に寄せる

却下。
provider 間の互換性を既定前提にせず、explicit adapter 境界で拡張した方が安全である。

## Validation

- requirements.md と requirements.llmthink.dsl を provider 追加と raw text intake に更新する
- extract help と root help を supported surface に合わせて更新する
- success manifest に `openai` provider case と raw text stdout / out cases を追加する
- failure manifest に raw `--stdout` + embedding conflict case を追加する
- `ssd extract` 実装を更新し、runner で受け入れを通す

## Related Artifacts

- docs/adr/0009-add-ollama-backed-embedding-generation-to-extract.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/extract-help.stdout
- docs/examples/golden/extract-help.semdl.stdout