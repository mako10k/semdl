# SEMDL

SEMDL は、自然言語文書や非構造データから抽出した意味的構造を、人間が読みやすく機械が検証しやすい形で保存するための記述系とツール群です。

現在の公開面は次のとおりです。

- `.ssd`: semantic document format
- `.ssm`: sidecar metadata format
- `.ssq`: query format
- `ssd`: validate, extract, search, explain, transform, and update CLI
- `.ssd` / `.ssm` / `.ssq` 向けの first-party VS Code extension
- Node ベースの first-party MCP server first slice

## Quick Start

SEMDL CLI は現在ソースから build して使う形が最短です。

```sh
cmake -S . -B build
cmake --build build
./build/ssd help
./build/ssd check docs/examples/minimal.ssd
./build/ssd explain A1 docs/examples/minimal.ssd
```

## Install CLI

CLI を prefix 配下へ install したい場合は次を使えます。

```sh
cmake -S . -B build
cmake --build build
cmake --install build --prefix "$HOME/.local"
```

上の例では `$HOME/.local/bin/ssd` が生成されます。

## User Docs

- [docs/user/README.md](docs/user/README.md): 利用者向けドキュメント入口
- [docs/user/vscode.md](docs/user/vscode.md): VS Code 利用ガイド
- [docs/user/mcp.md](docs/user/mcp.md): MCP クライアント接続ガイド
- [docs/examples](docs/examples): CLI examples, fixtures, and golden assets

## MCP Integration

このリポジトリは、first slice として Node ベースの first-party MCP server を持ちます。
この server は C++ の `ssd` バイナリを proxy する形で read-only tool から始め、VS Code language model tools と input schema source of truth を共有します。

- [docs/user/mcp.md](docs/user/mcp.md)
- [docs/examples/mcp/README.md](docs/examples/mcp/README.md)

## Formal Specs And Developer Docs

利用者向け入口から外した仕様・実装・開発者向け情報は次にまとめています。

- [docs/developer/README.md](docs/developer/README.md)
- [docs/requirements.md](docs/requirements.md)
- [docs/adr/README.md](docs/adr/README.md)
- [docs/cli.ebnf](docs/cli.ebnf)
- [docs/test-runner-contract.md](docs/test-runner-contract.md)

## License

This repository is licensed under the Mozilla Public License 2.0. See [LICENSE](LICENSE).