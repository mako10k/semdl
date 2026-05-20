# 0044 Add First-Party MCP Proxy And Shared Tool Contracts

- Status: Accepted
- Date: 2026-05-19

## Context

SEMDL はすでに first-party VS Code extension と TypeScript LSP を scope に入れているが、LLM / tool-calling 面では first-party MCP server と VS Code language model tool の責務が未定義だった。

今回の要求では、C++ 実装を基本資産として活かしつつ、Node 側から VSIX の tool registration、MCP の tool invocation、LSP の将来監査入力を束ねたい。
このとき Node 側 convenience で tool schema や監査元情報を再定義すると、VSIX / MCP / LSP / binary の間で drift が起きやすい。

## Decision

### 1. add a first-party Node MCP server to product scope

SEMDL は first-party MCP server を scope に入れてよい。

- first slice は local stdio server で始めてよい
- hosted endpoint は必須にしない
- MCP server は C++ `ssd` binary を proxy する Node adapter として配置してよい

### 2. keep Node as the shared adapter layer

VSIX、MCP server、TypeScript LSP の接続点は Node 層で共有してよい。

- VSIX は VS Code language model tool registration を担当してよい
- MCP server は tool invocation transport を担当してよい
- LSP は Node provider boundary を介して分析結果を受けてもよい
- Node 層は orchestration / transport / schema loading を担当し、SEMDL semantics の正本にはならない

### 3. use one shared tool contract catalog

public tool の名前、説明、input schema は 1 つの shared catalog に固定する。

- VS Code `contributes.languageModelTools` は catalog から同期する
- runtime tool registration も同じ catalog を読む
- MCP tool list も同じ catalog から導出する
- package.json manual copy と runtime manual copy の二重管理を許可しない

### 4. start with read-only tools

first slice の public tool は read-only を優先してよい。

- `check`
- `explain`
- `search`
- `help`

mutation tool は後続 slice に分離してよい。

### 5. make binary audit data the target source for LSP

LSP は引き続き TypeScript process にしてよいが、将来の監査元情報は binary から Node adapter 経由で取得する target architecture にする。

- 初期移行では TypeScript analyzer fallback を残してよい
- fallback は transitional implementation であり、恒久的な source of truth と見なしてはならない
- binary-native audit contract を追加した段階で LSP provider を切り替えられる boundary を先に作ってよい

## Consequences

- VSIX tool と MCP tool の schema drift を抑えやすくなる
- C++ binary を維持したまま Node 側で tool-calling surfaces を増やせる
- LSP を即座に全面書き換えずに、binary audit source への移行路を確保できる
- mutation tool は別 slice へ遅らせるため、最初の MCP 導入を read-only に閉じられる

## Alternatives Considered

### define MCP directly from Node-local tool schemas without updating product scope

却下。
今回の変更は VSIX、MCP、LSP、binary の責務境界を変えるため、requirements / ADR を伴わない local convenience 変更にしてはならない。

### implement MCP as a separate runtime outside the VS Code Node package

却下。
現時点では LSP と VSIX がすでに Node / TypeScript にあり、shared adapter 層を別 runtime に切ると schema と process 管理が先に分岐する。

### wait for a binary-native audit command before adding any MCP or tool surface

却下。
binary audit contract を待つだけでは tool schema と Node adapter boundary が未定義のまま残り、VSIX / MCP 導入を不必要に遅らせる。

## Validation

- requirements.md と requirements.llmthink.dsl に MCP / shared tool contract / binary audit transition を同期する
- shared tool catalog から VS Code `languageModelTools` を同期する仕組みを追加する
- Node CLI proxy と stdio MCP server を first slice として実装する
- TypeScript LSP に analysis provider boundary を追加する
- semantics-code-reviewer で scope widening と source-of-truth 設計を確認する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/developer/mcp-tooling.md
- editors/vscode/tooling/semdl-tool-contracts.json
- editors/vscode/src/mcpServer.ts