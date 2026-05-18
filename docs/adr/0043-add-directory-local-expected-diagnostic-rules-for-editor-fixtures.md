# 0043 Add Directory-Local Expected Diagnostic Rules For Editor Fixtures

- Status: Accepted
- Date: 2026-05-18

## Context

SEMDL の initial VS Code integration は editor-side diagnostics を提供するが、`docs/query/invalid-*.ssq` や
failure fixture 用の invalid `.ssd` のように、repo 内には意図的に invalid な artifact も存在する。

これらを editor diagnostics から外すために fixture を `.txt` へ退避すると、CLI / requirements / golden が指す
primary artifact path と人手編集 path が分離し、artifact hygiene が悪化する。

一方で、invalid fixture を無条件に workspace-wide suppress すると、editor integration が広範囲のエラー許容を
既定化してしまい、requirements が求める adapter surface としての安全側既定に反する。

必要なのは、core / CLI semantics を変えずに、editor diagnostics だけへ局所的な expected-error allow rule を
与える narrow policy surface である。

## Decision

### 1. add a directory-local editor diagnostics policy file

initial slice では、expected diagnostic allow rule は対象ファイルと同一ディレクトリに置く
directory-local config file で定義してよい。

- config file 名は `.semdl-diagnostics.json` としてよい
- language server は、診断対象ファイルの所属ディレクトリだけを見て同名 file を探索してよい
- parent directory への継承、merge、workspace-global default はこの slice に含めない

### 2. keep the allow rule narrow and explicit

allow rule は explicit file-local match に限定してよい。

- rule は config file と同じ directory 内の file 名だけを指定してよい
- rule は exact file name、1-based line number、exact diagnostic message の三点一致でのみ適用してよい
- wildcard、glob、directory pattern、message prefix match、severity-only allow はこの slice に含めない
- config に broad rule が書かれていても language server は無視してよい

### 3. keep analyzer semantics unchanged

expected diagnostic allow rule は publish-layer policy として扱ってよい。

- analyzer は従来どおり full diagnostics を返してよい
- filter は language server が diagnostics を publish する直前にだけ適用してよい
- document symbols や他の LSP surface は suppress 対象にしない
- core / CLI behavior、requirements、grammar の source of truth は変えない

### 4. defer sidecar-based expected-diagnostic declarations

sidecar に expected diagnostic allow rule を書く案は後続 slice に分離してよい。

- initial slice では `.ssd` / `.ssq` / `.ssm` の core format responsibility を広げない
- sidecar support が必要になった時点で別 ADR として扱ってよい

## Consequences

- repo 内の invalid fixture は primary extension のまま保持できる
- editor diagnostics は narrow config がある file だけ局所的に quiet にできる
- broad suppress default を導入せずに `get_errors` と editor view を clean に保ちやすくなる
- config discovery は same-directory only なので、意図しない上位ディレクトリ汚染を避けやすい

## Alternatives Considered

### rename invalid fixtures to `.txt`

却下。
diagnostics は消せるが、CLI acceptance surface が本来の language extension から外れ、artifact hygiene が悪い。

### support sidecar-based expected-diagnostic declarations first

却下。
core format responsibility と editor-only policy surface が混ざり、initial slice が広がる。

### add workspace-wide suppression defaults

却下。
広範囲のエラー許容が既定になるため、安全側既定に反する。

## Validation

- requirements.md と requirements.llmthink.dsl に directory-local expected diagnostic rule の制約を同期する
- invalid fixture directory に `.semdl-diagnostics.json` を追加する
- language server test で config なし、exact match allow、broad rule ignored、non-matching diagnostics retained を確認する
- semantics-code-reviewer で editor-only policy と adapter boundary を確認する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- editors/vscode/server/src/server.ts
- editors/vscode/server/src/analyzer.ts
- editors/vscode/README.md