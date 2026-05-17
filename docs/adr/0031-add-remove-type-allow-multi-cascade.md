# 0031 Add Remove Type Allow-Multi Cascade

- Status: Accepted
- Date: 2026-05-17

## Context

`ssd remove` は現在、metadata subtree delete、structural single-target delete、single-target `--cascade`、および `type:<kind> --allow-multi` による限定的な multi-target structural remove までを扱っている。
このため multi-target structural remove 自体は一部使えるが、matched set に dependent があると closure をまとめて削除する経路がなく、`--allow-multi` は safe subset にしか使えない。

一方で broader multi-target selector semantics や remove の non-destructive output surface を同時に入れると、selector language、preview contract、write policy が一気に広がる。
そのためこの slice では既存 type selector surface を維持したまま、multi-target remove にだけ cascade closure を追加する。

## Decision

### 1. extend only the existing `type:<kind> --allow-multi` surface

この slice では `ssd remove type:<kind> --allow-multi --cascade <file>` を追加してよい。

- `--allow-multi` は引き続き `type:<kind>` selector にのみ意味を持つ
- `--cascade` 単独の single-target remove は current behavior を維持する
- broader multi-target selector semantics は引き続き後続 slice に分離する

### 2. option order stays narrow and explicit

multi-target cascade surface は `type:<kind> --allow-multi --cascade <file>` の順序に限定してよい。

- `ssd remove type:alternative --allow-multi --cascade docs/examples/minimal.ssd`
- `ssd remove type:hypothesis --allow-multi --cascade docs/examples/remove-multi-cascade-source.ssd`

この slice では option reordering や additional shorthand は導入しない。

### 3. closure is the union of existing cascade edges for each matched root

matched root set に `--cascade` を付けた場合、削除対象は各 root から辿れる existing cascade closure の union としてよい。

- assertion <- hypothesis.about
- hypothesis <- alternative.group
- resource <- segment.source

single-target cascade と同じ edge list を再利用し、partial detach や edge list の拡張は行わない。

### 4. failure remains safety-first

`--allow-multi` without `--cascade` は current behavior を維持し、matched root set の外側に dependent が残るなら failure としてよい。

`--allow-multi --cascade` では union closure の外に dependent が残らないことを確認してから mutation してよい。
failure は file write 前に起こること。

### 5. apply-only boundary stays in place

remove はこの slice でも apply path のみとする。

- `--dry-run` / `--stdout` / `--out` は追加しない
- paired input では current sibling rewrite behavior を維持する
- success stdout は current update apply result format を再利用してよい

## Consequences

- `type:<kind> --allow-multi` は safe subset から dependent-aware structural remove へ一段広がる
- broader selector generalization や non-destructive remove output を reopen せずに、既存 remove surface を実用化しやすくなる
- acceptance は multi-target no-cascade failure と multi-target cascade success を同じ fixture family で固定できる

## Alternatives Considered

### allow-multi を type selector 以外へ一般化する

却下。
current selector semantics では multi-match source が type selector に限られ、new selector contract を同時に作ると slice が大きくなりすぎる。

### remove に dry-run / stdout / out を先に追加する

却下。
write policy と preview shape の設計が先に広がり、existing safety matrix の延長としては遠い。

### multi-target remove で cascade を暗黙有効にする

却下。
current remove の explicit safety controls と対称でなくなる。

## Validation

- requirements.md と requirements.llmthink.dsl を remove multi-target cascade slice に同期する
- docs/cli.ebnf と remove help / grammar help を更新する
- success manifest に `type:<kind> --allow-multi --cascade` success case を追加する
- failure manifest に no-cascade multi-target failure と invalid option ordering boundary を追加または更新する
- CLI 実装で parse、union closure collection、write path を追加し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/remove-help.stdout
- docs/examples/golden/remove-help.semdl.stdout
- docs/examples/golden/help-grammar.stdout
- docs/examples/golden/help-grammar.semdl.stdout