# Terminology Inventory

この文書は、SEMDL の用語を正式制定する glossary ではなく、
contributors が用語整理を進めるための棚卸しです。

- 対象は、既存の明示定義がない語、複数文書で意味や表示名が揺れている語、主要仕様で前提語として使われている語に絞る
- 使用箇所は全文検索の羅列ではなく、authoritative docs と利用者向け入口の代表箇所だけを挙げる
- `.md` が正本であり、この文書自体も inventory であって glossary の最終版ではない

## Selection Rule

この inventory では、各用語を次の 3 つに分ける。

- `existing-definition`: すでに requirements に主要定義があり、呼び出し側の用語統一だけを見ればよい
- `needs-definition`: 使われているが、定義文または責務境界の明示が不足している
- `needs-normalization`: 概念自体は見えているが、表示名や略称が複数あり統一が必要

## Existing Definitions In Requirements

| Term | Status | Primary definition source | Representative usage locations |
| --- | --- | --- | --- |
| SEMDL | existing-definition | [../requirements.md](../requirements.md) 1.目的 | [../../README.md](../../README.md), [../user/README.md](../user/README.md) |
| resource / リソース | existing-definition | [../requirements.md](../requirements.md) 4.1 | [../requirements.md](../requirements.md), [../examples/minimal.ssd](../examples/minimal.ssd) |
| segment / セグメント | existing-definition | [../requirements.md](../requirements.md) 4.2 | [../requirements.md](../requirements.md), [../examples/minimal.ssd](../examples/minimal.ssd) |
| assertion / 主張 | existing-definition | [../requirements.md](../requirements.md) 4.3 | [../requirements.md](../requirements.md), `ssd explain` help examples |
| qualified assertion / 修飾付き主張 | existing-definition | [../requirements.md](../requirements.md) 4.4 | [../requirements.md](../requirements.md) |
| provenance / 出所情報 | existing-definition | [../requirements.md](../requirements.md) 4.5 | [../requirements.md](../requirements.md), ADR 群の metadata / annotation 文脈 |
| hypothesis / 仮説 | existing-definition | [../requirements.md](../requirements.md) 4.6 | [../requirements.md](../requirements.md), remove / annotate 系 ADR |
| alternative / 代替案 | existing-definition | [../requirements.md](../requirements.md) 4.6 | [../requirements.md](../requirements.md), query / remove 系 examples |
| embedding / 埋め込み | existing-definition | [../requirements.md](../requirements.md) 4.7 | [../requirements.md](../requirements.md), extract / similarity 関連 ADR |

## Canonical Terms Adopted In This Slice

| Term | Status | Why it should be defined or normalized | Representative usage locations |
| --- | --- | --- | --- |
| `.ssd` | adopted | 正式表示名を SEMDL Canvas に固定し、primary semantic document として扱う | [../requirements.md](../requirements.md) 2.1, [../../README.md](../../README.md), [../../editors/vscode/package.json](../../editors/vscode/package.json), [../../editors/vscode/syntaxes/semdl-ssd.tmLanguage.json](../../editors/vscode/syntaxes/semdl-ssd.tmLanguage.json) |
| `.ssm` | adopted | 正式表示名を SEMDL Sidekick に固定し、companion metadata layer として扱う | [../requirements.md](../requirements.md) 2.1, [../../README.md](../../README.md), [../../editors/vscode/package.json](../../editors/vscode/package.json), [../../editors/vscode/syntaxes/semdl-ssm.tmLanguage.json](../../editors/vscode/syntaxes/semdl-ssm.tmLanguage.json) |
| `.ssq` | adopted | 正式表示名を SEMDL Lens に固定し、semantic query surface として扱う | [../requirements.md](../requirements.md) 2.1, [../../README.md](../../README.md), [../../editors/vscode/package.json](../../editors/vscode/package.json), [../../editors/vscode/syntaxes/semdl-ssq.tmLanguage.json](../../editors/vscode/syntaxes/semdl-ssq.tmLanguage.json) |
| Solo mode | adopted | inline profile の新しい display-layer term とし、単一 Canvas に構造と補助情報を収めるモードを指す | [../requirements.md](../requirements.md) 5.2, extract / normalize / set 関連 ADR |
| Companion mode | adopted | sidecar profile の新しい display-layer term とし、Canvas と Sidekick を組みにするモードを指す | [../requirements.md](../requirements.md) 5.2-5.4, normalize / extract / metadata 関連 ADR |
| sidecar | retained-technical | sibling companion file や ownership boundary を指す技術用語として残す | [../requirements.md](../requirements.md) 2.1, ADR 群の write ownership 文脈 |
| first-party VS Code extension | needs-definition | product surface、配布 artifact、adapter boundary のどれを指すかを固定したい | [../requirements.md](../requirements.md) 2.スコープ, [../../README.md](../../README.md), [README.md](README.md) |
| TypeScript language server | needs-definition | editor protocol adapter、analyzer 実装、将来の binary-fed provider の区別が必要 | [../requirements.md](../requirements.md) 3.5, [mcp-tooling.md](mcp-tooling.md) |
| first-party MCP server | needs-definition | hosted endpoint ではなく local stdio Node server first slice であることを明示したい | [../requirements.md](../requirements.md) 2.スコープ, [../user/mcp.md](../user/mcp.md), [mcp-tooling.md](mcp-tooling.md) |
| language model tool | needs-definition | VS Code host API の tool と SEMDL tool contract の関係を明示したい | [../requirements.md](../requirements.md) 3.5, [mcp-tooling.md](mcp-tooling.md), [../../editors/vscode/tooling/semdl-tool-contracts.json](../../editors/vscode/tooling/semdl-tool-contracts.json) |
| Tool inventory | needs-definition | catalog of public tools なのか、registration set なのか、acceptance surface なのかを固定したい | [../requirements.md](../requirements.md) 3.5, [mcp-tooling.md](mcp-tooling.md) |
| input schema source of truth | needs-definition | package metadata、runtime validation、binary contract との関係が重要だが、用語としての定義がまだ薄い | [../requirements.md](../requirements.md) 3.5, [mcp-tooling.md](mcp-tooling.md), ADR 0044 |
| binary audit contract | needs-definition | LSP migration の target source を指す重要語だが、具体的な command / payload surface は未定義 | [../requirements.md](../requirements.md) 3.5, [mcp-tooling.md](mcp-tooling.md), [../user/mcp.md](../user/mcp.md) |
| local analyzer fallback | needs-definition | temporary implementation detail なのか supported architecture term なのかを切り分けたい | [../requirements.md](../requirements.md) 3.5, [mcp-tooling.md](mcp-tooling.md) |
| read-only tool | needs-definition | first slice scope termとして使われているが、許容される side effect と output surface の境界を持たせたい | [../requirements.md](../requirements.md) 3.5, [mcp-tooling.md](mcp-tooling.md), [../user/mcp.md](../user/mcp.md) |
| mutation tool | needs-definition | read-only tool との対になる phase term だが、まだ境界条件が固定されていない | [../requirements.md](../requirements.md) 3.5, [mcp-tooling.md](mcp-tooling.md) |

## Protected Identifiers

次は今回の slice で rename しない。

- `.ssd`、`.ssm`、`.ssq` の拡張子
- `ssd` binary 名と subcommand 名
- VS Code language id の `semdl-ssd`、`semdl-ssm`、`semdl-ssq`
- command id、tool id、toolReferenceName、schema field 名、環境変数
- CLI grammar、runner manifest、golden が比較する argv surface

## Suggested Next Glossary Entries

glossary を後続で正式化する場合、最初に定義すべき候補は次の順序がよい。

1. first-party VS Code extension / TypeScript language server / first-party MCP server の責務境界
2. language model tool / Tool inventory / input schema source of truth の関係
3. binary audit contract / local analyzer fallback / read-only tool / mutation tool の migration 用語

## Inventory Notes

- requirements 4.x の概念語はすでに定義の核があるため、まずは glossary へ再掲するかどうかの判断で足りる
- integration 用語は requirements と developer docs で重要度が上がった一方、まだ用語自体の短い定義文がない
- `.ssd` / `.ssm` / `.ssq` は Canvas / Sidekick / Lens に正式表示名を固定し、識別子は保護する方針に整理した