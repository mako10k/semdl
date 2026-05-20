# MCP Client Guide

このページは、SEMDL の first-party MCP server first slice と、各 IDE / CLI client への接続方法をまとめる利用者向けガイドです。

重要: 現在の first slice は local stdio の Node server です。
hosted endpoint は同梱しておらず、tool execution は `ssd` バイナリ proxy を基本にします。

## Documented Client Examples

このリポジトリで用意するのは、次の client 向けの接続例です。

- VS Code の `mcp.json`
- Claude Desktop の `claude_desktop_config.json`
- Claude Code CLI / project `.mcp.json`

それ以外の IDE や CLI でも、stdio または streamable HTTP を受ける MCP client なら、generic template を起点に調整できる場合があります。

## Security Notes

- local stdio server はローカル実行プロセスなので、信頼できる server だけを起動してください
- API token や secret は config へ直書きせず、client 側の secret / env 機構を優先してください
- project-shared config を version control に入れる場合は、secret を含めないでください

## First-Party Server Entry Point

repo checkout 上で first slice を試す場合の起点は次です。

```sh
cd editors/vscode
npm install
npm run bundle
SEMDL_BINARY=/absolute/path/to/ssd node bundle/mcp-server.js
```

`SEMDL_BINARY` を省略した場合は `ssd` を `PATH` から先に試し、`SEMDL_PROJECT_ROOT` または実行時の workspace root が分かる場合は、その `build/ssd` を次の候補にします。

現在の public tool surface は read-only に限定しています。

- `check_semdl_document`
- `explain_semdl_entity`
- `search_semdl_query`
- `read_semdl_help`

## Template Artifacts

- [../examples/mcp/README.md](../examples/mcp/README.md)
- [../examples/mcp/vscode.mcp.template.jsonc](../examples/mcp/vscode.mcp.template.jsonc)
- [../examples/mcp/claude-desktop.template.jsonc](../examples/mcp/claude-desktop.template.jsonc)
- [../examples/mcp/claude-code.project.template.jsonc](../examples/mcp/claude-code.project.template.jsonc)
- [../examples/mcp/generic-stdio.template.jsonc](../examples/mcp/generic-stdio.template.jsonc)
- [../examples/mcp/generic-http.template.jsonc](../examples/mcp/generic-http.template.jsonc)
- [../examples/mcp/claude-code.commands.sh](../examples/mcp/claude-code.commands.sh)

## VS Code

client schema や file location は version により変わりうるため、最終的には vendor docs を優先してください。

VS Code は workspace の `.vscode/mcp.json` または user profile の `mcp.json` で MCP server を構成できます。
workspace 共有を始めるなら [../examples/mcp/vscode.mcp.template.jsonc](../examples/mcp/vscode.mcp.template.jsonc) を起点にしてください。

使い分けの目安:

- project ごとに共有したい: `.vscode/mcp.json`
- 個人ローカル設定に閉じたい: user profile の `mcp.json`

## Claude Desktop

client schema や config file location は version により変わりうるため、最終的には vendor docs を優先してください。

Claude Desktop は `claude_desktop_config.json` の `mcpServers` で local stdio server を扱います。
標準パスの例:

- macOS: `~/Library/Application Support/Claude/claude_desktop_config.json`
- Windows: `%APPDATA%\Claude\claude_desktop_config.json`

[../examples/mcp/claude-desktop.template.jsonc](../examples/mcp/claude-desktop.template.jsonc) をベースに、実際の server command と args を埋めてください。

## Claude Code CLI

client schema や config file location は version により変わりうるため、最終的には vendor docs を優先してください。

Claude Code では project-shared `.mcp.json` と `claude mcp add` の両方が使えます。

共有設定を repository に置く場合:

- [../examples/mcp/claude-code.project.template.jsonc](../examples/mcp/claude-code.project.template.jsonc)

CLI で追加する場合:

- [../examples/mcp/claude-code.commands.sh](../examples/mcp/claude-code.commands.sh)

Claude Code 側の project-scoped config は repo root の `.mcp.json` を使う前提です。

## Other IDEs And CLI Clients

他の MCP-compatible client では、設定 schema や file location が変わることがあります。
その場合は次の generic templates から始めるのが安全です。

- stdio: [../examples/mcp/generic-stdio.template.jsonc](../examples/mcp/generic-stdio.template.jsonc)
- remote HTTP: [../examples/mcp/generic-http.template.jsonc](../examples/mcp/generic-http.template.jsonc)

調整時の確認点:

- local process 起動か remote URL 接続か
- config file location
- secret / token の渡し方
- project-shared config を許可するか

## Scope Note For Contributors

このガイドはクライアント接続面を整理するためのものです。
SEMDL の first-party MCP surface はまだ staged rollout 中であり、mutation tool や binary-native audit contract は後続 slice です。
開発者向け背景は [../developer/README.md](../developer/README.md) を参照してください。