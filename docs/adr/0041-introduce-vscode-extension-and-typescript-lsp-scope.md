# 0041 Introduce VS Code Extension And TypeScript LSP Scope

- Status: Accepted
- Date: 2026-05-18

## Context

SEMDL は current repo shape では CLI と spec artifacts が中心であり、first-party editor integration surface をまだ持っていない。
一方で `.ssd`、`.ssm`、`.ssq` は人手編集される text asset であり、code highlighting と editor-side diagnostics を持たないままでは運用入口が CLI 偏重になる。

ただし editor integration を導入する際に、syntax highlighting、semantic highlighting、validation、navigation、formatting、refactoring を一度に scope へ入れると、実装責務が過剰に広がる。
特に highlighting と language server の責務を最初に切り分けないと、extension host と server process と grammar artifacts の境界が曖昧になる。

今回必要なのは full IDE parity ではなく、SEMDL の first editor slice として VS Code 向け delivery target と initial LSP boundary を固定することである。

## Decision

### 1. add first-party VS Code editor integration to scope

この段階から、SEMDL の first-party editor integration は VS Code extension を対象に含めてよい。

- 対象 language は `.ssd`、`.ssm`、`.ssq` としてよい
- 配布形態は VSIX を前提にしてよい
- non-VS Code editor plugin は引き続き後続 slice に分離してよい

### 2. keep syntax highlighting independent from the language server

initial code highlighting は TextMate grammar を使うことにしてよい。

- syntax highlighting は language server 非起動時でも機能してよい
- initial highlighting は `.ssd`、`.ssm`、`.ssq` の lexical / structural token classification を対象にしてよい
- semantic token を highlight の必須前提にしてはならない

### 3. make the language server a separate TypeScript process

initial LSP は TypeScript 実装にしてよく、extension host から分離した language server process として扱ってよい。

- server implementation language は TypeScript としてよい
- extension host script と language server は責務を分けてよい
- editor runtime convenience のために parser / validator semantics を ad hoc に再定義してはならない

### 4. keep the first LSP slice narrow

initial LSP feature set は narrow に固定してよい。

- parse / validate diagnostics
- top-level document symbols
- `.ssd`、`.ssm`、`.ssq` の basic language identification

この slice では completion、hover、rename、formatting、code action、workspace-wide index、semantic token は必須にしない。

### 5. keep CLI / requirements / grammar as the source of truth

editor integration は adapter surface として扱ってよい。

- SEMDL semantics の source of truth は requirements、grammar artifacts、core / CLI behavior に残してよい
- extension / LSP は existing contracts に準拠または再利用する側とし、editor-only rule divergence を導入しない
- implementation ordering は spec-first を維持し、first slice では boundary と acceptance target を先に固定してよい

## Consequences

- 次の editor implementation slice で VSIX scaffold と TypeScript LSP scaffold に着手しやすくなる
- syntax highlighting と LSP diagnostics を独立に立ち上げられるため、最初の editor surface を小さく閉じられる
- semantic token や richer IDE features は後続 slice として分離しやすくなる

## Alternatives Considered

### syntax highlighting だけを scope に入れて LSP は引き続き defer する

却下。
ユーザー要求に反し、editor integration の責務境界も先送りになる。

### syntax highlighting も semantic token も language server でまとめて扱う

却下。
server 起動に highlighting を依存させると first slice が重くなり、offline / degraded editor surface も失う。

### TypeScript 以外の language server 実装を先に比較検討する

却下。
今回の要求では TS を許容しており、初手で implementation language exploration を広げる合理性が薄い。

## Validation

- requirements.md の scope と design principles に editor integration boundary を追加する
- requirements.llmthink.dsl に highlight と TypeScript LSP の責務分離を同期する
- semantics-code-reviewer で scope widening と initial feature boundary を確認する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl