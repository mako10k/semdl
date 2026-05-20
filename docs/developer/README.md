# Developer Documentation

このディレクトリは contributors / maintainers 向けの入口です。
利用者向けの導線は root [../../README.md](../../README.md) と [../user/README.md](../user/README.md) を優先してください。

## Audience

次を行う人向けです。

- 仕様策定や ADR 更新
- CLI / core / runner 実装
- VS Code extension と language server 開発
- MCP server と language model tool 開発
- acceptance artifacts や golden の保守

## Authoritative Documents

- [../requirements.md](../requirements.md): requirements and product behavior source of truth
- [../requirements.llmthink.dsl](../requirements.llmthink.dsl): paired reasoning artifact
- [../adr/README.md](../adr/README.md): ADR rules
- [../cli.ebnf](../cli.ebnf): CLI grammar artifact
- [../test-runner-format.md](../test-runner-format.md): manifest format
- [../test-runner-contract.md](../test-runner-contract.md): runner contract
- [../../AGENTS.md](../../AGENTS.md): repo working rules for coding agents and multi-artifact changes

## Repository Layout

- [../](../): requirements, ADRs, grammar, examples, and plans
- [../../src](../../src): CLI, core library, and runner implementation
- [../../include](../../include): C++ headers
- [../../editors/vscode](../../editors/vscode): VS Code extension and TypeScript language server

## Build And Validation

```sh
cmake -S . -B build
cmake --build build
./build/semdl_runner
```

VS Code extension local development:

```sh
cd editors/vscode
npm install
npm test
npm run bundle
```

## Documentation Split

- 利用者向け入口: root [../../README.md](../../README.md), [../user/README.md](../user/README.md)
- 開発者向け入口: このページ
- 仕様正本: requirements, ADRs, grammar, runner contract artifacts

README に開発事情を寄せすぎず、開発者向け背景や repo mechanics はここから下位文書へ辿れるように保ちます。

## MCP Note

first-party MCP server は Node ベースの adapter surface としてこの repo の scope に入ります。
ただし、first slice は read-only tool と C++ binary proxy を優先し、mutation tool と binary-native audit contract は staged rollout として後続へ分けます。

詳細は [mcp-tooling.md](mcp-tooling.md) を参照してください。