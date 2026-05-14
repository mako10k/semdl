# ADR 運用

SEMDL のアーキテクチャ判断は Architecture Decision Record で管理する。

## 目的

- 要求仕様と実装方針のあいだにある設計判断を分離して残す
- 将来の format、CLI、parser、sidecar、test runner の変更理由を追跡できるようにする
- requirements.md と golden test だけでは表しにくい採否理由と代替案を固定する

## 対象

以下の変更は ADR を必須とする。

- .ssd、.ssm、.ssq の責務境界変更
- CLI サブコマンド、selector、競合解決規則の変更
- parser / validator / runner の公開契約変更
- ライブラリ層と CLI 層の責務変更
- 既存 ADR を覆す設計変更

単なる typo 修正、説明改善、golden の非意味的整形だけでは ADR を必須としない。

## 置き場所と命名

- 置き場所: docs/adr/
- ファイル名: 4 桁連番 + kebab-case の要約
- 例: 0001-use-architecture-decision-records.md

## ステータス

各 ADR は少なくとも以下のいずれかを持つ。

- Proposed
- Accepted
- Superseded
- Deprecated

Superseded の場合は置き換え先 ADR を明記する。

## 必須項目

- Title
- Status
- Date
- Context
- Decision
- Consequences
- Alternatives Considered
- Validation
- Related Artifacts

## 運用ルール

- 仕様を確定させる前に ADR を Proposed で作る
- 合意後に Accepted へ更新する
- requirements.md、requirements.llmthink.dsl、golden test、EBNF への影響がある場合は同一変更で更新する
- 既存 ADR と矛盾する変更は、新 ADR を作って supersede する
- ADR 番号は再利用しない

## 初期方針

- ADR は docs/requirements.md を置き換えない
- 要求仕様は what、ADR は why と boundary を中心に書く
- 実装開始前に architecture へ影響する判断は Accepted まで持っていく
