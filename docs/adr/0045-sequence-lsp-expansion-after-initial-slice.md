# 0045 Sequence LSP Expansion After Initial Slice

- Status: Accepted
- Date: 2026-05-21

## Context

SEMDL の initial VS Code integration は、diagnostics と top-level document symbols に絞った narrow slice として導入済みである。
同時に、accepted ADR 0044 では、LSP の監査元情報を将来的に binary から Node adapter 経由で取得できるよう、analysis provider boundary を先に作る target architecture を採用している。

ここで richer LSP feature を足したいとしても、transitional な TypeScript analyzer fallback に hover、completion、rename、formatting などを直接積み上げると、editor-only semantics と移行コストが同時に増えやすい。
また richer feature の中でも、grammar artifact から機械的に導出しやすいものと、stable symbol identity、cross-file audit source、canonical formatting contract のような未確定契約を必要とするものが混在している。

今回必要なのは、「LSP を充実させる」という曖昧な要求を、source-of-truth と境界条件に従った実装順序へ分解して、先に着手してよい slice と後続決定が必要な slice を固定することである。

## Decision

### 1. introduce the analysis provider boundary before new richer feature surfaces

richer LSP feature を追加する前に、現行 diagnostics / symbols が analysis provider boundary を介して供給される形へ先に寄せる。

- TypeScript analyzer は fallback として残してよい
- fallback は transitional implementation に留め、source of truth と見なしてはならない
- provider boundary は binary-native audit source へ切り替えるための前提 slice として扱う

### 2. prioritize richer diagnostics as the first richer LSP slice

provider boundary 導入後の最初の richer LSP slice は richer diagnostics にしてよい。

- 追加してよい情報は existing parser / validator semantics から導出できるものに限る
- severity mapping、range 精度、related information などの改善は許容してよい
- expected diagnostic allow rule の publish-layer boundary は維持し、suppress policy を analyzer の semantics に混ぜてはならない

### 3. split completion into keyword completion and identifier completion

completion は 1 つの feature として一括導入してはならない。

- grammar artifact から導出できる keyword completion は先に導入してよい
- local identifier completion は、単一文書内の列挙対象、可視範囲、参照可能条件を acceptance として固定してから導入してよい
- cross-file identifier completion は workspace-wide index や cross-file audit source が揃うまで必須にしない

### 4. gate hover on an explicit source-of-truth contract

hover は source-of-truth を固定する前に導入してはならない。

- hover text の正本は requirements、grammar artifact、CLI / core behavior のどこから導出するかを先に決める
- editor convenience のために Node や LSP 側で説明文を独自定義してはならない
- hover は keyword completion より後の slice として扱ってよい

### 5. defer refactoring and cross-file features until the missing contracts are explicit

次の feature は、必要な契約が明示されるまで deferred のままにしてよい。

- hover
- definition
- references
- rename
- formatting
- code actions
- workspace-wide index

これらは少なくとも次のいずれかを必要とする。

- stable symbol identity
- cross-file audit data source
- canonical formatting contract
- mutation safety rule

## Consequences

- richer LSP を追加しても、transitional TypeScript analyzer 依存を深めにくくなる
- keyword completion のような grammar-derived feature を、hover や rename より先に小さく閉じられる
- refactoring や cross-file feature を、未定義契約のまま local convenience で実装する圧力を抑えられる
- diagnostics、completion、hover、refactoring を別 slice として acceptance 設計しやすくなる

## Alternatives Considered

### add hover and completion directly on the local TypeScript analyzer before the provider boundary

却下。
0044 の target architecture に逆行し、transitional fallback に richer semantics を蓄積してしまう。

### treat all completion as one feature and ship keywords and identifiers together

却下。
keyword completion は grammar 由来で閉じやすい一方、identifier completion は scope と eligibility を決めないと editor-only rule divergence になりやすい。

### start with rename or formatting because they are visible editor features

却下。
rename には stable symbol identity と cross-file reference resolution、formatting には canonical formatting contract が必要であり、現時点では acceptance boundary が弱い。

## Validation

- requirements.md と requirements.llmthink.dsl に LSP expansion ordering と defer 条件を同期する
- TypeScript LSP に analysis provider boundary を current diagnostics / symbols surface へ先行導入する
- provider boundary 導入後に richer diagnostics を first richer slice として設計・検証する
- keyword completion、identifier completion、hover を別 slice として acceptance を切る
- semantics-code-reviewer で sequencing と deferred boundary を確認する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/adr/0041-introduce-vscode-extension-and-typescript-lsp-scope.md
- docs/adr/0044-add-first-party-mcp-proxy-and-shared-tool-contracts.md
- docs/developer/mcp-tooling.md
- editors/vscode/server/src/analysisProvider.ts