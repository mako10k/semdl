# 0032 Add Remove Nondestructive Output Surface

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd remove` は現状、metadata subtree delete、single-target structural remove、single-target `--cascade`、および `type:<kind> --allow-multi --cascade` までを apply path として扱っている。
一方で non-destructive surface は未実装のままで、remove だけが update command の中で preview / stdout / out parity から外れている。

この穴が残ると、削除結果を pipeline に流したり、別ファイルへ安全に書き出したり、write policy を変えずに削除結果を確認したりする経路が remove に存在しない。
ただし sidecar pair 出力や output profile selection まで同時に入れると、remove の write policy と render contract が大きく広がる。

## Decision

### 1. add inline-only non-destructive surfaces to `remove`

この slice では `ssd remove` に `--dry-run`、`--stdout`、`--out <output.ssd>` を追加してよい。

- scope は existing selector semantics を維持する
- `--allow-multi` は引き続き `type:<kind>` に限定してよい
- broader multi-target selector semantics は引き続き後続 slice に分離する

### 2. stdout and out render the post-remove canonical inline view

`--stdout` と `--out <output.ssd>` は、remove 後の canonical inline `.ssd` を扱ってよい。

- structural remove でも metadata remove でも result profile は inline に固定してよい
- paired `.ssd` + `.ssm` input でも、stdout / out は sidecar pair を生成せず inline result だけを返してよい
- `--stdout` は source `.ssd` と paired `.ssm` を変更しない
- `--out <output.ssd>` は source `.ssd` と paired `.ssm` を変更しない

### 3. dry-run previews the same remove result without writing

`--dry-run` は apply path と同じ remove validation と target resolution を通したうえで、file write を行わず preview を返してよい。

- bare `--dry-run` は source file を target とする preview として扱ってよい
- `--out <output.ssd> --dry-run` は output path を target file とする preview として扱ってよい
- preview は remove selector、source profile、result profile、preserved source paths、write target、change count を含んでよい

### 4. option matrix stays narrow

この slice では option surface を次に限定してよい。

- `ssd remove <selector> <file>`
- `ssd remove <selector> --cascade <file>`
- `ssd remove <selector> --dry-run <file>`
- `ssd remove <selector> --stdout <file>`
- `ssd remove <selector> --out <output.ssd> <file>`
- `ssd remove <selector> --out <output.ssd> --dry-run <file>`
- `ssd remove type:<kind> --allow-multi <file>`
- `ssd remove type:<kind> --allow-multi --cascade <file>`
- 上記の `type:<kind> --allow-multi` surface に対しても `--dry-run`、`--stdout`、`--out <output.ssd> [--dry-run]` を末尾に付けてよい

`--stdout` は `--dry-run` や `--out` と併用してはならない。
option reordering や sidecar output profile は導入しない。

### 5. alias protection stays strict

`--out` は既知 source を alias してはならない。

- source `.ssd` を指す `--out` は failure とする
- paired `.ssm` を持つ入力では、その `.ssm` を alias する `--out` も failure とする
- 判定は add / merge / normalize の non-destructive output slice と同じ path normalization 規則を再利用してよい

## Consequences

- remove も update command として dry-run / stdout / out parity を持てる
- paired input に対する remove result は inline-only に固定され、sidecar output pair や format selection を reopen せずに済む
- acceptance では metadata remove、single-target structural remove、cascade remove、multi-target cascade remove の non-destructive cases を同じ fixture family で固定できる

## Alternatives Considered

### `--dry-run` だけを先に追加する

却下。
preview だけだと pipeline 接続と別ファイル書き出しが残り、update command 間の non-destructive parity が中途半端に残る。

### paired input では stdout / out も `.ssd` + `.ssm` pair を生成する

却下。
remove に output profile selection と pair write contract を持ち込むと、この slice より大きくなる。

### metadata remove を stdout / out から除外する

却下。
remove 後の semantic result を canonical inline view へ揃えれば narrow に統一でき、kind ごとの output rule を増やさずに済む。

## Validation

- requirements.md と requirements.llmthink.dsl を remove non-destructive output slice に同期する
- docs/cli.ebnf と remove help / grammar help / root help goldens を更新する
- success manifest に remove `--dry-run`、`--stdout`、`--out`、`--out --dry-run` の cases を追加する
- failure manifest に `--stdout` conflict と `--out` alias conflicts を追加する
- CLI 実装で parse、preview、inline render / out write path を追加し、runner を通す

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