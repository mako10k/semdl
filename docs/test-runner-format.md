# SEMDL Test Runner Format

SEMDL の CLI 受け入れテストと golden test は、機械的に実行しやすいように JSON manifest で管理する。

## 目的

- 入力ファイル、実行コマンド、期待終了コード、期待 stdout、期待 stderr を 1 件ごとに固定する
- 成功系と失敗系を同じ構造で扱う
- 将来の test runner 実装が docs から直接ケースを収集できるようにする

## manifest 形式

ファイルは UTF-8 の JSON とし、トップレベルは object、`cases` は array とする。

各 case object は最低限以下のキーを持つ。

- `id`
  - テストケース識別子
- `command`
  - 実行コマンド文字列
- `cwd`
  - 実行時の基準ディレクトリ
- `expected_exit`
  - 期待終了コード
- `expected_stdout`
  - 期待 stdout ファイルの相対パス。不要なら空文字列でもよい
- `expected_stderr`
  - 期待 stderr ファイルの相対パス。不要なら空文字列でもよい
- `notes`
  - 人間向け補足

## 例

```json
{
  "cases": [
    {
      "id": "check-minimal",
      "command": "ssd check docs/examples/minimal.ssd",
      "cwd": ".",
      "expected_exit": 0,
      "expected_stdout": "docs/examples/golden/check-minimal.stdout",
      "expected_stderr": "",
      "notes": "sidecar auto-discovery succeeds"
    }
  ]
}
```

## 運用ルール

- 成功系は `expected_exit = 0` とする
- 失敗系は非 0 とし、stderr golden を必須にする
- dry-run 系は stdout にのみ差分予定を固定し、ファイル実体変更は期待しない
- merge の成功系は stdout の期待値として `minimal.inline.ssd` を参照してよい
- breaking change 時は manifest と golden を同一コミットで更新する
