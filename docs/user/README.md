# User Documentation

このディレクトリは SEMDL 利用者向けの入口です。
形式仕様、ADR、リポジトリ運用ルールではなく、まず使い始めるための情報をまとめます。

## Start Here

- [../../README.md](../../README.md): 最短の導入と CLI quick start
- [vscode.md](vscode.md): VS Code で `.ssd` / `.ssm` / `.ssq` を扱う
- [mcp.md](mcp.md): 互換 MCP サーバを IDE / CLI へ接続する

## Common Tasks

- CLI の入口を見る: `ssd help`
- 文書を検証する: `ssd check <file>`
- エンティティを確認する: `ssd explain <id> <file>`
- query を試す: `ssd search <query.ssq> <file.ssd>`

## Examples

- [../examples](../examples): examples, fixtures, and golden assets
- [../examples/minimal.ssd](../examples/minimal.ssd): 最小の `.ssd` サンプル
- [../query/example.ssq](../query/example.ssq): 最小の `.ssq` サンプル

## When You Need The Formal Spec

利用者向けガイドで不足する厳密な定義は次を参照してください。

- [../requirements.md](../requirements.md)
- [../cli.ebnf](../cli.ebnf)
- [../ssd.ebnf](../ssd.ebnf)
- [../ssq.ebnf](../ssq.ebnf)