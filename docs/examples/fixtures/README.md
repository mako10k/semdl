# Fixture Overview

このディレクトリは、Ollama がない環境でも `extract` の埋め込み生成受け入れテストを再現できるようにするための deterministic fixture を保持する。

## Files

- `ollama-embeddings.psv`
  - 実 Ollama (`nomic-embed-text:latest`) から取得した固定ベクトルの正本
  - 区切りは `|`
  - 列: `target_id | kind | source_field | prompt | provider | model | dimensions | generated_at | vector`
- `ollama-embeddings-overview.ssd`
  - `ollama-embeddings.psv` の prompt mapping を SEMDL で読みやすく mirror した対比用サンプル
  - `ssd check` と `ssd explain` の動作確認に使う
- `fake-ollama.sh`
  - `ollama-embeddings.psv` だけを読んで prompt と model に一致した vector を返す test stub
- `embeddings/*.txt`
  - 実 Ollama から取得した各 prompt の raw capture backup
  - 現在の受け入れテストの正本ではなく、転記確認用の補助ファイルとして残している

## Prompt Mapping

| target_id | kind | source_field | prompt | raw_capture | provider | model | dimensions |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `R1` | `resource` | `label` | `抽出元文書` | `embeddings/r1.txt` | `ollama` | `nomic-embed-text:latest` | `768` |
| `S1` | `segment` | `text_quote` | `4月の売上は前年同月比で増加した` | `embeddings/s1.txt` | `ollama` | `nomic-embed-text:latest` | `768` |
| `A1` | `assertion` | `object` | `4月の売上は増加した` | `embeddings/a1.txt` | `ollama` | `nomic-embed-text:latest` | `768` |
| `H1` | `hypothesis` | `summary` | `増加の主体は国内売上である可能性が高い` | `embeddings/h1.txt` | `ollama` | `nomic-embed-text:latest` | `768` |
| `ALT1A` | `alternative` | `label` | `国内売上が増加した` | `embeddings/alt1a.txt` | `ollama` | `nomic-embed-text:latest` | `768` |

## Semantic Mirror

`ollama-embeddings-overview.ssd` は、README の表と 1 対 1 に近い対応になるように、5 個の embeddable target をそのまま SEMDL で並べたファイルである。

- `D1.source_ref` は `ollama-embeddings.psv` を指す
- 各 block の直前コメントに `target_id`、`source_field`、raw capture path、provider、model、dimensions を書く
- `R1 / S1 / A1 / H1 / ALT1A` の id と本文は authoritative PSV の prompt mapping に合わせる

README と `ollama-embeddings-overview.ssd` の mapping がずれていないかは次で機械確認できる。

```sh
sh docs/examples/fixtures/verify-overview-mapping.sh
```

## Quick Validation

build 済みなら、repo root から次で確認できる。

```sh
./build/ssd check docs/examples/fixtures/ollama-embeddings-overview.ssd
./build/ssd explain R1 docs/examples/fixtures/ollama-embeddings-overview.ssd
./build/ssd explain H1 docs/examples/fixtures/ollama-embeddings-overview.ssd
```

system install 用に CLI を prefix 配下へ配置したい場合は、`ssd` だけを `cmake --install` で install できる。
`semdl_runner` は internal test runner なので install 対象には含めない。

```sh
cmake -S . -B build
cmake --build build
cmake --install build --prefix "$HOME/.local"
```

既定では command は `bin/ssd` に入るので、上の例では `$HOME/.local/bin/ssd` が生成される。

`fake-ollama.sh` の参照先確認には次を使う。

```sh
bash docs/examples/fixtures/fake-ollama.sh run nomic-embed-text:latest "4月の売上は前年同月比で増加した"
```

## Capture Rule

fixture の更新時は、実 Ollama がある環境で対象 prompt に対して `ollama run nomic-embed-text:latest "..."` を実行し、取得した vector を `ollama-embeddings.psv` と raw capture backup の両方へ反映する。