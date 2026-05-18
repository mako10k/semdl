# Pitfall Prevention Workflow

この文書は仕様の正本ではない。既存の正本 artifact に反映すべき失敗を、再利用可能な再発防止チェックへ変換するための運用オーバーレイとして使う。

## Purpose

- create_pitfall_case 系ツールが使える場合は、それを優先して事例を検索、登録、補強する
- ツールが使えない環境でも、repo 内に検索可能な seed case と再発防止手順を残す
- pitfall 記録を事後メモで終わらせず、requirements、golden、manifest、help など既存正本 artifact の更新漏れ防止へ接続する

## Scope

対象は次に限定する。

- authoritative artifact の更新漏れを実際に検出した失敗
- 同型の見落としが再発しそうな multi-artifact change
- spec-first の作法を破ると acceptance drift が起きる変更

対象外。

- 単発 typo
- 一度きりの実装バグで、再発防止が特定の artifact check に落ちないもの
- 仕様変更や CLI 契約変更そのものの定義

## Tool-First Flow

1. 変更開始前に `search_pitfall_precautions` で planned work に近い注意点を引く
2. 類似失敗がありそうなら `search_pitfall_cases` で既存ケースを探す
3. 実失敗を検出したら `create_pitfall_case` で新規 case を作る
4. case id が得られたら `append_pitfall_guidance` で recovery と avoidance を足す
5. pitfall から導かれる repo actions を、既存正本 artifact の更新に落とす

推奨 tags:

- `semdl`
- `spec-first`
- `golden-tests`
- `acceptance-drift`
- `cli-help`
- `requirements-sync`

## Fallback When Pitfall Tools Are Unavailable

pitfall ツールが domain 解決に失敗する環境では、次を最低限の代替とする。

1. この文書の Local Pitfall Catalog に事例を追加する
2. AGENTS の起動条件に照らして、次回の事前確認へ組み込む
3. 再利用価値が高い短い注意点だけを repo memory に残す
4. 失敗の修正先は必ず既存正本 artifact に戻す

## Converting A Pitfall Into Repo Actions

pitfall は新しい正本を作るためではなく、既存正本への更新漏れを減らすために使う。少なくとも次を確認する。

文書間に矛盾があるときの判断規律:

- まず優先順位を固定する。requirements と accepted ADR を最上位、その次に grammar と contract artifacts、その次に manifest と golden、その次に examples / fixtures / help snapshots / current implementation を置く。
- 下位 artifact の現状一致は、上位 artifact を覆す根拠にしてはならない。
- 下位 artifact が広く同じ形を取っていても、それは「正しい」の証拠ではなく、「系統的に drift している」可能性の証拠として扱う。
- 方針変更を採るなら、ユーザー承認または明示決定を得たうえで、先に上位 artifact を更新し、その変更に lower artifacts を追従させる。
- 文章レイヤーの判断では、「現物がこうなっているから」ではなく、「どの上位文書がこの surface を支配しているか」を明記してから結論を出す。

| Failure Shape | Required Checks |
| --- | --- |
| CLI / help text changed | [docs/examples/golden/help-root.stdout](docs/examples/golden/help-root.stdout), [docs/examples/golden/help-root.semdl.stdout](docs/examples/golden/help-root.semdl.stdout), [docs/examples/golden/help-root-alias.semdl.stdout](docs/examples/golden/help-root-alias.semdl.stdout), command-specific help golden |
| New CLI behavior or result contract | [docs/requirements.md](docs/requirements.md), [docs/requirements.llmthink.dsl](docs/requirements.llmthink.dsl), related examples, manifest, golden |
| Runner-visible success/failure output changed | [docs/examples/testcases/cli-success.json](docs/examples/testcases/cli-success.json) or [docs/examples/testcases/cli-failure.json](docs/examples/testcases/cli-failure.json), matching stdout or stderr golden |
| Architecture boundary changed | ADR 要否を [docs/adr/README.md](docs/adr/README.md) で判定 |

常に根拠として参照する文書:

- [docs/requirements.md](docs/requirements.md)
- [docs/test-runner-format.md](docs/test-runner-format.md)
- [docs/adr/README.md](docs/adr/README.md)

## Operating Loop

1. planned work を 1 文で書く
2. pitfall ツールが使えれば precaution search、使えなければ Local Pitfall Catalog を読む
3. 実装や仕様更新を行う
4. 失敗が authoritative artifact の更新漏れなら case 化する
5. failure shape を上の表で repo actions に変換する
6. 変更後に focused validation を実行する
7. 再利用性が高ければ AGENTS または repo memory を補強する

## Local Pitfall Catalog

### LOCAL-PITFALL-0001 Help Text Drift After Multi-Artifact Change

- Status: active
- Date: 2026-05-15
- Tags: `semdl`, `spec-first`, `cli-help`, `golden-tests`, `acceptance-drift`
- External tool status: backfilled to pitfallcounter case `id-39bdd1a1-19e2af05d9f` after domain recovery was verified on 2026-05-15
- Primary text: CLI help 文言や search 機能を更新したが、root help 系 golden の同期を忘れて runner が失敗した。command-specific help だけ直しても、root help と alias help が別 surface として残る。
- Impact: 実装本体は正しくても acceptance が落ち、修正完了判定が遅れる。
- Detection signal: full runner で root help または help alias の stdout mismatch が出る。
- Recovery:
  - 実 help 出力と golden の diff を取る
  - [docs/examples/golden/help-root.stdout](docs/examples/golden/help-root.stdout), [docs/examples/golden/help-root.semdl.stdout](docs/examples/golden/help-root.semdl.stdout), [docs/examples/golden/help-root-alias.semdl.stdout](docs/examples/golden/help-root-alias.semdl.stdout) を同期する
  - command-specific help golden も同時に見直す
- Avoidance:
  - CLI help 文言、search capability、reference help を触る変更では、実装前または実装直後に root help 群と command-specific help 群を一括確認する
  - requirements の「CLI 機能追加時は受け入れ例または golden を同時追加」と test-runner-format の「breaking change 時は manifest と golden を同一変更で更新」を checklist に入れる
- Required repo actions:
  - [docs/requirements.md](docs/requirements.md)
  - [docs/examples/golden/help-root.stdout](docs/examples/golden/help-root.stdout)
  - [docs/examples/golden/help-root.semdl.stdout](docs/examples/golden/help-root.semdl.stdout)
  - [docs/examples/golden/help-root-alias.semdl.stdout](docs/examples/golden/help-root-alias.semdl.stdout)
  - [docs/examples/golden/search-help.stdout](docs/examples/golden/search-help.stdout)
  - [docs/examples/golden/search-help.semdl.stdout](docs/examples/golden/search-help.semdl.stdout)
  - [docs/examples/testcases/cli-success.json](docs/examples/testcases/cli-success.json)
