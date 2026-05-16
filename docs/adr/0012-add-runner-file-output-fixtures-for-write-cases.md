# 0012 Add Runner File Output Fixtures For Write Cases

- Status: Accepted
- Date: 2026-05-16

## Context

SEMDL の test runner は現在、manifest に基づいて CLI を実行し、
exit code、stdout、stderr だけを比較する。

この契約でも read-only command や dry-run surface は受け入れ固定できるが、
`split`、`set`、`annotate`、`remove` のような file-writing command では、
標準出力だけでは実際に書かれた `.ssd` / `.ssm` の内容を検証できない。

一方で、runner は ADR 0002 と ADR 0003 により CLI の外側から argv 実行を行う層として固定されており、
core library API を直接呼んで file update 結果を検証する構成にはできない。

## Decision

### 1. manifest extension

write case 向けに、manifest case へ次の optional key を追加する。

- `setup_files`
  - 実行前に repo fixture から一時 sandbox へコピーする repo-relative path の array
- `expected_files`
  - 実行後に sandbox 内の runtime path を repo fixture と比較する map
  - key は sandbox 内の runtime relative path
  - value は repo root 基準の expected fixture path

既存 case では両方とも省略可能とし、read-only case の構造は維持する。

### 2. sandboxed execution

case が `setup_files` または `expected_files` を持つ場合、runner は一時 sandbox を作る。

- `setup_files` は relative path を保ったまま sandbox へコピーする
- `cwd` は repo-relative のまま sandbox 内の対応パスへ再解決する
- `argv` は shell を介さず、そのまま CLI へ渡す
- runner は CLI を外部プロセスとして実行し続け、core library API を直接呼ばない

### 3. comparison boundary

comparison 順序は次のとおりとする。

- exit code
- stdout
- stderr
- declared `expected_files`

`expected_files` に列挙された file は byte-for-byte で fixture と一致しなければならない。
この slice では undeclared additional output は failure 条件にしない。

## Consequences

- file-writing CLI case も manifest-driven に acceptance へ載せられる
- runner の CLI 外部実行という layer boundary を保ったまま file output を比較できる
- sandbox copy と fixture compare の責務が runner 契約へ明示される

## Alternatives Considered

### stdout summary だけで write case を受け入れる

却下。
write result の中身が壊れていても stdout が一致すれば通ってしまい、
file-writing command の acceptance として弱い。

### runner が core library を直接呼んで file result を検証する

却下。
ADR 0002 と ADR 0003 の CLI / runner 境界に反し、
runner contract が library API に引きずられる。

## Validation

- requirements.md に sandboxed write-case acceptance の前提を追記する
- requirements.llmthink.dsl を同じ判断へそろえる
- docs/test-runner-format.md に `setup_files` と `expected_files` を追加する
- docs/test-runner-contract.md に sandbox execution と file comparison 順序を追加する
- split apply と update apply の success cases を manifest と fixture へ追加する
- runner 実装を更新し、write case を通す

## Related Artifacts

- docs/adr/0002-separate-library-cli-and-runner.md
- docs/adr/0003-establish-initial-implementation-layout.md
- docs/requirements.md
- docs/requirements.llmthink.dsl
- docs/test-runner-format.md
- docs/test-runner-contract.md
- docs/examples/testcases/cli-success.json