# SEMDL Test Runner Format

SEMDL の CLI 受け入れテストと golden test は、機械的に実行しやすいように JSON manifest で管理する。

## 目的

- 入力ファイル、実行コマンド、期待終了コード、期待 stdout、期待 stderr を 1 件ごとに固定する
- 成功系と失敗系を同じ構造で扱う
- 将来の test runner 実装が docs から直接ケースを収集できるようにする

## manifest 形式

ファイルは UTF-8 の JSON とし、トップレベルは object、`cases` は array とする。

トップレベル object に `cases` キーが存在しない manifest は不正とし、runner 自体のエラーとする。

各 case object は最低限以下のキーを持つ。

- `id`
  - テストケース識別子
- `command`
  - 実行コマンド文字列
- `argv`
  - runner がそのままプロセス起動に使えるトークン列
- `cwd`
  - 実行時の基準ディレクトリ
- `stdin`
  - 標準入力の固定文字列。不要なら空文字列
- `environment`
  - 追加環境変数。不要なら空 object
- `expected_exit`
  - 期待終了コード
- `expected_stdout`
  - 期待 stdout ファイルの相対パス。不要なら空文字列でもよい
- `expected_stderr`
  - 期待 stderr ファイルの相対パス。不要なら空文字列でもよい
- `expected_output_kind`
  - `exact`、`empty`、`fixture-file` のいずれか
- `notes`
  - 人間向け補足

## 例

```json
{
  "cases": [
    {
      "id": "check-minimal",
      "command": "ssd check docs/examples/minimal.ssd",
      "argv": ["ssd", "check", "docs/examples/minimal.ssd"],
      "cwd": ".",
      "stdin": "",
      "environment": {},
      "expected_exit": 0,
      "expected_stdout": "docs/examples/golden/check-minimal.stdout",
      "expected_stderr": "",
      "expected_output_kind": "exact",
      "notes": "sidecar auto-discovery succeeds"
    }
  ]
}
```

## 運用ルール

- 成功系は `expected_exit = 0` とする
- 失敗系は非 0 とし、stderr golden を必須にする
- parser / lexer 系ケースは `command` だけでなく `argv` を正とし、shell 依存の再解釈を避ける
- `stdin` と `environment` は空でも明示し、runner の実行条件差を減らす
- `expected_output_kind = exact` は golden と完全一致、`empty` は空出力、`fixture-file` は既存 fixture を期待値として読むことを表す
- dry-run 系は stdout にのみ差分予定を固定し、ファイル実体変更は期待しない
- merge の成功系は stdout の期待値として `minimal.inline.ssd` を参照してよい
- breaking change 時は manifest と golden を同一コミットで更新する
