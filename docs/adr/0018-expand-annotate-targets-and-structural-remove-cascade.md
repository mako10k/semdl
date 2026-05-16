# 0018 Expand Annotate Targets And Structural Remove Cascade

- Status: Accepted
- Date: 2026-05-16

## Context

`ssd annotate` は sidecar shorthand としては機能していたが、standalone `.ssd` に対する inline 更新面と auto routing が未固定だった。
同時に `ssd remove` は metadata subtree 削除までは扱えても、structural single-target delete と `--cascade` の dependency edge が未固定で、受け入れ例を安定して増やせない状態だった。

この 2 つはどちらも write target の決定規則が核になる。
annotate では paired / standalone と target kind ごとの routing を先に決めないと `.ssd` / `.ssm` 境界がぶれる。
remove では dependency edge を先に決めないと cascade が実装依存になり、安全失敗の意味も曖昧になる。

## Decision

### 1. annotate target matrix

`ssd annotate` は `--target inline|sidecar|auto` を受け付ける。

- `--target sidecar` は sidecar metadata 更新として扱う
- `--target inline` は standalone `.ssd` 入力に限って成功する
- inline annotate の対象 kind は `assertion` と `hypothesis` に限定する
- `--target auto` は paired input では sidecar を選ぶ
- `--target auto` は standalone input では inline が許される target だけ inline を選び、それ以外は sidecar へ落とす

create-only metadata field-map は引き続き `ssd add annotation --target sidecar` に残す。

### 2. structural remove stays single-target by default

`ssd remove` は metadata selector に加えて structural single-target delete を許可する。
ただし structural target に inbound reference がある場合は既定で failure とする。

- `type:<kind> --allow-multi` を除き multi-target structural remove は後続 slice に分離する
- selector が構造要素を一意に指すときだけ structural remove を試みる
- metadata remove は従来どおり structure-safe な subtree delete として扱う

### 3. cascade edge list

`--cascade` が明示されたときだけ direct / transitive dependent を target と同時削除してよい。
この slice の dependency edge は次に固定する。

- assertion <- hypothesis.about
- hypothesis <- alternative.group
- resource <- segment.source

cascade は closure 全体を削除し、dependent を残した部分削除や partial detach は行わない。

### 4. preview and write-path contract

annotate の `--dry-run` は既存 update preview format を再利用する。
write target は resolved target matrix に従って 1 つの inline `.ssd` または sidecar `.ssm` に決まる。
remove はこの slice では apply path のみを扱い、paired apply では必要な sibling file だけを書き換える。

## Consequences

- annotate は standalone と paired の両方で明示的な target routing を持つ
- inline annotate を assertion / hypothesis に絞ることで、現行 `.ssd` grammar artifact との整合を保てる
- remove は structural delete を扱えるが、single-target + explicit cascade に閉じた安全面を維持する
- acceptance では inline annotate success、sidecar fallback、cascade success、non-cascade failure を固定できる

## Alternatives Considered

### annotate inline を全 entity kind に広げる

却下。
現行の `.ssd` grammar artifact で inline meta block を許している境界より広くなり、spec と実装のズレを増やす。

### remove cascade の dependency edge を実装依存のままにする

却下。
受け入れ例とエラーメッセージの安定性が失われる。

### structural multi-remove を同時に解禁する

却下。
selector safety、preview shape、partial failure policy まで広がり slice が大きくなりすぎる。

## Validation

- requirements.md と requirements.llmthink.dsl を annotate target matrix / remove cascade edge に同期する
- cli.ebnf を annotate target option と remove cascade option に合わせて更新する
- annotate / remove / grammar help goldens を更新する
- inline annotate success と paired inline failure を acceptance に追加する
- structural remove single-target success、cascade success、non-cascade failure を acceptance に追加する
- CLI 実装を build し、runner を通す

## Related Artifacts

- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/cli.ebnf
- docs/examples/testcases/cli-success.json
- docs/examples/testcases/cli-failure.json
- docs/examples/golden/annotate-help.stdout
- docs/examples/golden/remove-help.stdout
- docs/examples/golden/help-grammar.stdout
