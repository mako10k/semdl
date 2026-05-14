# SEMDL Test Runner Contract

SEMDL の test runner は manifest を読み、shell を介さずに CLI を実行し、golden と比較する。

## 目的

- manifest 形式とは別に、実行順序と比較意味を固定する
- parser / lexer 系の失敗ケースでも shell 差異を避ける
- 実装が変わっても同じ期待動作を再現できるようにする

## Discovery

runner は少なくとも以下の manifest を明示順で読む。

1. docs/examples/testcases/cli-success.json
2. docs/examples/testcases/cli-failure.json

manifest の追加は許容するが、初期 runner は上記 2 ファイルで完結できること。

## Execution

各 case について runner は次の順で処理する。

1. `cwd` へ移動する
2. `environment` を現在環境へ上書き適用する
3. `stdin` を固定文字列として準備する
4. `argv` をそのまま用いてプロセスを起動する
5. shell 展開、文字列再分割、追加 quoting を行わない
6. exit code、stdout、stderr を収集する

`command` は表示用であり、実行入力としては使わない。

## Comparison

比較順序は次のとおりとする。

1. exit code
2. stdout
3. stderr

`expected_output_kind` の意味は次のとおりとする。

- `exact`
  - 対応する golden ファイルと完全一致する
- `empty`
  - 出力が空文字列である
- `fixture-file`
  - 既存 fixture ファイルを期待値として読む

stdout と stderr はバイト列完全一致を基本とする。
改行正規化や空白のトリムは既定では行わない。

## Failure Handling

- manifest 必須キーが欠けている case は runner 自体のエラーとする
- top-level `cases` キーが欠けている manifest は runner 自体のエラーとする
- `expected_exit` と実 exit code が異なる場合、その時点で case は失敗とする
- `expected_stdout` または `expected_stderr` が空文字列なら、そのストリームは空であることを要求する

## Non-Goals

- shell script 互換のコマンドライン再解釈
- fuzzy match や部分一致による golden 比較
- manifest 自動生成

## Related Artifacts

- docs/test-runner-format.md
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
