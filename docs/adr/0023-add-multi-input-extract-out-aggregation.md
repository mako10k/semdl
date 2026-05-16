# 0023 Add Multi-Input Extract Out Aggregation

- Status: Accepted
- Date: 2026-05-16

## Context

`ssd extract` は既に raw `.txt` intake と explicit embedding adapter を持つが、
requirements の top-level command surface は `ssd extract --out <output.ssd> <input>...` を掲げている一方、
実装と help は実質 single-input のまま残っている。

この drift を放置すると、

- public CLI contract と実装が食い違う
- 複数 source を 1 つの `.ssd` へ集約する write path を acceptance で固定できない
- embedding generation を multi-input に広げるときの ID 衝突方針が未文書化のままになる

という問題が残る。

## Decision

### 1. extend only `extract --out`

この slice では multi-input surface を `ssd extract --out <output.ssd> [extract options] <input>...` に限定して追加する。

- trailing input は 1 件以上を受け付ける
- `--stdout` は引き続き single-input surface に留める
- `--embed-provider` と `--embed-model` は既存どおり optional pair として扱う
- output profile は inline `.ssd` に固定し、`--target`、profile selection、stdout multiplexing は引き続き後続 slice に分離する

### 2. allow mixed `.ssd` and `.txt` inputs

`extract --out` は existing `.ssd` と plain `.txt` を同一 command で混在させてよい。

- `.ssd` input は paired `.ssm` が存在すれば解決済み view として読み込む
- `.txt` input は既存 raw extract rule に従って skeletal document view へ変換する
- input order は source order として保持する

### 3. generate one merged `.ssd` with deterministic rebasing

output `.ssd` は input 集合の resolved view を 1 つに集約した canonical inline document とする。

- source input 間の entity ID 衝突は failure にせず rebasing で解決する
- rebased ID は kind ごとの running index により deterministic に採番する
- intra-document reference field は rebased ID へ書き換える
- `alternative_group` と `alternative.group` の shared token も source ごとに rebased token へ書き換える
- output order は input order を尊重しつつ canonical render に従う

### 4. embedding generation targets the rebased merged view

embedding option を伴う場合、generated `.ssm` は merged output view に対して 1 つだけ生成する。

- embeddable target selection は既存 extract plan を使う
- generated metadata record の `id` は rebased merged ID を使う
- output `.ssd` は merged inline document、paired `.ssm` は generated embedding records を保持する
- raw `.txt` + `--stdout` + embedding rejection など既存 failure contract は維持する

### 5. keep `--out` non-destructive and alias-safe

`extract --out` は multi-input でも non-destructive とし、source inputs や source sidecars を変更しない。

- `--out` は任意の source input path と alias してはならず、existing `.ssd` だけでなく raw `.txt` input も含めて equivalent path alias を failure にする
- `--out` は任意の source `.ssd` の paired `.ssm` と alias してはならず、equivalent path alias も failure にする
- generated paired `.ssm` も source input、source sidecar、または output `.ssd` と alias してはならず、equivalent path alias を failure にする

## Consequences

- requirements にある `extract --out <output.ssd> <input>...` の public surface を実装へ揃えられる
- mixed source aggregation を `extract` に閉じた write path として固定できる
- multi-input でも ID collision policy を failure ではなく deterministic rebasing に統一できる
- `--stdout` multiplexing や profile selection を同時に設計せずに済む
- non-destructive `--out` contract を multi-input extract にも維持できる

## Alternatives Considered

### multi-input を `--stdout` にも広げる

却下。
single stdout contract に aggregation と sidecar generation の追加判断が増え、slice が広がる。

### source ID collision を failure にする

却下。
既存 `.txt` skeleton が `D1` / `R1` / `S<n>` を固定採番するため、practical な multi-input surface にならない。

### `.ssd` と `.txt` の mixed input を禁止する

却下。
requirements の broad surface は input 種別を分けておらず、raw intake と existing semantic source を同じ `extract --out` path で扱える方が対称である。

### profile selection をこの slice で同時に導入する

却下。
multi-input aggregation に paired output lifecycle まで重ねると、generated `.ssm` と propagated source metadata の責務が一度に広がりすぎる。

## Validation

- requirements.md と requirements.llmthink.dsl を multi-input `extract --out` contract と deferred profile boundary に同期する
- docs/cli.ebnf と extract help を `--out` multi-input / `--stdout` single-input の境界に更新する
- success manifest に multi-input existing `.ssd`、multi-input mixed `.ssd` + `.txt`、multi-input + embedding case を追加する
- failure manifest に out-path alias rejection case を追加する
- CLI 実装で input aggregation、ID rebasing、reference rewriting、paired `.ssm` generation を実装する
- `./build/semdl_runner` を通す

## Related Artifacts

- docs/adr/0009-add-ollama-backed-embedding-generation-to-extract.md
- docs/adr/0014-expand-extract-provider-and-raw-text-intake.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json