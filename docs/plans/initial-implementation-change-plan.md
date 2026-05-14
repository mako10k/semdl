# Initial Implementation Change Plan

## Entry Point

この plan は、次のいずれかを入口として起こす前提とする。

- [plan-semdl-spec-change.prompt.md](../../.github/prompts/plan-semdl-spec-change.prompt.md)
- [semdl-spec-change/SKILL.md](../../.github/skills/semdl-spec-change/SKILL.md)

単発の相談では prompt、複数成果物をまたぐ更新では skill を優先する。

## Scope of the First Slice

最初の実装 slice は read-only 経路に限定する。

- `src/core/`
  - minimal.ssd / minimal.ssm を読める parse
  - 基本 validate
  - `id:`、`path:`、`meta:` の最小 resolve
  - inline / sidecar 統合 view
- `src/cli/`
  - `ssd check`
  - `ssd explain`
  - exit code と標準出力 / 標準エラーの最小整形
  - selector を持つ将来コマンドに共通で使う front-end の最小実装
  - 具体的には、selector 構文解釈、missing-target、wrong-layer を副作用なしで弾く preflight までを含む
- `src/runner/`
  - 薄い独立骨格だけを作る
  - `cli-success.json` / `cli-failure.json` の discovery と case 読み込み口だけを持つ
  - 実行と比較の本体は後続 slice とする

この slice は「更新機能の実行」を含まないが、「更新系コマンドの front-end 境界確認」は含む。

## Acceptance Surface for the First Slice

最初の実装対象に含める受け入れ例は次の 2 件とする。

- [check-minimal.stdout](../examples/golden/check-minimal.stdout)
- [explain-A1.stdout](../examples/golden/explain-A1.stdout)

最初の実装対象に含める failure case は次の 2 件を優先する。

- [check-unknown-option.error.stderr](../examples/golden/check-unknown-option.error.stderr)
- [set-malformed-selector.error.stderr](../examples/golden/set-malformed-selector.error.stderr)

selector resolve 境界の確認として、次の 1 件も first slice に含める。

- [set-id-missing-target.error.stderr](../examples/golden/set-id-missing-target.error.stderr)

## Expected File Impact

最初の実装 slice で変更対象になりうるのは次の範囲である。

- `src/core/`
- `src/cli/`
- `src/runner/`
- 必要なら build 用の最小設定ファイル

この slice では、次の docs は原則変更しない。

- [docs/requirements.md](../requirements.md)
- [docs/cli.ebnf](../cli.ebnf)
- [docs/test-runner-format.md](../test-runner-format.md)
- [docs/test-runner-contract.md](../test-runner-contract.md)

これらを変更したくなった場合は、implementation drift ではなく spec drift とみなし、先に spec-change フローへ戻る。

## Sequencing

1. `src/core/` の read-only parse / validate / resolve を作る
2. `src/cli/` の front-end で unknown option、malformed selector、missing-target を副作用なしで弾けるようにする
3. `src/cli/` から `ssd check` を通す
4. `src/cli/` から `ssd explain` を通す
5. `src/runner/` に manifest discovery の薄い骨格を置く
6. 既存 golden と比較して不足があれば spec-change フローへ戻る
7. その後に runner 実行 / 比較本体を別 slice として起こす

## Non-Goals

- `add` / `set` / `remove` / `annotate` の更新系実装
- `add` / `set` / `remove` / `annotate` の副作用を伴う更新実装
- `split` / `merge` / `normalize` の書き込み系実装
- runner の実行 / 比較本体実装
- docs の受け入れ surface を拡張すること