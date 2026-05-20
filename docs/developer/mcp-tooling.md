# MCP Tooling Architecture

## Goal

SEMDL の MCP、VS Code language model tools、TypeScript LSP、C++ binary の責務を分離しつつ、Tool schema と監査元情報の drift を防ぐ。

## Boundary

- C++ binary: CLI semantics、validation semantics、将来の audit raw data source of truth
- Node shared layer: tool contract loading、schema reuse、CLI proxy、transport adaptation
- VSIX: language model tool registration と VS Code specific UX
- MCP server: tool invocation transport
- TypeScript LSP: editor protocol adapter。監査元情報は将来 binary 経由へ移す

## First Slice Tool Inventory

first slice で公開する tool は read-only に限定する。

- `check_semdl_document`: `ssd check <document>` を proxy する
- `explain_semdl_entity`: `ssd explain <id> <document>` を proxy する
- `search_semdl_query`: `ssd search <query> <document>` を proxy する
- `read_semdl_help`: `ssd help ...` を proxy する

これらは VS Code language model tools と MCP tools で同一名・同一 input schema を共有する。

## Source Of Truth

Tool の名前、表示名、LLM 向け説明、入力 schema は 1 つの shared catalog から供給する。
VS Code の `contributes.languageModelTools` はその catalog から同期生成する。

Node 側 convenience の都合で package.json と runtime registration の schema が別管理になることは許容しない。

## LSP Transition

現行 LSP は TypeScript analyzer を直接持つ。
ただし、将来の target state は次である。

- Node 側 provider interface が LSP に分析結果を供給する
- provider は binary-native audit contract を優先する
- TypeScript analyzer は binary contract 導入までの fallback に留める

## Migration Phases

### Phase 1

- requirements / ADR を更新する
- shared tool catalog を追加する
- Node CLI proxy を追加する
- VS Code language model tools を登録する
- stdio MCP server を追加する
- LSP に provider boundary を追加する

### Phase 2

- C++ binary に machine-readable audit command を追加する
- Node provider を binary audit source 優先へ切り替える
- diagnostics / symbols の元情報を binary 由来へ寄せる

### Phase 3

- mutation tool を段階導入する
- confirmation policy と side-effect boundary を tool ごとに定義する
- MCP / VSIX 共通の error normalization と richer structured output を追加する

## Non-Goals For Phase 1

- hosted MCP endpoint
- non-VS Code editor plugin の first-party 実装
- mutation tool の一括導入
- Node 側独自 semantics の定義