# 0001 Use Architecture Decision Records

- Status: Accepted
- Date: 2026-05-14

## Context

SEMDL は .ssd / .ssm / .ssq の責務分担、CLI 更新規則、selector 安全制約、test-first な golden assets など、
実装より前に固定すべき設計判断が多い。
requirements.md だけでは判断理由や代替案の採否が埋もれやすく、
後から parser、runner、format を実装するときに「何を変えてよいか」が不明確になりやすい。

## Decision

SEMDL は architecture に影響する判断を docs/adr/ 以下の ADR で管理する。

初期ルールは次のとおりとする。

- format 境界、CLI 公開契約、selector 規則、runner 契約の変更は ADR 必須
- ADR は 4 桁連番で採番する
- architecture を確定する ADR は Accepted まで更新してから実装へ進む
- requirements.md、requirements.llmthink.dsl、golden test、cli.ebnf に影響する場合は同一コミットで反映する

## Consequences

- requirements.md は要求と期待動作の基準として保ちやすくなる
- なぜその責務分担にしたかを個別ファイルで追える
- 後続の ADR で supersede できるため、変更履歴を壊さずに進められる
- 小さな文言修正まで ADR にすると重くなるため、対象範囲の線引きを維持する必要がある

## Alternatives Considered

### requirements.md に全理由を埋め込む

却下。
要求、理由、代替案、履歴が一つの文書に混ざり、変更差分が読みにくくなる。

### requirements.llmthink.dsl のみを判断記録として使う

却下。
DSL 監査には向くが、人間向けの採否理由や supersede 運用の中心としては薄い。

## Validation

- docs/adr/README.md に運用ルールを固定する
- requirements.md に ADR 参照ルールを追加する
- repo memory に ADR 運用方針を記録する

## Related Artifacts

- docs/adr/README.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
