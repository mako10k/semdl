# SEMDL

SEMDL は、自然言語文書や非構造データから抽出した意味的構造を、人間が読みやすく機械が検証しやすい形で保存するための記述系とツール群です。

このリポジトリは spec-first で運用されており、要求仕様、ADR、grammar、acceptance artifacts を先に固定し、その後に CLI と editor integration を実装します。

## Scope

- `.ssd`: semantic document format
- `.ssm`: sidecar metadata format
- `.ssq`: query format
- `ssd`: validate, extract, search, explain, transform, and update CLI
- VS Code extension for `.ssd`, `.ssm`, and `.ssq`

## Authoritative Docs

- [docs/requirements.md](docs/requirements.md): product requirements and behavior source of truth
- [docs/adr/README.md](docs/adr/README.md): ADR process and architecture decision rules
- [docs/cli.ebnf](docs/cli.ebnf): CLI grammar artifact
- [docs/test-runner-format.md](docs/test-runner-format.md): test manifest format
- [docs/test-runner-contract.md](docs/test-runner-contract.md): test runner execution contract
- [editors/vscode/README.md](editors/vscode/README.md): VS Code extension development notes

## Repository Layout

- [docs](docs): requirements, ADRs, grammar, examples, and acceptance assets
- [src](src): CLI, core library, and runner implementation
- [include](include): public headers for the C++ implementation
- [editors/vscode](editors/vscode): VS Code extension and TypeScript language server

## Build

```sh
cmake -S . -B build
cmake --build build
./build/ssd help
```

## Install

The public CLI command can be installed with CMake:

```sh
cmake -S . -B build
cmake --build build
cmake --install build --prefix "$HOME/.local"
```

This installs `ssd` to `$HOME/.local/bin/ssd` in the example above.

## VS Code Extension

The repository also contains a first-party VS Code extension and TypeScript language server for `.ssd`, `.ssm`, and `.ssq`.
See [editors/vscode/README.md](editors/vscode/README.md) for local build and packaging steps.

## License

This repository is licensed under the Mozilla Public License 2.0. See [LICENSE](LICENSE).