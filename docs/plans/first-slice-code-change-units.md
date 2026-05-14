# First Slice Code Change Units

## Target Cases

first slice で担保対象とする既存 case は次の 5 件とする。
first slice の front-end 厳格化に伴い、check の trailing unknown option も同時に担保する。

- [check-minimal](../examples/testcases/cli-success.json)
- [explain-A1](../examples/testcases/cli-success.json)
- [check-unknown-option-long](../examples/testcases/cli-failure.json)
- [check-trailing-unknown-option-long](../examples/testcases/cli-failure.json)
- [set-malformed-selector-missing-field](../examples/testcases/cli-failure.json)
- [set-id-missing-target](../examples/testcases/cli-failure.json)

## Change Units

### Unit 1: Core document loading skeleton

- Ownership: `src/core/`
- Goal: `.ssd` と対応 `.ssm` の存在を read-only に把握できる最小 loading 口を作る
- Covers:
  - [check-minimal](../examples/testcases/cli-success.json)
  - [explain-A1](../examples/testcases/cli-success.json)
- Initial files:
  - `include/semdl/core/document_store.hpp`
  - `src/core/document_store.cpp`

### Unit 2: CLI command dispatch skeleton

- Ownership: `src/cli/`
- Goal: `check` と `explain` の entrypoint を最小配線する
- Covers:
  - [check-minimal](../examples/testcases/cli-success.json)
  - [explain-A1](../examples/testcases/cli-success.json)
- Initial files:
  - `include/semdl/cli/cli_app.hpp`
  - `src/cli/cli_app.cpp`
  - `src/cli/main.cpp`

### Unit 3: CLI option preflight

- Ownership: `src/cli/`
- Goal: leading / trailing unknown option を実行前に弾く front-end を作る
- Covers:
  - [check-unknown-option-long](../examples/testcases/cli-failure.json)
  - [check-trailing-unknown-option-long](../examples/testcases/cli-failure.json)
- Expected follow-up:
  - `check` に対する許可オプション表の追加

### Unit 4: Selector syntax preflight

- Ownership: `src/cli/` with later support from `src/core/`
- Goal: `path:<id>.<field>` の dotted field を要求する最小構文検査を置く
- Covers:
  - [set-malformed-selector-missing-field](../examples/testcases/cli-failure.json)
- Expected follow-up:
  - selector parser の共通化

### Unit 5: Missing-target resolve preflight

- Ownership: `src/core/` and `src/cli/`
- Goal: 構文的には正しい `id:` selector が対象不在で失敗する経路を作る
- Covers:
  - [set-id-missing-target](../examples/testcases/cli-failure.json)
- Expected follow-up:
  - wrong-layer 判定の追加

### Unit 6: Runner manifest discovery skeleton

- Ownership: `src/runner/`
- Goal: `cli-success.json` / `cli-failure.json` を既定探索する薄い骨格を作る
- Covers:
  - first slice の実行準備
- Initial files:
  - `include/semdl/runner/discovery.hpp`
  - `src/runner/discovery.cpp`
  - `src/runner/main.cpp`

## Sequencing

1. Unit 1
2. Unit 2
3. Unit 3
4. Unit 4
5. Unit 5
6. Unit 6

## Validation Order

1. CMake configure and build succeed
2. `ssd check` entrypoint starts from the new skeleton
3. `ssd explain` entrypoint starts from the new skeleton
4. preflight failures for leading / trailing unknown option, malformed selector, and missing-target become independently testable
5. runner discovery skeleton can locate both manifest files without executing cases yet