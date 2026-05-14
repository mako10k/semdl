# 0005 Add Opt-In SEMDL Help Rendering

- Status: Accepted
- Date: 2026-05-14

## Context

SEMDL の help は 0004 で CLI 正本として固定されたが、出力は text のみである。

このままだと、人間には読みやすい一方で、外部ツールや後続処理から
help を構造的に取り込みたい場面では行単位の文字列処理に依存しやすい。

一方で help の既定出力を機械向けへ寄せると、CLI の入口としての可読性が落ちる。

今回の要求では、既定の text help を維持したまま、opt-in で SEMDL 形式の
help 出力を追加する必要がある。

## Decision

SEMDL CLI の help には opt-in の rendering option を追加する。

### 1. default output

- 既定の help 出力は従来どおり text とする
- `ssd help`
- `ssd --help`
- `ssd <subcommand> --help`

### 2. opt-in machine-readable rendering

以下の help surface は `--format semdl` を受け付ける。

- `ssd help [topic] --format semdl`
- `ssd help reference [target] --format semdl`
- `ssd help recipes [target] --format semdl`
- `ssd --help --format semdl`
- `ssd <subcommand> --help --format semdl`

### 3. initial SEMDL profile

初期 slice の `--format semdl` は line-preserving な help profile とする。

- `document` block に help title、command、topic、format を持つ
- `resource` block に help topic を持つ
- `segment` block に text help の各非空行を順序付きで保持する

この初期 profile は CLI 出力契約として固定し、core parser がそのまま
再読込できることまでは要求しない。

### 4. unimplemented subcommands

未実装 subcommand であっても、公開済み command 名には command-specific help を用意する。
初期受け入れ面では search、extract、similarity、add、normalize の `--help` を固定する。

## Consequences

- 既定の CLI 可読性を維持したまま、help の構造化出力を追加できる
- text help と SEMDL help は二重契約になるため、golden を同時に更新する必要がある
- SEMDL help は parser 入力互換ではなく CLI 出力 profile として扱うため、過剰な format 境界変更を避けられる

## Alternatives Considered

### JSON や YAML を help の machine-readable 形式にする

却下。
今回の要求は SEMDL 形式を求めており、repo の文脈でも別 format を help 専用に増やす理由が弱い。

### 既定 help を SEMDL に変更する

却下。
CLI の直接利用時の可読性が下がり、0004 の入口設計と衝突する。

## Validation

- requirements.md に `--format semdl` と未実装 subcommand help の受け入れ面を追記する
- requirements.llmthink.dsl に対応 decision を追加する
- docs/cli.ebnf に help rendering option を追加する
- manifest と golden に text help / SEMDL help / invalid format failure を追加する
- CLI 実装で root/topic/subcommand help の renderer 切替を実装する

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/help-root.semdl.stdout
- docs/examples/golden/help-grammar.semdl.stdout
- docs/examples/golden/check-help.semdl.stdout