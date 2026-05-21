SEMDL 要求仕様

# 1. 目的

SEMDL は、自然言語文書や構造化されていない知識から抽出した意味的構造を、
人間が読めて、機械が検証できて、検索・比較・追跡できる形で保存するための
軽量な記述系およびツール群である。

本仕様の目的は、次の 4 点を同時に満たすことである。

- 意味的構造を明示的に記述できること
- その記述が原文のどこに基づくか追跡できること
- 推測・曖昧さ・代替解釈を事実と区別して記述できること
- 構造検索と埋め込みベース検索を同一の資産に対して実行できること

# 2. スコープ

SEMDL が提供する対象は以下とする。

- 主要意味文書 `.ssd`。正式表示名は SEMDL Canvas とする
- 付随メタデータ companion `.ssm`。正式表示名は SEMDL Sidekick とする
- `.ssd` を対象とする検索クエリ `.ssq`。正式表示名は SEMDL Lens とする
- .ssd の検証、抽出、検索、比較を行う CLI ツール ssd
- 埋め込みベクトルを併用した意味類似検索機能
- `.ssd`、`.ssm`、`.ssq` を対象とする first-party VS Code extension
- Node ベースの first-party MCP server
- `.ssd`、`.ssm`、`.ssq` の code highlighting
- editor-side diagnostics / symbols を提供する TypeScript language server

SEMDL が対象外とするものは以下とする。

- 汎用の知識表現標準そのものの再実装
- 完全な自動意味解析器の内蔵
- 数学的に厳密な確率推論エンジンの汎用実装
- 生バイナリをそのまま人手編集するための重量級フォーマット
- non-VS Code editor 向け first-party plugin の同時実装

## 2.1 正式名称ポリシー

- `.ssd` の正式表示名は SEMDL Canvas とする
- `.ssm` の正式表示名は SEMDL Sidekick とする
- `.ssq` の正式表示名は SEMDL Lens とする
- `Solo mode` と `Companion mode` を profile の正式表示名とし、`inline profile` と `sidecar profile` は移行期の対応表現に限って残してよい
- `Canvas`、`Sidekick`、`Lens` は人向けの正式名称であり、拡張子、CLI binary 名、subcommand 名、language id、tool id、schema key は変更しない
- `sidecar` は sibling companion file や ownership boundary を指す技術用語として残してよい

# 3. 設計方針

## 3.1 軽量性

- UTF-8 のテキストとして保存できること
- 人手で差分確認しやすいこと
- 最小構成では外部データベースを必須にしないこと

## 3.2 可読性

- 人間が見て構造の境界を追いやすいこと
- 最小限の記法で記述できること
- 省略規則はあってよいが、意味が曖昧になる省略は避けること
- 意味構造の理解に必須でない大量メタ情報は、本体から分離できること

## 3.3 追跡可能性

- 各主張が原文または他の主張のどれに基づくか辿れること
- 推論で生成した主張には推論であることが明示されること

## 3.4 拡張可能性

- 将来の語彙追加や独自属性追加に耐えること
- ローカル埋め込みと外部埋め込みサービスの両方に対応できること

## 3.5 エディタ統合

- first-party editor integration は VS Code extension を対象にしてよい
- initial code highlighting は `.ssd`、`.ssm`、`.ssq` の TextMate grammar を使い、language server 非起動時でも機能してよい
- initial language server は TypeScript 実装にしてよく、extension host から分離した process として扱ってよい
- initial editor integration は `.ssd`、`.ssm`、`.ssq` の basic language identification を提供してよい
- initial LSP feature set は parse / validate diagnostics と top-level document symbols に限定してよい
- richer LSP surface を広げる前に、現行 diagnostics / symbols を binary 由来監査情報へ差し替え可能にする analysis provider boundary を先に導入しなければならない
- provider boundary 導入後の最初の richer LSP slice は、existing parser / validator semantics だけから導出できる richer diagnostics を優先してよい
- completion は 1 slice にまとめず、grammar artifact 由来の keyword completion を local identifier completion より先に導入してよい
- initial keyword completion は grammar-derived keyword に限定してよく、top-level block keyword、allowed nested block keyword、query header keyword、query entry keyword だけを候補にしてよい
- initial keyword completion は document-local context だけで判定してよく、identifier、field name、cross-file symbol は候補に含めてはならない
- initial hover は grammar-derived keyword hover に限定してよく、top-level block keyword、allowed nested block keyword、query header keyword、query entry keyword だけを対象にしてよい
- initial hover の表示名と format 名は requirements から、構文形は primary grammar artifact から導出しなければならない
- initial hover は document-local context だけで判定してよく、identifier explanation、field-level inference、cross-file lookup を要求してはならない
- initial local identifier completion の source-of-truth は current `.ssd` document 内で宣言済みの top-level block identifier に限定してよい
- initial local identifier completion は document-local scope に限定し、cross-file symbol、workspace index、paired file lookup を要求してはならない
- initial local identifier completion は requirements 既存 relation で参照先 kind が固定されている `segment.source`、`assertion.subject`、`assertion.source_segment`、`hypothesis.about` の field value だけで提供してよい
- initial local identifier completion は `predicate`、`kind`、`group`、free scalar field、quoted string position、reference eligibility が未固定の field では候補を出してはならない
- hover、definition、references、rename、formatting、code action、workspace-wide index は、それぞれの source-of-truth、stable symbol identity、cross-file data source、canonical formatting contract、mutation safety rule の必要条件を満たすまで deferred のままにしてよい
- semantic token は後続 slice に分離してよい
- editor integration は requirements、grammar artifacts、core / CLI behavior に準拠する adapter surface とし、editor-only semantics を先行定義してはならない
- first-party MCP server は Node ベースの adapter surface として実装してよく、既存 CLI / core behavior を proxy または再利用する側に置いてよい
- first-party MCP tool と VS Code language model tool は同じ Tool inventory と input schema source of truth を共有しなければならない
- first-party MCP / VS Code language model tool の first slice は read-only tool を優先してよく、mutation tool は後続 slice に分離してよい
- public tool schema、error semantics、監査入力の正本は Node 側 convenience ではなく CLI / core / binary contract から導出されなければならない
- language server は TypeScript process に留めてよいが、LSP 監査に必要な元情報は将来的に binary から Node adapter 経由で取得できる形に寄せなければならない
- binary audit contract が揃うまでの移行段階では、Node 側 local analyzer fallback を残してよいが、恒久的な source of truth と見なしてはならない
- editor diagnostics の expected-error allow rule は editor-only policy file として定義してよく、core / CLI format へ先行して埋め込んではならない
- initial expected-error allow rule は診断対象 file と同一ディレクトリの `.semdl-diagnostics.json` だけを見てよい
- initial expected-error allow rule は exact file name、1-based line number、exact diagnostic message の一致だけを許容条件にしてよい
- wildcard、directory-wide default、workspace-wide default、parent directory 継承、message prefix match は initial slice に含めない

# 4. 概念モデル

SEMDL は最低限、以下の概念を扱えること。

## 4.1 リソース

文書、文、節、語句、画像、表、バイナリ領域、外部 URI など、意味づけの対象となるもの。

## 4.2 セグメント

原文や原データ中の一部分を指すもの。少なくとも以下を表現できること。

- 文字列引用による選択
- テキスト位置範囲による選択
- バイト位置範囲による選択
- 必要に応じた入れ子の絞り込み

注記: これは W3C Web Annotation の Selector モデルに近い考え方を採る。

## 4.3 主張

主語・述語・目的語、またはそれと等価な関係表現により、意味的関係を記述する単位。

主張は以下を持てること。

- 識別子
- 型
- 属性
- 原文対応
- 根拠
- 作成者または生成器

## 4.4 修飾付き主張

主張そのものに対して、追加情報を付与できること。少なくとも以下を扱えること。

- 確信度
- 根拠の種類
- 生成時刻
- 生成方法
- 役割
- コメント

注記: これは RDF-star や PROV-O の qualified relation pattern に相当する要求である。

## 4.5 出所情報

各主張や各抽出結果について、少なくとも以下を区別できること。

- 元データそのもの
- その主張を生んだ処理または推論
- その処理の実行主体

すなわち、Entity / Activity / Agent に相当する追跡情報を保持できること。

## 4.6 仮説と代替案

SEMDL は、事実として確定していない内容を仮説として記述できなければならない。

- 仮説であることが明示されること
- 代替案を複数並べて保持できること
- 代替案どうしが同一の論点に属することを表現できること
- 代替案ごとに根拠と確信度を持てること

## 4.7 埋め込み

意味検索や類似度計算に用いるため、主張・リソース・セグメントに埋め込みベクトルを関連付けられること。

- 埋め込みモデル名
- ベクトル次元
- 生成日時
- 生成元

埋め込み生成は、初期仕様では read path ではなく explicit write path の責務とすることを推奨する。

- `ssd extract` の明示オプション付き出力
- 将来の dedicated embedding / enrich / derive 系 command
- 外部前処理によって生成された sidecar / external reference

最初の実装 slice では、`ssd extract` を既存 `.ssd` から paired `.ssm` を生成する
embedding enrichment path として使ってよい。
このとき provider は explicit adapter surface として `ollama` と `openai` を受け付け、
`--embed-provider <provider> --embed-model <model>` を明示指定する。
加えて `ssd extract` は plain `.txt` input から skeletal `.ssd` を生成してよく、
初期 raw extraction では `document D1`、`resource R1`、non-empty line ごとの `segment S<n>` に限定する。
後続 multi-input extract out slice では、`ssd extract --out <output.ssd> <input>...` に限って
複数 input を集約して 1 つの inline `.ssd` を生成してよく、mixed `.ssd` / `.txt` input、
deterministic な ID rebasing、paired `.ssm` 付き input の resolved view 集約を受け付けてよい。

`ssd search`、`ssd similarity`、`.ssq` evaluation は、既存埋め込みを読み取って使うだけに留め、
暗黙の再計算や自動永続化を行わないことを推奨する。

# 5. .ssd フォーマット要件

## 5.1 基本要件

- 拡張子は .ssd とする
- UTF-8 テキストで保存できること
- コメント記法を持つこと
- 安定した識別子参照ができること
- パーサが一意に解釈できること

## 5.2 記述モード

SEMDL は、少なくとも次の 2 つの記述モードを許容すること。

- Solo mode（inline profile）
  - 意味構造とメタ情報を 1 つの Canvas `.ssd` にまとめて記述する方式
- Companion mode（sidecar profile）
  - 意味構造の本体を Canvas `.ssd` に記述し、可読性を損なうメタ情報を Sidekick `.ssm` へ分離する方式

Companion mode は、可読性を優先するための推奨方式として扱ってよい。
一方で、単独配布やレビュー容易性を優先する場合は Solo mode も許容する。

注記: つまり「サイドカー専用」ではなく、「サイドカーを既定選択肢として認め、必要なら本体へ戻せる」方式とする。

## 5.3 記述対象

.ssd では少なくとも以下を記述できること。

- リソース定義
- セグメント定義
- 主張定義
- 修飾付き主張
- 出所情報
- 仮説と代替案
- 埋め込みメタデータ

ただし Companion mode では、上記のうち一部を別ファイルへ退避できること。

## 5.4 本体とサイドカーの責務分担

Canvas `.ssd` 本体には、意味構造を読むために最低限必要な情報を優先して残すこと。

- リソース定義
- セグメント定義
- 主張定義
- 仮説と代替案の関係そのもの
- 読解に必須な最小限の属性

以下の情報は、サイドカーへ分離可能であること。

- 詳細な出所情報
- 詳細な修飾付き主張
- 埋め込みベクトル本体およびその詳細メタデータ
- 生成器情報、生成時刻、処理履歴
- 長い根拠メモ、説明コメント
- 集計用または検索最適化用の補助情報

以下の情報は、サイドカーへ逃がせてもよいが、本体から完全に見えなくしてはならない。

- 原文対応が存在すること
- 主張が推測であること
- 代替案が存在すること
- 埋め込みが存在すること

注記: つまり、本体だけ読んでも「何が主張されているか」と「それが事実か仮説か」は分かる必要がある。

埋め込みの persist 境界は少なくとも以下を満たすことを推奨する。

- `.ssd` 本体には存在可視化だけを残してよい
- ベクトル本体と詳細メタデータは `.ssm` または外部参照へ置けること
- 検索順位や一時近傍 index のような derived data を persist 必須にしないこと

## 5.5 サイドカー形式

Companion mode では、Canvas `.ssd` と同じベース名を持つ Sidekick `.ssm` を関連付けられること。

例:

- sample.ssd
- sample.ssm

ここで .ssm は SEMDL metadata sidecar を表す拡張子とする。

.ssm は少なくとも以下を満たすこと。

- .ssd 内の識別子を参照できること
- .ssd 本体を変更せずに付加情報だけ更新できること
- 複数の主張やリソースに対するメタ情報をまとめて保持できること
- パーサが .ssd と .ssm の対応関係を一意に解決できること

埋め込みを保持する場合、少なくとも以下のいずれかを記録できること。

- vector body
- vector_ref

`vector_ref` の解決規則は少なくとも以下を満たすことを推奨する。

- 絶対パスはそのまま使う
- 相対パスは、その embedding record を宣言しているファイルを基準に解決する
- paired sidecar 運用では `.ssm` にある record は `.ssm` 基準、inline record は `.ssd` 基準とする

あわせて、少なくとも以下のメタデータを保持できること。

- target id
- model
- dimensions
- generated_at
- generator または provider

## 5.6 サイドカーの解決規則

ツールは、特段の指定がない場合、同一ディレクトリにある同ベース名の .ssm を自動検出してよい。

解決順序は少なくとも以下をサポートすること。

- .ssd 単独
- .ssd + 同ベース名 .ssm
- 明示指定された任意の .ssd / .ssm 組

.ssd と .ssm の両方に同一項目が存在する場合の優先規則は仕様で固定すること。
初期仕様では、以下を推奨する。

- 構造定義は .ssd を正とする
- 補助メタ情報は .ssm を正とする
- 同一キー競合時はエラーまたは警告とする

## 5.7 原文対応

.ssd は、意味構造と原文または原データの対応を記述できなければならない。

- 元リソースへの参照
- セグメント指定
- 直接引用
- 任意の補足メモ

原文対応は、単に URL を貼れるだけでは不十分であり、少なくとも「どの部分に対応するか」を示せる必要がある。

Companion mode では、詳細な selector、state、rendering 情報を Sidekick `.ssm` へ分離してよい。
ただし本体には「原文対応あり」の最小表現が残ること。

## 5.8 推測の明示

原文に明示されていない補完や省略復元を記述する場合は、以下が必須であること。

- 推測であるという種別
- その推測の根拠
- 競合する他候補がある場合の列挙

Companion mode では、詳細な根拠説明や評価メモを Sidekick `.ssm` へ分離してよい。
ただし推測種別と代替案の存在は .ssd 本体で判別できること。

## 5.9 バイナリデータの扱い

バイナリデータは .ssd 本文へ無制限に埋め込むのではなく、以下のいずれかで扱うことを基本とする。

- 外部ファイル参照
- バイト範囲指定
- 必要最小限のエンコード済み埋め込み

注記: これにより「バイナリも扱える」という要求は満たしつつ、可読性と差分性を保つ。

埋め込みベクトル本体や大きな派生データは、可能なら .ssm または外部バイナリ参照へ逃がすことを推奨する。

## 5.10 自己参照

.ssd 文書自身に対してメタデータを付与できること。

- 文書 ID
- バージョン
- 作成者
- 生成器
- 文書全体の出所情報

これは「自己リファレンス」を、実装可能で検証しやすい形に限定した定義である。

Companion mode では、文書全体メタデータの詳細版を Sidekick `.ssm` に置けること。

## 5.11 `.ssd` / `.ssm` grammar artifact

`.ssd` と `.ssm` の primary grammar artifact は [docs/ssd.ebnf](docs/ssd.ebnf) に置くことを推奨する。

この artifact は、少なくとも以下を formalize する。

- block-oriented な記述構造
- document / resource / segment / assertion / hypothesis / alternative
- document_meta / meta / embedding
- identifier、quoted string、number、boolean の基本字句

# 6. .ssq フォーマット要件

## 6.1 基本要件

- 拡張子は .ssq とする
- .ssd を検索対象とする
- テキストとして保存・共有できること
- 初期仕様では参照系の問い合わせ専用とし、更新構文は持たないこと

.ssq は、CLI argv より優先して grammar を洗練すべき query language artifact として扱うことを推奨する。

## 6.2 検索機能

.ssq は少なくとも以下の検索条件を表現できること。

- ノード型による検索
- 関係型による検索
- 属性値による検索
- 原文対応の有無による検索
- 出所情報による検索
- 仮説 / 事実 / 代替案の種別による検索
- 埋め込みベースの類似検索条件

初期仕様の similarity condition は、free-text query ではなく既存 target を基準にした近傍検索として扱うことを推奨する。
`.ssq` の `similar:` は、統合ビュー内の既存 target を参照し、暗黙の再埋め込みを起こさないこと。
初期 slice では、`similar:` の anchor target と candidate 集合はいずれも入力集合の統合ビュー全体で解決し、anchor 自身は結果から除外することを推奨する。

## 6.3 結果の意味

検索結果は少なくとも以下のどちらかを返せること。

- 一致した主張またはリソース
- 一致箇所を含む部分グラフ

初期 slice の `return: subgraph` は grouped result を返し、`select` と optional single `where` を使う通常検索に加えて、
`similar` を伴う query でも受け付けてよい。

初期 slice の subgraph result は global union ではなく、match ごとの grouped result とすることを推奨する。
各 group は matched node と、その node から outbound に 1 hop で参照される structural node だけを context として含める。
初期 boundary は少なくとも次に限定してよい。

- `segment`
  - `source` が指す `resource`
- `assertion`
  - `subject` が指す `resource`
  - `source_segment` が指す `segment`
- `hypothesis`
  - `about` が指す `assertion`

`resource`、`alternative`、`document` は、この段階では追加の context expansion を要求しない。
subgraph 出力は少なくとも `query_file`、`mode`、`inputs`、`subgraphs`、各 group の `match_file`、`match_id`、`match_kind`、`context_nodes` を含み、context node は `file`、`id`、`kind` を持つことを推奨する。
`similar` を伴う場合は top-level に `anchor`、`metric`、`model`、`dimensions` を持ち、各 group に `score` を含めることを推奨する。

類似検索を含む結果解決では、初期仕様では同一 model と同一 dimensions の埋め込みだけを比較対象とすることを推奨する。
cross-model alignment や暗黙変換は、この段階では要求しない。
初期 slice の `return: matches` では、similarity を使う場合に少なくとも metric と per-match score を結果へ明示し、順位は score 降順、同点時は file path と id の昇順で安定化することを推奨する。
anchor または candidate の埋め込みが欠落、malformed vector、unreadable `vector_ref`、model/dimensions 不一致、zero-norm vector の場合は、skip や no-match へ劣化させず non-zero failure とする方針を優先する。

## 6.4 更新構文の扱い

初期仕様の .ssq には、insert、update、delete などの更新構文を含めないことを推奨する。

理由は以下の通りである。

- 検索言語の責務を参照系に限定したほうが理解しやすい
- .ssd / .ssm の分離運用では、更新対象が本体かサイドカーかを明示する規則が別途必要になる
- 更新を言語へ入れると、検証、競合解決、トランザクション、dry-run、差分出力まで設計対象が広がる
- 現段階の要求は検索・抽出・説明・統合分離の整理が主であり、更新言語まで入れると過剰になりやすい

したがって、更新操作が必要な場合は、まず CLI の専用サブコマンドで扱う方針を優先する。

例:

- ssd add ...
- ssd set ...
- ssd remove ...
- ssd annotate ...

これにより、更新対象、出力先、サイドカー方針、競合時挙動をコマンドオプションで明示しやすくする。

将来的に更新構文を導入する場合は、以下のいずれかとして分離することを推奨する。

- .ssq を参照専用のまま維持し、更新専用の別言語を導入する
- semql のような上位言語を定義し、その中で query と mutation を明示的に分ける

注記: もし将来 semql という名称を採用するなら、GraphQL の query / mutation 分離に近い責務分離を採るほうが自然である。

## 6.5 将来の semql における責務境界

将来的に semql を導入する場合、初期仕様では .ssq を置き換えるのではなく、.ssq を包含する上位概念として扱うことを推奨する。

責務は少なくとも以下のように分離する。

- query
  - 読み取り専用
  - .ssd / .ssm を解決した統合ビューに対して問い合わせる
- mutation
  - 更新専用
  - 追加、変更、削除、注釈付与などの変更要求を表現する
- admin または transform
  - split、merge、normalize などの運用変換を表現する

このとき、query と mutation は同じ識別子体系を参照してよいが、意味的責務は分離すること。

特に mutation は、単なる値更新に留めず、少なくとも以下を伴う設計が必要になる。

- 更新対象が .ssd 本体か .ssm かの決定
- 更新後の整形方針
- 競合時ポリシー
- dry-run
- 失敗時のロールバック方針または非適用保証

したがって semql は、検索文法の自然拡張としてではなく、運用責務を含む別レイヤとして設計するのが望ましい。

## 6.6 `.ssq` grammar artifact

`.ssq` の primary grammar artifact は [docs/ssq.ebnf](docs/ssq.ebnf) に置くことを推奨する。

初期 artifact は、requirements 上の query responsibility を formalize する minimal profile とし、少なくとも以下を表すこと。

- 読み取り専用 query であること
- query block を 1 つ持つこと
- selector / filter / similarity condition の最小 slot
- result mode

初期 profile の `similar` は、既存 target を参照する similarity slot とし、free-text を受けないことを推奨する。

初期 profile の `where` は sample-backed な最小構文に限定し、少なくとも以下のみを表すこと。

- 属性またはフラグの presence check
- scalar 値との単一 equality check

ここでの scalar 値は、quoted string、number、boolean に限定することを推奨する。

後続 filter slice では、`.ssq` の `where` を次まで広げてよい。

- numeric field に対する `>`, `>=`, `<`, `<=` の range comparison
- `and` / `or` を含む boolean expression
- parenthesized grouping
- 各 leaf 項は presence check、equality check、numeric range comparison のいずれか

この slice では次を追加制約として固定する。

- `=` は既存互換の token equality を維持し、数値右辺でも lexical equality として扱う
- range comparison の右辺は number に限定する
- range comparison で左辺 field が欠損または非数値の candidate は failure ではなく no-match とする
- precedence は `and` が `or` より高い
- 括弧は precedence override と nested grouping に使ってよい
- unary `not` は filter term または parenthesized group の前に置いてよく、repeated unary `not` も許可してよい
- `not` は `and` より高い precedence を持ち、dangling `not` は validation failure とする
- current `where` profile では operand position の `not` は reserved keyword とし、field name `not` は受け付けない
- 関数呼び出しと新 predicate は引き続き `.ssq` grammar artifact に含めない

workspace に concrete sample が少ない段階では、この artifact を full query language の完成版としてではなく、
将来 semql family へつながる starting point として扱う。

# 7. 類似度と不確実性

## 7.1 類似度

SEMDL は埋め込みベクトルに基づく類似度計算を提供する。

- 主張単位または文書単位で比較できること
- 使用した埋め込みモデルを結果に含められること
- 類似度指標を明示できること

初期仕様の pairwise similarity は、既存の 2 target を比較する read-only operation とすることを推奨する。
比較対象に埋め込みが存在しない場合、暗黙生成ではなく failure とする方針を優先する。

初期仕様では、同一 model・同一 dimensions・明示された metric の組だけを比較可能とし、
cross-model 比較は既定で行わないことを推奨する。

similarity metric の source of truth は embedding persistence record ではなく command / query evaluation policy とし、
初期仕様では cosine を既定値としてよいが、結果には使用 metric を明示することを推奨する。

## 7.2 不確実性

SEMDL は不確実性を扱えること。ただし、要件は以下のように限定する。

- 各仮説や主張に対する確信度を保持できること
- 代替案の集合に対して相対比較ができること
- 複数段の推論経路について、利用した合成方式を明示した場合に限り、派生スコアを計算できること

注記: 元の「可能性連鎖の確率が計算される」は数学的前提が曖昧であり、そのままでは要求として不適切であるため、
「確信度とその合成方式を明示した評価値を計算できる」に修正する。

# 8. CLI 要件

CLI ツール名は ssd とする。

最低限、以下のコマンドを提供すること。

- ssd check <file>
  - 構文と参照整合性を検証する
- ssd search <query.ssq> <file>...
  - 構造検索と意味検索を実行する
- ssd extract --out <output.ssd> <input>...
  - 入力から .ssd を生成する
- ssd similarity <target1> <target2> <file>
  - 類似度を計算する

`ssd search` は、初期仕様では既存埋め込みを解決して検索に使う read-only command とし、
必要な埋め込みが欠ける場合でも暗黙に生成・保存しないことを推奨する。
`similar:` を含む `return: matches` の first slice では、anchor target と候補を入力集合の統合ビュー全体で解決し、
score 順に結果を返し、embedding 不備は no-match へ劣化させず failure とする方針を優先する。

`ssd similarity` は、既存の 2 target を 1 つの入力文書集合の中で pairwise 比較する command とし、
少なくとも両側の embedding existence、model 一致、dimensions 一致を前提条件に置くことを推奨する。
初期 slice では `ssd similarity <target1> <target2> <file>` を受け付け、結果には left/right operand、metric、model、dimensions、score を含めることを推奨する。
embedding record が `vector` または `vector_ref` を持つ場合はいずれも解決可能であることを推奨し、
malformed vector、vector_ref 読み取り失敗、zero-norm vector は non-zero failure とする方針を優先する。
- ssd explain <id> <file>
  - 指定した主張または仮説の根拠、出所、代替案、確信度を表示する
- ssd split <input.ssd>
  - .ssd 本体から分離可能なメタ情報を .ssm へ退避する
- ssd merge <input.ssd>
  - .ssd と .ssm を統合してインライン化した .ssd を生成する
- ssd normalize <input.ssd>
  - 識別子順序、レイアウト、サイドカー配置方針を正規化する

CLI は、少なくとも次のサイドカー運用を支援すること。

- ssd check sample.ssd
  - sample.ssm が存在すれば自動併読して検証できること
- ssd extract --out sample.ssd <input>
  - この段階では inline `.ssd` output に固定してよく、embedding option がある場合だけ paired `.ssm` を追加生成してよい

最初の explicit embedding + raw text extract slice では、`extract` は少なくとも以下を満たすこと。

- 既存 `.ssd` を入力に受けられること
- plain `.txt` を入力に受けられること
- `--embed-provider <provider> --embed-model <model>` を明示したときだけ embedding generation を行うこと
- `--embed-provider` と `--embed-model` は両方そろわない限り失敗すること
- provider は少なくとも `ollama` と `openai` を受け付けること
- raw `.txt` に対する `--stdout` は skeletal `.ssd` を出力できること
- raw `.txt` に対する `--stdout` では embedding option を拒否すること
- no-provider の `--stdout` は 1 件以上の existing `.ssd` / plain `.txt` input から canonical inline `.ssd` を出力できること
- single existing `.ssd` without provider も resolved canonical inline `.ssd` stdout として成功してよいこと
- embedding-enabled `--stdout` は single existing `.ssd` に限って `--format inline|sidecar` を受け付けてよいこと
- embedding-enabled `--stdout` で `--format` を省略した場合の既定値は `sidecar` とし、generated `.ssm` profile を出力できること
- embedding-enabled `--stdout --format sidecar` は generated `.ssm` profile を出力できること
- embedding-enabled `--stdout --format inline` は generated embeddings を含む single-file `.ssd` text を出力できること
- `--format inline` では assertion / hypothesis は inline `meta` を使ってよく、inline `meta` を持てない embeddable kind は同じ stdout payload 内の top-level `meta <id>` block として出力してよいこと
- embedding-enabled `--stdout --format bundle` は single existing `.ssd` に対して line-preserving plain-text bundle を返してよく、inline `.ssd` payload と generated `.ssm` payload を同時に含めてよいこと
- embedding-enabled `--stdout` は existing `.ssd` input が 2 件以上でも `--format inline|sidecar|bundle` を明示した場合に限って成功してよいこと
- multi-input embedding-enabled `--stdout --format inline` は rebased merged view の canonical inline `.ssd` を返してよいこと
- multi-input embedding-enabled `--stdout --format sidecar` は rebased merged IDs を使う generated `.ssm` profile text を返してよいこと
- multi-input embedding-enabled `--stdout --format bundle` は同じ rebased merged view から inline `.ssd` payload と generated `.ssm` payload を同時に返してよいこと
- multi-input extract が inline `.ssd` を返す surface は、`--out`、no-provider `--stdout`、embedding-enabled `--format inline`、bundle 内 inline payload のいずれでも single top-level `document D1` を返すこと
- multi-input aggregate の `document D1` body は `title` と `source_ref` について source document 間の same-value intersection だけを保持し、値が異なる、または存在有無が揃わない field は省略してよいこと
- multi-input aggregate の document-scoped `version` / `generator` と paired `document_meta` 由来 field も same-value intersection だけを保持し、non-consensus field は aggregate output から省略してよいこと
- multi-input embedding-enabled `--stdout` では `--format` omitted を受け付けず、single-input compatibility default と切り分けて failure にしてよいこと
- `--format bundle` の bundle framing は `stdout_profile` / `inline_profile` / `sidecar_profile` header、`inline_document:` / `sidecar_document:` section、payload line の `|` prefix を canonical shape として固定してよいこと
- `--out <file>` では 1 件以上の input から出力先 inline `.ssd` を生成でき、embedding option を伴う場合は paired `.ssm` も生成できること
- `--out <file>` は existing `.ssd` と plain `.txt` を同一 command で混在させてよいこと
- multi-input `--out` は source order を保った resolved view を集約し、source 間の ID 衝突を deterministic rebasing で解決すること
- rebasing は entity ID だけでなく、`alternative_group` と `alternative.group` の shared token にも適用すること
- multi-input `--out` の output `.ssd` は canonical inline document として書かれること
- multi-input `--out` の generated `.ssm` は rebased merged view の ID を使うこと
- no-provider `--stdout` と `extract --out` は引き続き `--format bundle` を受け付けず、bundle は embedding-enabled `--stdout` 専用 surface としてよいこと
- `--out <file>` は non-destructive とし、任意の source input path、任意の source paired `.ssm`、または generated paired `.ssm` と alias する path を拒否すること
- raw `.txt` の embedding target は少なくとも generated `resource.label` と generated `segment.text_quote` に限定すること
- provider 実行失敗や unsupported provider は non-zero failure として返すこと
- ssd explain <id> sample.ssd
  - 本体とサイドカーを統合した表示ができること
- ssd split sample.ssd
  - 既存のインライン記述から sample.ssm を生成できること
- ssd merge sample.ssd
  - sample.ssm が存在すれば単一のインライン .ssd を生成できること
- ssd normalize sample.ssd
  - インライン / サイドカーのいずれか指定したプロファイルへ整形できること

## 8.1 サイドカー操作コマンド

split と merge は、単なるファイル変換ではなく、意味保存変換として定義する。

- split は、意味構造を変えずに、可読性を損なうメタ情報のみを .ssm に移すこと
- merge は、.ssd と .ssm の情報を統合し、単一ファイルで自己完結した表現を得ること
- normalize は、同一意味内容に対して安定した並び順と整形結果を与えること

初期 normalize slice はまず `ssd normalize <input.ssd> --stdout` を canonical inline view として返すことを推奨する。
この stdout 面では、paired `.ssm` が存在すればそれを統合し、存在しなければ standalone `.ssd` を canonical order と formatting へ整形してよい。

paired transform apply の最小拡張として、bare `ssd normalize <input.ssd>` は standalone と paired の両方で成功してよい。
standalone 入力では同一 `.ssd` を canonical inline へ書き換え、paired 入力では `.ssd` を canonical inline へ書き換えたうえで sibling `.ssm` を削除する。
bare `ssd merge <input.ssd>` は paired `.ssm` が存在する入力に限って成功し、統合後の inline `.ssd` を書き戻したうえで sibling `.ssm` を削除する。
paired sidecar がない入力に対する bare merge apply は、この段階では merge と normalize の責務境界を保つため失敗とする。
この段階では `--out`、`--inline`、`--sidecar`、`--dry-run` まで要求しない。

後続 transform slice では、merge / normalize の両方に `--dry-run` と `--out <output.ssd>` を追加してよい。

- `ssd merge <input.ssd> --dry-run` は paired input に限って成功し、in-place apply なら削除される sibling `.ssm` を preview に含めてよい
- `ssd merge <input.ssd> --out <output.ssd>` は paired input に限って成功し、merged inline 出力を別ファイルへ書く
- `ssd normalize <input.ssd> --dry-run` は standalone / paired の両方で成功し、paired input なら in-place apply で削除される sibling `.ssm` を preview に含めてよい
- `ssd normalize <input.ssd> --out <output.ssd>` は standalone / paired の両方で成功し、canonical inline 出力を別ファイルへ書く
- `--out` は non-destructive とし、この slice では source `.ssd` と paired `.ssm` を変更または削除しない
- `--out` は source `.ssd` または paired `.ssm` を alias してはならない
- `--inline`、`--sidecar`、profile selection、conflict policy は引き続き後続 slice に分離してよい

さらに後続 profile slice では、`normalize` の non-destructive surface に限って `--format inline|sidecar` を追加してよい。

- `ssd normalize <input.ssd> --dry-run --format inline|sidecar` は source files を変更せず、指定 profile の preview を返す
- `ssd normalize <input.ssd> --out <output.ssd> --format inline|sidecar` は source files を変更せず、指定 profile の結果を output 側へ書く
- `--format` を省略した `normalize --dry-run` と `normalize --out` は従来どおり inline を既定とする
- `--format sidecar` は `<output>.ssd` と sibling `<output>.ssm` の pair を生成または preview してよい
- bare `normalize <input.ssd>` と `normalize --stdout` は inline-only のままとし、この slice では `--format` を受けない
- `merge` は inline-oriented command、`split` は sidecar-oriented command のままとし、この slice では profile selection を広げない
- merge / sidecar 競合時の conflict policy は引き続き後続 slice に分離してよい

さらに後続 merge conflict slice では、`merge` に限って trailing `--fail-on-conflict` を追加してよい。

- `ssd merge <input.ssd> [--stdout|--dry-run|--out <output.ssd> [--dry-run]] --fail-on-conflict` は pre-merge source comparison で differing duplicate を検出したら失敗する
- comparison scope は inline source の document body にある `version` / `generator` と、inline `meta {}` / paired `meta` / paired `document_meta` に現れる field に限定してよい
- `embedding.*` は block 単位ではなく leaf field 単位で比較してよい
- same-value duplicate は success してよい
- `--fail-on-conflict` を付けない `merge` は current behavior を明示し、paired `.ssm` 側を優先して merged inline view を返してよい
- conflict 発生時は stdout、preview、`--out`、in-place apply のいずれでも output を返さず file mutation もしない
- warning mode は引き続き後続 slice に分離してよい

さらに後続 merge precedence slice では、`merge` に限って trailing `--prefer-source inline|sidecar` を追加してよい。

- `ssd merge <input.ssd> [--stdout|--dry-run|--out <output.ssd> [--dry-run]] --prefer-source inline|sidecar` は sidecar-owned duplicate field に対する merged inline view の優先元だけを切り替えてよい
- `inline` は inline source を優先し、`sidecar` は paired `.ssm` を優先してよい
- scope は current conflict slice と同じく document body の `version` / `generator`、inline `meta {}`、paired `meta`、paired `document_meta`、`embedding.*` leaf field に限定してよい
- duplicate でない field は current merged result のままとしてよい
- `--fail-on-conflict` と `--prefer-source` を同時に指定した場合、differing duplicate があれば precedence 選択より前に failure してよい
- `--prefer-source` は paired `.ssm` がある input に対してのみ受け付けてよく、standalone input では failure としてよい
- stdout、dry-run、`--out`、in-place apply はすべて同じ selected precedence の merged inline view を使ってよい
- warning mode は引き続き後続 slice に分離してよい

この slice の受け入れ例として、少なくとも以下を含めてよい。

- `ssd normalize docs/examples/normalize-source.ssd --dry-run --format sidecar`
- `ssd normalize docs/examples/normalize-source.ssd --out docs/examples/normalized-sidecar.ssd --format sidecar`
- `ssd normalize docs/examples/normalize-standalone-source.ssd --out docs/examples/normalized-standalone-sidecar.ssd --format sidecar`
- `ssd normalize docs/examples/normalize-source.ssd --stdout --format sidecar`
- `ssd normalize docs/examples/normalize-source.ssd --format sidecar`

merge conflict slice の受け入れ例として、少なくとも以下を含めてよい。

- `ssd merge docs/examples/normalize-standalone-source.ssd --stdout --fail-on-conflict`
- `ssd merge docs/examples/merge-same-value-source.ssd --stdout --fail-on-conflict`
- `ssd merge docs/examples/merge-conflict-source.ssd --stdout --fail-on-conflict`
- `ssd merge docs/examples/merge-conflict-source.ssd --dry-run --fail-on-conflict`
- `ssd merge docs/examples/merge-conflict-source.ssd --out docs/examples/merge-conflict-output.ssd --fail-on-conflict`
- `ssd merge docs/examples/merge-conflict-source.ssd --fail-on-conflict`
- `ssd normalize docs/examples/normalize-source.ssd --out docs/examples/normalize-source.ssm --format sidecar`

初期仕様では、少なくとも以下のオプション群を想定する。

- --inline
  - 可能な限り .ssd 本体へ統合する
- --sidecar
  - 分離可能なメタ情報を .ssm へ退避する
- --stdout
  - 結果を標準出力へ出す
- --out <file>
  - 出力先を明示する
- --dry-run
  - 実際には書き込まず、変更対象のみ表示する

## 8.2 変換の整合性

split と merge は、少なくとも以下の性質を満たすこと。

- merge(split(X)) が X と意味的に等価であること
- split(merge(X)) が指定プロファイルに従って安定化すること
- 識別子参照が変わらないこと
- 原文対応、仮説種別、代替案関係が失われないこと

注記: テキスト差分として完全一致を必須にする必要はないが、意味内容の保存は必須とする。

## 8.3 競合時の挙動

merge 時に .ssd と .ssm の間で競合がある場合、CLI は policy を明示契約として選べること。

現行 slice では、少なくとも以下を満たすこと。

- default `merge` は sidecar-owned duplicate field に対して paired `.ssm` を優先する
- `merge --prefer-source inline|sidecar` は sidecar-owned duplicate field に対する優先元を明示選択してよい
- `merge --warn-on-conflict` は differing duplicate を検出しても成功を継続し、warning を stderr に出してよい
- `merge --fail-on-conflict` は differing duplicate を検出したらエラー終了する
- `--fail-on-conflict` は `--warn-on-conflict` より強く、conflict があれば failure を優先する
- same-value duplicate は conflict と扱わない
- `--fail-on-conflict` の判定は merged view ではなく pre-merge source comparison に基づく

`--warn-on-conflict` は merge result 自体を変えず、default precedence または `--prefer-source` で選ばれた merged view を維持すること。
優先元を warning に応じて暗黙に切り替えてはならない。

## 8.4 表示と配布の用途

CLI は、少なくとも以下の 2 用途を支援すること。

- 編集用
  - 人手編集しやすい .ssd + .ssm の分離形を維持する
- 配布用
  - 単独で意味内容を持ち運べるインライン .ssd を生成する

したがって merge は配布やレビュー提出向け、split は日常編集向けの主要操作として位置付ける。

## 8.5 更新操作の位置付け

更新操作を提供する場合、初期仕様ではクエリ言語ではなく CLI サブコマンドとして提供することを推奨する。

少なくとも以下の性質を持たせやすいためである。

- 対象ファイルの明示
- .ssd と .ssm のどちらを更新するかの明示
- dry-run
- 差分確認
- 競合時の失敗または確認

したがって、更新系は当面 CLI に寄せ、.ssq は参照専用として保つ。

## 8.6 初期更新 CLI の最小集合

責務境界を単純に保つため、初期実装の更新系 CLI は最小集合に限定することを推奨する。

最低限の候補は以下とする。

- ssd add <kind> ...
  - 新しいリソース、主張、仮説、代替案を追加する
- ssd set <selector> <field> <value>
  - 単一対象の属性またはメタ属性を設定する
- ssd remove <selector>
  - 対象要素または対象メタ情報を削除する
- ssd annotate <selector> <kind> <text>
  - rationale、caveat、todo などの注記を付与する

この 4 つを最小集合とする理由は以下の通りである。

- add で新規作成を扱える
- set で単純な属性変更を扱える
- remove で削除を扱える
- annotate で説明・根拠・注意事項の追加を、構造更新と分離して扱える

一方で、初期実装では以下を必須にしない。

- 複数対象一括更新
- 条件式ベース更新
- 依存関係を跨ぐ複合 mutation
- 対話的マージ解決

## 8.6.1 更新対象 selector の最小構文

更新系 CLI が受け付ける <selector> は、初期仕様では単一対象を一意に指せる最小構文に限定することを推奨する。

最低限、以下の selector 形式をサポートする。

- id:<id>
  - 識別子そのものを指す
- type:<kind>
  - 型を指す。ただし更新系では単独使用を既定で禁止し、--allow-multi がある場合のみ許可してよい
- path:<id>.<field>
  - 対象要素の単一フィールドを指す
- meta:<id>.<field>
  - 主として .ssm に載る補助メタ情報フィールドを指す
- doc:self
  - 文書自身を指す

例:

- ssd set id:A12 label "東京本社"
- ssd set path:A12.confidence 0.82
- ssd set meta:A12.embedding.model text-embedding-3-large
- ssd annotate id:H3 rationale "原文に主語が省略されているため補完した"
- ssd remove meta:A12.embedding

初期仕様では、selector は 1 コマンドにつき 1 対象に解決できることを原則とする。
複数対象に一致する selector は、既定で失敗させる。

## 8.6.2 add の対象種別

初期 add slice の `ssd add <kind>` は、少なくとも以下の inline structural kind を扱うこと。

- resource
- segment
- assertion
- hypothesis
- alternative

`provenance` と `annotation` は後続 slice に分離してよい。

各 kind は、最低限 required field を持つこと。
required field が不足する場合は対話補完ではなく失敗を返し、明示入力を促す。

初期 add slice の required field は次を最小とする。

- resource: id, type, label
- segment: id, source, text_quote
- assertion: id, subject, predicate, object
- hypothesis: id, about, kind, summary
- alternative: id, group, label

初期 add slice は `.ssd` 本体への inline add と `--dry-run` を先に実装し、
sidecar-only add と `--out` / `--stdout` / `--target` は後続 slice に分離してよい。

後続 add slice では、metadata-only kind として `annotation` と `provenance` を追加してよい。

- `annotation`: required field は id, kind, text
- `provenance`: required field は id, provenance_kind
- `provenance` では confidence と rationale を optional field として受け付けてよい
- metadata-only add は `--target sidecar` を必須とする
- metadata-only add は create-only とし、対象 id の sidecar metadata field が未存在のときだけ成功する
- paired `.ssm` が存在しない場合は、新規作成してよい
- 既存 field の更新は `ssd set` または `ssd annotate` の責務に残す

さらに後続 metadata-only add output slice では、`annotation` と `provenance` に限って sidecar-only の `--stdout` と `--out <output.ssm>` を追加してよい。

- `--target sidecar` 必須と create-only semantics は維持してよい
- `ssd add <metadata-kind> <file> [field=value ...] --target sidecar --stdout` は source `.ssd` と source sibling `.ssm` を変更せず、add 後の canonical sidecar `.ssm` text を stdout へ返してよい
- `ssd add <metadata-kind> <file> [field=value ...] --target sidecar --out <output.ssm>` は source `.ssd` と source sibling `.ssm` を変更せず、add 後の canonical sidecar `.ssm` text を output file へ書いてよい
- `ssd add <metadata-kind> <file> [field=value ...] --target sidecar --out <output.ssm> --dry-run` は target file を output path に向けた preview として扱ってよい
- bare `--dry-run` は sibling sidecar path を target file とする preview として扱ってよい
- `--stdout` は `--dry-run` や `--out` と併用してはならない
- `--out` は source `.ssd` や source sibling `.ssm` を alias してはならない
- inline target、auto target、upsert、broader conflict policy はこの slice に含めない

さらに後続 structural add output slice では、inline structural kind に限って `--stdout` と `--out <output.ssd>` を追加してよい。

- 対象 kind は resource / segment / assertion / hypothesis / alternative に限定してよい
- `ssd add <structural-kind> <file> [field=value ...] --stdout` は source file を変更せず、追加後の canonical inline `.ssd` を stdout へ返してよい
- `ssd add <structural-kind> <file> [field=value ...] --out <output.ssd>` は source `.ssd` と paired `.ssm` を変更せず、追加後の canonical inline `.ssd` を output file へ書いてよい
- `ssd add <structural-kind> <file> [field=value ...] --out <output.ssd> --dry-run` は target file を output path に向けた preview として扱ってよい
- `--stdout` と `--out` は metadata-only add には広げず、`annotation` / `provenance` では failure のままにしてよい
- `--stdout` は `--dry-run` や `--out` と併用してはならない
- `--out` は source `.ssd` や paired `.ssm` を alias してはならない

## 8.6.3 set と annotate の責務差分

set は、既存要素の正規フィールドまたはメタフィールドを更新する操作とする。

- 例: label、confidence、status、source_ref

現行 set output slice では、existing single-target selector semantics を維持したまま `--stdout`、`--out <output-file>`、`--out <output-file> --dry-run` を追加してよい。

- `ssd set path:<id>.<field> <value> --stdout <file>` と `ssd set id:<id> <field> <value> --stdout <file>` は source file を変更せず、canonical inline `.ssd` を stdout へ返してよい
- `ssd set meta:<id>.<field> <value> --stdout <file>` は source file を変更せず、canonical sidecar `.ssm` を stdout へ返してよい
- `ssd set <selector> ... --out <output-file> <file>` は source `.ssd` と source sibling `.ssm` を変更せず、resolved write profile の canonical text を output file へ書いてよい
- `ssd set <selector> ... --out <output-file> --dry-run <file>` は existing set preview format を再利用しつつ target file を output path に向けてよい
- `--stdout` は `--dry-run` や `--out` と併用してはならない
- set output options の accepted order は `--stdout`、`--dry-run`、または `--out <output-file> [--dry-run]` に限ってよく、`--dry-run --out ...` のような未規定順序は failure としてよい
- `--out` は source `.ssd` を alias してはならず、sidecar write profile の場合は source sibling `.ssm` も alias してはならない

次の set id-list slice では、current id-selector semantics を保ったまま explicit id list だけを追加してよい。

- syntax は `ssd set id:<id>,id:<id>,... <field> <value> ... <file>` に限定してよい
- list atom は `id:` に限定してよく、`path:`、`meta:`、`type:`、`doc:self`、mixed list は failure としてよい
- apply / `--dry-run` / `--stdout` / `--out` / `--out --dry-run` のいずれでも mutation 前に全 id を解決・検証し、1 件でも missing target や wrong-layer があれば全体 failure としてよい
- duplicate id は first-seen order で dedup し、preview / apply の `changes` は dedup 後 target 数に一致してよい
- id list set の result payload は current id set と同じ canonical inline `.ssd` に固定してよい

次の set type allow-multi slice では、current set ownership を保ったまま `type:<kind>` に限定した multi-target set を追加してよい。

- syntax は `ssd set type:<kind> <field> <value> --allow-multi ... <file>` に限定してよい
- `type:<kind>` を使う set surface では matched target 数に関わらず `--allow-multi` を常に必須としてよい
- apply / `--dry-run` / `--stdout` / `--out` / `--out --dry-run` のいずれでも mutation 前に matched target 全件を解決・検証し、1 件でも missing target や wrong-layer があれば全体 failure としてよい
- matched target order は matched id の ascending order に固定してよく、preview / apply / output の `changes` はその順序で確定した target 数に一致してよい
- result payload は current set output surface をそのまま再利用し、canonical inline `.ssd` に固定してよい

annotate は、説明、注意、根拠、TODO などの付加注記を追加する操作とする。

- 例: rationale、caveat、todo
- `--target sidecar` は paired `.ssm` がある入力で通常の sidecar 更新として扱う
- `--target inline` は standalone `.ssd` 入力に限り許可し、対象 kind は `assertion` と `hypothesis` に限定する
- `--target auto` は paired 入力では sidecar を選び、standalone 入力では inline が許される `assertion` / `hypothesis` のみ inline を選び、それ以外は sidecar へ落としてよい

現行 annotate output slice では、既存 target matrix を維持したまま `--stdout`、`--out <output-file>`、`--out <output-file> --dry-run` を追加してよい。

- `ssd annotate <selector> <kind> <text> --target inline|sidecar|auto --stdout <file>` は source file を変更せず、resolved target が `inline` なら canonical inline `.ssd`、`sidecar` なら canonical sidecar `.ssm` を stdout へ返してよい
- `ssd annotate <selector> <kind> <text> --target inline|sidecar|auto --out <output-file> <file>` は source `.ssd` と source sibling `.ssm` を変更せず、resolved target profile の canonical text を output file へ書いてよい
- `ssd annotate <selector> <kind> <text> --target inline|sidecar|auto --out <output-file> --dry-run <file>` は既存 annotate preview format を再利用しつつ target file を output path に向けてよい
- `--stdout` は `--dry-run` や `--out` と併用してはならない
- `--out` は source `.ssd` を alias してはならず、resolved target が `sidecar` の場合は source sibling `.ssm` も alias してはならない

次の annotate id-list slice では、current annotate ownership を保ったまま explicit id list だけを追加してよい。

- syntax は `ssd annotate id:<id>,id:<id>,... <kind> <text> --target inline|sidecar ... <file>` に限定してよい
- list atom は `id:` に限定してよく、`type:`、`path:`、`meta:`、`doc:self`、mixed list は failure としてよい
- `--target inline` と `--target sidecar` だけを許可してよく、`--target auto` はこの slice では failure としてよい
- apply / `--dry-run` / `--stdout` / `--out` / `--out --dry-run` のいずれでも mutation 前に全 id を解決・検証し、1 件でも missing target、inline unsupported、standalone requirement violation があれば全体 failure としてよい
- duplicate id は first-seen order で dedup してよく、preview / apply / output の `changes` は dedup 後 target 数に一致してよい
- result payload は current annotate output surface をそのまま再利用し、`--target inline` は canonical inline `.ssd`、`--target sidecar` は canonical sidecar `.ssm` に固定してよい

次の annotate type allow-multi sidecar slice では、current annotate sidecar ownership を保ったまま `type:<kind>` selector による opt-in multi-target annotate を追加してよい。

- syntax は `ssd annotate type:<kind> <kind> <text> --target sidecar --allow-multi ... <file>` に限定してよい
- `type:<kind>` annotate では matched target 数に関わらず `--allow-multi` を常に必須としてよい
- apply / `--dry-run` / `--stdout` / `--out` / `--out --dry-run` のいずれでも mutation 前に matched target 全件を解決・検証し、0 件一致なら missing-target failure としてよい
- matched target order は matched id の ascending order に固定してよく、preview / apply / output の `changes` はその順序で確定した target 数に一致してよい
- target profile は `sidecar` のみに固定してよく、paired input / standalone input のどちらでも current sidecar annotate と同じ sibling `.ssm` write contract を再利用してよい
- `--target inline` と `--target auto` はこの slice では failure としてよい
- result payload は current annotate sidecar output surface をそのまま再利用し、canonical sidecar `.ssm` に固定してよい
- generic selector list、type selector の inline / auto multi-target routing、mixed target profile resolution は引き続き後続 slice に分離してよい

field-map から create-only に sidecar metadata を起こす経路が必要な場合は、
`ssd add annotation --target sidecar` を別経路として追加してよい。
この場合でも annotate 自体は shorthand command として残し、既存注記の更新責務は annotate または set に残す。

この区別により、構造変更と説明文追加を分離する。

## 8.6.4 remove の安全制約

remove は、初期仕様では以下の制約を持つことを推奨する。

- 参照されている構造要素の削除は既定で失敗する
- 補助メタ情報の削除は、構造整合性を壊さない限り許可する
- 構造削除は単一 target selector に限定し、複数 target 一括削除は既存 `type:<kind> --allow-multi` の deferred surface に残す
- `--cascade` が明示された場合のみ従属要素削除を許可してよい
- `type:<kind> --allow-multi --cascade` は既存 cascade edge の closure を matched set 全体へ拡張してよい
- 初期 cascade edge は次に固定する
  - assertion <- hypothesis.about
  - hypothesis <- alternative.group
  - resource <- segment.source
- `--cascade` は direct / transitive dependents を同時削除し、target だけを残す部分削除はしない

現行 slice では `--allow-multi` は引き続き `type:<kind>` に限定してよく、broader multi-target selector semantics は次の remove list slice へ分離してよい。

次の remove list slice では、remove 専用に comma-separated explicit selector list を追加してよい。

- syntax は `selector1,selector2,...` に限定してよい
- list atom は `id:<id>`、`path:<id>.<field>`、`type:<kind>` に限定してよい
- `meta:<id>.<field>` list と `doc:self` list はこの slice に含めない
- explicit `id:` / `path:` list は、それ自体で multi-target intent として扱ってよい
- list に含まれる `type:<kind>` atom が複数 target に展開される場合だけ、既存の `--allow-multi` を要求してよい
- `--cascade`、`--dry-run`、`--stdout`、`--out <output.ssd> [--dry-run]` は resolved root set の union に対して既存 remove surface をそのまま適用してよい
- duplicate target は resolved id union に正規化して 1 回だけ remove してよい
- safety check は union remove set の外に dependent が残るかどうかで判定してよい
- option ordering は current contract を維持し、output-before-safety reorder は failure のままとしてよい

現行 remove output surface では、selector semantics を広げずに `--dry-run`、`--stdout`、`--out <output.ssd>` を追加してよい。

- `ssd remove <selector> [--allow-multi [--cascade]|--cascade] --dry-run <file>` は apply と同じ validation を通した preview を返してよい
- `ssd remove <selector> [--allow-multi [--cascade]|--cascade] --stdout <file>` は source file を変更せず、remove 後の canonical inline `.ssd` を stdout へ返してよい
- `ssd remove <selector> [--allow-multi [--cascade]|--cascade] --out <output.ssd> <file>` は source `.ssd` と paired `.ssm` を変更せず、remove 後の canonical inline `.ssd` を output file へ書いてよい
- `ssd remove <selector> [--allow-multi [--cascade]|--cascade] --out <output.ssd> --dry-run <file>` は target file を output path に向けた preview として扱ってよい
- `--stdout` は `--dry-run` や `--out` と併用してはならない
- `--out` は source `.ssd` や paired `.ssm` を alias してはならない
- paired input でも non-destructive output は inline canonical `.ssd` に固定してよく、sidecar pair や `--format` はこの slice に含めない

注記: これにより remove を最小限に保ちつつ、誤削除を避ける。

## 8.7 更新 CLI の共通オプション

更新系 CLI は少なくとも以下の共通オプションを持つことを推奨する。

- --target inline|sidecar|auto
  - 更新先を .ssd 本体、.ssm、または自動決定から選ぶ
  - 初期実装では annotate が `inline|sidecar|auto` を使い、metadata-only add は `sidecar` のみを使う
- --dry-run
  - 実際には書き込まず変更内容のみ表示する
- --out <file>
  - 破壊的上書きではなく別ファイルへ出力する
- --format inline|sidecar
  - 出力プロファイルを指定する
  - 初期実装では normalize の `--dry-run` と `--out` がこの option を使い、help render の `--format semdl` とは別役割とする
- --fail-on-conflict
  - 現行 slice では merge 専用で、merge surface の末尾に付くときだけ競合検出時に失敗させる

remove の safety control では、現行 slice で `type:<kind> --allow-multi --cascade` を許可してよい。

## 8.8 更新 CLI とサイドカーの相互作用

更新系 CLI は、サイドカー方式を採る場合に少なくとも以下を満たすこと。

- 構造更新は既定で .ssd を優先する
- 補助メタ情報更新は既定で .ssm を優先する
- 本体へ残すべき最小情報を失わせない
- split / merge / normalize 後も識別子参照が安定する

例として、主張に対する確信度変更は .ssm へ書くのが自然だが、主張が仮説である種別そのものを消す更新は .ssd 側の整合性検証を伴う。

## 8.9 レイヤ対応表

SEMDL の各レイヤの責務を、初期仕様では以下のように整理する。

| レイヤ | 主用途 | 読み書き | 主対象 | 代表操作 | 備考 |
| --- | --- | --- | --- | --- | --- |
| .ssd | 意味構造本体 | 読み書き | リソース、主張、仮説、代替案の骨格 | 定義、参照、最小属性保持 | 人手可読性を優先 |
| .ssm | 補助メタ情報 | 読み書き | 出所詳細、埋め込み、詳細修飾、処理履歴 | 付加、更新、退避 | 可読性維持のため外出し |
| .ssq | 参照クエリ | 読み取り専用 | 統合ビュー | search、filter、similarity query | 初期仕様では更新を持たない |
| ssd CLI | 運用と更新 | 読み書き | ファイル、構造、メタ情報 | check、extract、split、merge、normalize、add、set、remove、annotate | 初期更新の主経路 |
| semql query | 将来の参照言語 | 読み取り専用 | 統合ビュー | query | .ssq を包含または置換しうる |
| semql mutation | 将来の更新言語 | 書き込み | 構造更新とメタ更新 | add、set、remove、annotate 相当 | query と分離する |
| semql admin / transform | 将来の運用変換 | 書き込み | 文書単位運用 | split、merge、normalize | mutation と責務分離する |

この表は、少なくとも以下を示すための基準とする。

- 何を人手編集対象とするか
- 何を検索専用に保つか
- 何を CLI で先行実装するか
- semql 導入時にどこまでを別レイヤに切り出すか

## 8.10 テストファースト前提の CLI 受け入れ例

SEMDL の CLI 仕様は、実装より先に受け入れ例を固定するテストファースト方針で進めることを推奨する。

このとき、以下を原則とする。

- 仕様追加時は、先に期待入出力を文章またはサンプルで固定する
- CLI の例は、将来そのまま acceptance test または golden test に転用できる形で書く
- 例に使う入力ファイルは .ssd / .ssm の最小サンプルとして同梱する

初期受け入れ例として、少なくとも以下を維持することを推奨する。

前提ファイル:

- [docs/examples/minimal.ssd](docs/examples/minimal.ssd)
- [docs/examples/minimal.ssm](docs/examples/minimal.ssm)
- [docs/examples/minimal.inline.ssd](docs/examples/minimal.inline.ssd)

test runner 定義:

- [docs/test-runner-format.md](docs/test-runner-format.md)
- [docs/examples/testcases/cli-success.json](docs/examples/testcases/cli-success.json)
- [docs/examples/testcases/cli-failure.json](docs/examples/testcases/cli-failure.json)

期待出力ファイル:

- [docs/examples/golden/check-minimal.stdout](docs/examples/golden/check-minimal.stdout)
- [docs/examples/golden/explain-A1.stdout](docs/examples/golden/explain-A1.stdout)
- [docs/examples/golden/set-path-A1-label.dryrun.stdout](docs/examples/golden/set-path-A1-label.dryrun.stdout)
- [docs/examples/golden/set-path-A1-label.apply.stdout](docs/examples/golden/set-path-A1-label.apply.stdout)
- [docs/examples/golden/set-path-A1-label.out.stdout](docs/examples/golden/set-path-A1-label.out.stdout)
- [docs/examples/golden/set-path-A1-label.out.dryrun.stdout](docs/examples/golden/set-path-A1-label.out.dryrun.stdout)
- [docs/examples/golden/set-id-list-A1-ALT1A-label.dryrun.stdout](docs/examples/golden/set-id-list-A1-ALT1A-label.dryrun.stdout)
- [docs/examples/golden/set-id-list-A1-ALT1A-label.apply.stdout](docs/examples/golden/set-id-list-A1-ALT1A-label.apply.stdout)
- [docs/examples/golden/set-id-list-A1-ALT1A-label.out.stdout](docs/examples/golden/set-id-list-A1-ALT1A-label.out.stdout)
- [docs/examples/golden/set-id-list-A1-ALT1A-label.out.dryrun.stdout](docs/examples/golden/set-id-list-A1-ALT1A-label.out.dryrun.stdout)
- [docs/examples/golden/set-type-alternative-label-allow-multi.dryrun.stdout](docs/examples/golden/set-type-alternative-label-allow-multi.dryrun.stdout)
- [docs/examples/golden/set-type-alternative-label-allow-multi.apply.stdout](docs/examples/golden/set-type-alternative-label-allow-multi.apply.stdout)
- [docs/examples/golden/set-type-alternative-label-allow-multi.out.stdout](docs/examples/golden/set-type-alternative-label-allow-multi.out.stdout)
- [docs/examples/golden/set-type-alternative-label-allow-multi.out.dryrun.stdout](docs/examples/golden/set-type-alternative-label-allow-multi.out.dryrun.stdout)
- [docs/examples/golden/set-id-A1-label.out.stdout](docs/examples/golden/set-id-A1-label.out.stdout)
- [docs/examples/golden/set-id-A1-label.out.dryrun.stdout](docs/examples/golden/set-id-A1-label.out.dryrun.stdout)
- [docs/examples/golden/set-meta-A1-confidence.dryrun.stdout](docs/examples/golden/set-meta-A1-confidence.dryrun.stdout)
- [docs/examples/golden/set-meta-A1-confidence.apply.stdout](docs/examples/golden/set-meta-A1-confidence.apply.stdout)
- [docs/examples/golden/set-meta-A1-confidence.out.stdout](docs/examples/golden/set-meta-A1-confidence.out.stdout)
- [docs/examples/golden/set-meta-A1-confidence.out.dryrun.stdout](docs/examples/golden/set-meta-A1-confidence.out.dryrun.stdout)
- [docs/examples/golden/annotate-H1-rationale.dryrun.stdout](docs/examples/golden/annotate-H1-rationale.dryrun.stdout)
- [docs/examples/golden/annotate-H1-todo.apply.stdout](docs/examples/golden/annotate-H1-todo.apply.stdout)
- [docs/examples/golden/annotate-H1-rationale-sidecar.out.dryrun.stdout](docs/examples/golden/annotate-H1-rationale-sidecar.out.dryrun.stdout)
- [docs/examples/golden/annotate-H1-status-inline.apply.stdout](docs/examples/golden/annotate-H1-status-inline.apply.stdout)
- [docs/examples/golden/annotate-H1-status-inline.out.dryrun.stdout](docs/examples/golden/annotate-H1-status-inline.out.dryrun.stdout)
- [docs/examples/golden/annotate-R1-rationale-auto-sidecar.apply.stdout](docs/examples/golden/annotate-R1-rationale-auto-sidecar.apply.stdout)
- [docs/examples/golden/annotate-R1-rationale-auto-sidecar.out.stdout](docs/examples/golden/annotate-R1-rationale-auto-sidecar.out.stdout)
- [docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.dryrun.stdout](docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.dryrun.stdout)
- [docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.apply.stdout](docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.apply.stdout)
- [docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.out.stdout](docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.out.stdout)
- [docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.out.dryrun.stdout](docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.out.dryrun.stdout)
- [docs/examples/golden/annotate-id-list-A1-H1-status-inline.stdout](docs/examples/golden/annotate-id-list-A1-H1-status-inline.stdout)
- [docs/examples/golden/annotate-type-alternative-rationale-sidecar.dryrun.stdout](docs/examples/golden/annotate-type-alternative-rationale-sidecar.dryrun.stdout)
- [docs/examples/golden/annotate-type-alternative-rationale-sidecar.apply.stdout](docs/examples/golden/annotate-type-alternative-rationale-sidecar.apply.stdout)
- [docs/examples/golden/annotate-type-alternative-rationale-sidecar.out.stdout](docs/examples/golden/annotate-type-alternative-rationale-sidecar.out.stdout)
- [docs/examples/golden/annotate-type-alternative-rationale-sidecar.out.dryrun.stdout](docs/examples/golden/annotate-type-alternative-rationale-sidecar.out.dryrun.stdout)
- [docs/examples/golden/annotate-type-alternative-rationale-sidecar.stdout](docs/examples/golden/annotate-type-alternative-rationale-sidecar.stdout)
- [docs/examples/golden/split-minimal.dryrun.stdout](docs/examples/golden/split-minimal.dryrun.stdout)
- [docs/examples/golden/split-minimal.apply.stdout](docs/examples/golden/split-minimal.apply.stdout)
- [docs/examples/golden/remove-A1-embedding.apply.stdout](docs/examples/golden/remove-A1-embedding.apply.stdout)
- [docs/examples/golden/remove-A1-embedding.dryrun.stdout](docs/examples/golden/remove-A1-embedding.dryrun.stdout)
- [docs/examples/golden/remove-A1-embedding.out.stdout](docs/examples/golden/remove-A1-embedding.out.stdout)
- [docs/examples/golden/remove-ALT1B.apply.stdout](docs/examples/golden/remove-ALT1B.apply.stdout)
- [docs/examples/golden/remove-ALT1B.dryrun.stdout](docs/examples/golden/remove-ALT1B.dryrun.stdout)
- [docs/examples/golden/remove-ALT1B.out.stdout](docs/examples/golden/remove-ALT1B.out.stdout)
- [docs/examples/golden/remove-type-alternative-allow-multi.apply.stdout](docs/examples/golden/remove-type-alternative-allow-multi.apply.stdout)
- [docs/examples/golden/remove-type-alternative-allow-multi.dryrun.stdout](docs/examples/golden/remove-type-alternative-allow-multi.dryrun.stdout)
- [docs/examples/golden/remove-type-alternative-allow-multi.out.stdout](docs/examples/golden/remove-type-alternative-allow-multi.out.stdout)
- [docs/examples/golden/remove-H1-cascade.apply.stdout](docs/examples/golden/remove-H1-cascade.apply.stdout)
- [docs/examples/golden/remove-H1-cascade.dryrun.stdout](docs/examples/golden/remove-H1-cascade.dryrun.stdout)
- [docs/examples/golden/remove-H1-cascade.out.stdout](docs/examples/golden/remove-H1-cascade.out.stdout)
- [docs/examples/golden/remove-type-hypothesis-allow-multi-cascade-out.dryrun.stdout](docs/examples/golden/remove-type-hypothesis-allow-multi-cascade-out.dryrun.stdout)
- [docs/examples/golden/remove-type-hypothesis-allow-multi-cascade.dryrun.stdout](docs/examples/golden/remove-type-hypothesis-allow-multi-cascade.dryrun.stdout)
- [docs/examples/golden/remove-type-hypothesis-allow-multi-cascade.out.stdout](docs/examples/golden/remove-type-hypothesis-allow-multi-cascade.out.stdout)
- [docs/examples/golden/remove-A1-cascade.apply.stdout](docs/examples/golden/remove-A1-cascade.apply.stdout)
- [docs/examples/golden/remove-type-alternative.error.stderr](docs/examples/golden/remove-type-alternative.error.stderr)
- [docs/examples/golden/remove-ALT1B-stdout-dryrun-invalid.error.stderr](docs/examples/golden/remove-ALT1B-stdout-dryrun-invalid.error.stderr)
- [docs/examples/golden/remove-ALT1B-stdout-out-invalid.error.stderr](docs/examples/golden/remove-ALT1B-stdout-out-invalid.error.stderr)
- [docs/examples/golden/remove-type-hypothesis-stdout-before-allow-multi.error.stderr](docs/examples/golden/remove-type-hypothesis-stdout-before-allow-multi.error.stderr)
- [docs/examples/golden/remove-type-hypothesis-out-before-allow-multi.error.stderr](docs/examples/golden/remove-type-hypothesis-out-before-allow-multi.error.stderr)
- [docs/examples/golden/remove-ALT1B-out-alias-source.error.stderr](docs/examples/golden/remove-ALT1B-out-alias-source.error.stderr)
- [docs/examples/golden/remove-ALT1B-out-alias-sidecar.error.stderr](docs/examples/golden/remove-ALT1B-out-alias-sidecar.error.stderr)
- [docs/examples/golden/remove-ALT1B-out-alias-source-equivalent.error.stderr](docs/examples/golden/remove-ALT1B-out-alias-source-equivalent.error.stderr)
- [docs/examples/golden/remove-ALT1B-out-alias-sidecar-equivalent.error.stderr](docs/examples/golden/remove-ALT1B-out-alias-sidecar-equivalent.error.stderr)
- [docs/examples/golden/remove-id-A1.error.stderr](docs/examples/golden/remove-id-A1.error.stderr)
- [docs/examples/golden/remove-H1-references.error.stderr](docs/examples/golden/remove-H1-references.error.stderr)
- [docs/examples/golden/remove-meta-missing-dryrun.error.stderr](docs/examples/golden/remove-meta-missing-dryrun.error.stderr)
- [docs/examples/golden/remove-R1-references.error.stderr](docs/examples/golden/remove-R1-references.error.stderr)
- [docs/examples/golden/annotate-invalid-kind.error.stderr](docs/examples/golden/annotate-invalid-kind.error.stderr)
- [docs/examples/golden/annotate-inline-unsupported-resource.error.stderr](docs/examples/golden/annotate-inline-unsupported-resource.error.stderr)
- [docs/examples/golden/annotate-inline-requires-standalone.error.stderr](docs/examples/golden/annotate-inline-requires-standalone.error.stderr)
- [docs/examples/golden/annotate-id-list-invalid-selector.error.stderr](docs/examples/golden/annotate-id-list-invalid-selector.error.stderr)
- [docs/examples/golden/annotate-id-list-auto-target-unsupported.error.stderr](docs/examples/golden/annotate-id-list-auto-target-unsupported.error.stderr)
- [docs/examples/golden/annotate-id-list-missing-target.error.stderr](docs/examples/golden/annotate-id-list-missing-target.error.stderr)
- [docs/examples/golden/annotate-id-list-inline-unsupported.error.stderr](docs/examples/golden/annotate-id-list-inline-unsupported.error.stderr)
- [docs/examples/golden/annotate-stdout-dryrun-invalid.error.stderr](docs/examples/golden/annotate-stdout-dryrun-invalid.error.stderr)
- [docs/examples/golden/annotate-stdout-out-invalid.error.stderr](docs/examples/golden/annotate-stdout-out-invalid.error.stderr)
- [docs/examples/golden/annotate-out-alias-input.error.stderr](docs/examples/golden/annotate-out-alias-input.error.stderr)
- [docs/examples/golden/annotate-out-alias-sidecar.error.stderr](docs/examples/golden/annotate-out-alias-sidecar.error.stderr)
- [docs/examples/golden/annotate-auto-out-alias-standalone-sidecar.error.stderr](docs/examples/golden/annotate-auto-out-alias-standalone-sidecar.error.stderr)
- [docs/examples/golden/annotate-type-alternative-missing-allow-multi.error.stderr](docs/examples/golden/annotate-type-alternative-missing-allow-multi.error.stderr)
- [docs/examples/golden/annotate-type-alternative-inline-target.error.stderr](docs/examples/golden/annotate-type-alternative-inline-target.error.stderr)
- [docs/examples/golden/annotate-type-alternative-auto-target.error.stderr](docs/examples/golden/annotate-type-alternative-auto-target.error.stderr)
- [docs/examples/golden/annotate-type-alternative-order-invalid.error.stderr](docs/examples/golden/annotate-type-alternative-order-invalid.error.stderr)
- [docs/examples/golden/annotate-type-alternative-dryrun-out-invalid.error.stderr](docs/examples/golden/annotate-type-alternative-dryrun-out-invalid.error.stderr)
- [docs/examples/golden/annotate-type-claim-missing-target.error.stderr](docs/examples/golden/annotate-type-claim-missing-target.error.stderr)
- [docs/examples/golden/annotate-type-alternative-out-alias-sidecar.error.stderr](docs/examples/golden/annotate-type-alternative-out-alias-sidecar.error.stderr)
- [docs/examples/golden/set-stdout-dryrun-invalid.error.stderr](docs/examples/golden/set-stdout-dryrun-invalid.error.stderr)
- [docs/examples/golden/set-stdout-out-invalid.error.stderr](docs/examples/golden/set-stdout-out-invalid.error.stderr)
- [docs/examples/golden/set-dryrun-out-order-invalid.error.stderr](docs/examples/golden/set-dryrun-out-order-invalid.error.stderr)
- [docs/examples/golden/set-id-list-invalid-selector.error.stderr](docs/examples/golden/set-id-list-invalid-selector.error.stderr)
- [docs/examples/golden/set-id-list-missing-target.error.stderr](docs/examples/golden/set-id-list-missing-target.error.stderr)
- [docs/examples/golden/set-id-list-wrong-layer.error.stderr](docs/examples/golden/set-id-list-wrong-layer.error.stderr)
- [docs/examples/golden/set-type-assertion-missing-allow-multi.error.stderr](docs/examples/golden/set-type-assertion-missing-allow-multi.error.stderr)
- [docs/examples/golden/set-type-alternative-order-invalid.error.stderr](docs/examples/golden/set-type-alternative-order-invalid.error.stderr)
- [docs/examples/golden/set-type-alternative-stdout-before-allow-multi.error.stderr](docs/examples/golden/set-type-alternative-stdout-before-allow-multi.error.stderr)
- [docs/examples/golden/set-type-alternative-out-before-allow-multi.error.stderr](docs/examples/golden/set-type-alternative-out-before-allow-multi.error.stderr)
- [docs/examples/golden/set-type-alternative-missing-target.error.stderr](docs/examples/golden/set-type-alternative-missing-target.error.stderr)
- [docs/examples/golden/set-type-assertion-wrong-layer-allow-multi.error.stderr](docs/examples/golden/set-type-assertion-wrong-layer-allow-multi.error.stderr)
- [docs/examples/golden/set-out-alias-input.error.stderr](docs/examples/golden/set-out-alias-input.error.stderr)
- [docs/examples/golden/set-out-alias-sidecar.error.stderr](docs/examples/golden/set-out-alias-sidecar.error.stderr)
- [docs/examples/golden/set-out-alias-standalone-sidecar.error.stderr](docs/examples/golden/set-out-alias-standalone-sidecar.error.stderr)
- [docs/examples/golden/set-malformed-selector.error.stderr](docs/examples/golden/set-malformed-selector.error.stderr)
- [docs/examples/golden/check-unknown-option.error.stderr](docs/examples/golden/check-unknown-option.error.stderr)
- [docs/examples/golden/check-trailing-unknown-option.error.stderr](docs/examples/golden/check-trailing-unknown-option.error.stderr)
- [docs/examples/golden/annotate-unterminated-quoted-string.error.stderr](docs/examples/golden/annotate-unterminated-quoted-string.error.stderr)
- [docs/examples/golden/set-id-missing-target.error.stderr](docs/examples/golden/set-id-missing-target.error.stderr)
- [docs/examples/golden/set-path-wrong-layer.error.stderr](docs/examples/golden/set-path-wrong-layer.error.stderr)
- [docs/examples/golden/set-meta-wrong-layer.error.stderr](docs/examples/golden/set-meta-wrong-layer.error.stderr)

受け入れ例:

- `ssd check docs/examples/minimal.ssd`
  - 同ベース名の .ssm を自動検出し、整合性エラーなしで終了する
  - 期待出力は [docs/examples/golden/check-minimal.stdout](docs/examples/golden/check-minimal.stdout) と一致する
- `ssd explain A1 docs/examples/minimal.ssd`
  - A1 の構造本体は .ssd から、confidence や embedding 情報は .ssm から統合表示する
  - 期待出力は [docs/examples/golden/explain-A1.stdout](docs/examples/golden/explain-A1.stdout) と一致する
- `ssd search docs/query/valid-range-filter.ssq docs/examples/minimal.ssd`
  - merged view の numeric metadata field に対する range filter が成功する
  - 期待出力は [docs/examples/golden/search-valid-range-filter.stdout](docs/examples/golden/search-valid-range-filter.stdout) と一致する
- `ssd search docs/query/valid-logical-and-filter.ssq docs/examples/minimal.ssd`
  - inline と sidecar の field を跨いだ homogeneous `and` chain が成功する
  - 期待出力は [docs/examples/golden/search-valid-logical-and-filter.stdout](docs/examples/golden/search-valid-logical-and-filter.stdout) と一致する
- `ssd search docs/query/valid-logical-or-filter.ssq docs/examples/minimal.ssd`
  - homogeneous `or` chain が成功し、いずれかの term を満たす candidate を返す
  - 期待出力は [docs/examples/golden/search-valid-logical-or-filter.stdout](docs/examples/golden/search-valid-logical-or-filter.stdout) と一致する
- `ssd search docs/query/valid-mixed-logical-filter.ssq docs/examples/minimal.ssd`
  - mixed `and` / `or` expression は `and` 優先で評価してよい
  - 期待出力は [docs/examples/golden/search-valid-mixed-logical-filter.stdout](docs/examples/golden/search-valid-mixed-logical-filter.stdout) と一致する
- `ssd search docs/query/valid-parenthesized-logical-filter.ssq docs/examples/minimal.ssd`
  - parenthesized grouping は default precedence を override してよい
  - 期待出力は [docs/examples/golden/search-valid-parenthesized-logical-filter.stdout](docs/examples/golden/search-valid-parenthesized-logical-filter.stdout) と一致する
- `ssd search docs/query/valid-parenthesized-quoted-string-filter.ssq docs/examples/minimal.ssd`
  - quoted string 内の parentheses は grouping token として解釈せず leaf value のまま扱う
  - 期待出力は [docs/examples/golden/search-valid-parenthesized-quoted-string-filter.stdout](docs/examples/golden/search-valid-parenthesized-quoted-string-filter.stdout) と一致する
- `ssd search docs/query/valid-not-filter.ssq docs/examples/minimal.ssd`
  - unary `not` は leaf term に対して使えてよい
  - 期待出力は [docs/examples/golden/search-valid-not-filter.stdout](docs/examples/golden/search-valid-not-filter.stdout) と一致する
- `ssd search docs/query/valid-not-group-filter.ssq docs/examples/minimal.ssd`
  - unary `not` は grouped expression 全体に対して使えてよい
  - 期待出力は [docs/examples/golden/search-valid-not-group-filter.stdout](docs/examples/golden/search-valid-not-group-filter.stdout) と一致する
- `ssd search docs/query/valid-double-not-filter.ssq docs/examples/minimal.ssd`
  - repeated unary `not` も許可してよい
  - 期待出力は [docs/examples/golden/search-valid-double-not-filter.stdout](docs/examples/golden/search-valid-double-not-filter.stdout) と一致する
- `ssd search docs/query/valid-not-precedence-filter.ssq docs/examples/minimal.ssd`
  - unary `not` は `and` より高い precedence で評価してよい
  - 期待出力は [docs/examples/golden/search-valid-not-precedence-filter.stdout](docs/examples/golden/search-valid-not-precedence-filter.stdout) と一致する
- `ssd search docs/query/valid-quoted-not-filter.ssq docs/examples/minimal.ssd`
  - quoted string 内の `not` は unary keyword として解釈しない
  - 期待出力は [docs/examples/golden/search-valid-quoted-not-filter.stdout](docs/examples/golden/search-valid-quoted-not-filter.stdout) と一致する
- `ssd search docs/query/valid-not-range-filter.ssq docs/examples/minimal.ssd`
  - unary `not` は numeric range filter term に対しても使えてよい
  - 期待出力は [docs/examples/golden/search-valid-not-range-filter.stdout](docs/examples/golden/search-valid-not-range-filter.stdout) と一致する
- `ssd search docs/query/valid-range-nonnumeric-no-match.ssq docs/examples/minimal.ssd`
  - 非数値 field に対する range filter は failure ではなく no-match に劣化する
  - 期待出力は [docs/examples/golden/search-valid-range-nonnumeric-no-match.stdout](docs/examples/golden/search-valid-range-nonnumeric-no-match.stdout) と一致する
- `ssd search docs/query/valid-logical-and-wide-spacing.ssq docs/examples/minimal.ssd`
  - grammar で許される複数スペース区切りの `and` chain も成功する
  - 期待出力は [docs/examples/golden/search-valid-logical-and-wide-spacing.stdout](docs/examples/golden/search-valid-logical-and-wide-spacing.stdout) と一致する
- `ssd search docs/query/valid-range-less-equal-filter.ssq docs/examples/minimal.ssd`
  - `<=` range filter も numeric sidecar field に対して成功する
  - 期待出力は [docs/examples/golden/search-valid-range-less-equal-filter.stdout](docs/examples/golden/search-valid-range-less-equal-filter.stdout) と一致する
- `ssd set path:A1.label "売上報告書" --dry-run docs/examples/minimal.ssd`
  - 本体フィールド変更予定のみを表示し、ファイルは変更しない
  - 期待出力は [docs/examples/golden/set-path-A1-label.dryrun.stdout](docs/examples/golden/set-path-A1-label.dryrun.stdout) と一致する
- `ssd set path:A1.label "売上報告書" --stdout docs/examples/minimal.ssd`
  - inline field update の `--stdout` は canonical inline `.ssd` を返してよい
  - 期待出力は [docs/examples/fixtures/set-path-A1-label.expected.ssd](docs/examples/fixtures/set-path-A1-label.expected.ssd) と一致する
- `ssd set path:A1.label "売上報告書" docs/examples/minimal.ssd`
  - `.ssd` 本体だけを書き換え、paired `.ssm` は変更しない
  - 期待出力は [docs/examples/golden/set-path-A1-label.apply.stdout](docs/examples/golden/set-path-A1-label.apply.stdout) と一致する
- `ssd set path:A1.label "売上報告書" --out docs/examples/set-inline-output.ssd docs/examples/minimal.ssd`
  - path selector の inline update も source pair を変更せず canonical inline `.ssd` を別ファイルへ書いてよい
  - 期待出力は [docs/examples/golden/set-path-A1-label.out.stdout](docs/examples/golden/set-path-A1-label.out.stdout) と一致する
- `ssd set path:A1.label "売上報告書" --out docs/examples/set-inline-output.ssd --dry-run docs/examples/minimal.ssd`
  - path selector の inline update も `--out --dry-run` で inline output path preview を返してよい
  - 期待出力は [docs/examples/golden/set-path-A1-label.out.dryrun.stdout](docs/examples/golden/set-path-A1-label.out.dryrun.stdout) と一致する
- `ssd set id:A1 label "売上報告書" --stdout docs/examples/minimal.ssd`
  - id selector の inline update も `--stdout` で canonical inline `.ssd` を返してよい
  - 期待出力は [docs/examples/fixtures/set-id-A1-label.expected.ssd](docs/examples/fixtures/set-id-A1-label.expected.ssd) と一致する
- `ssd set id:A1 label "売上報告書" --out docs/examples/set-inline-output.ssd docs/examples/minimal.ssd`
  - id selector の inline update も source pair を変更せず canonical inline `.ssd` を別ファイルへ書いてよい
  - 期待出力は [docs/examples/golden/set-id-A1-label.out.stdout](docs/examples/golden/set-id-A1-label.out.stdout) と一致する
- `ssd set id:A1 label "売上報告書" --out docs/examples/set-inline-output.ssd --dry-run docs/examples/minimal.ssd`
  - id selector の inline update も `--out --dry-run` で inline output path preview を返してよい
  - 期待出力は [docs/examples/golden/set-id-A1-label.out.dryrun.stdout](docs/examples/golden/set-id-A1-label.out.dryrun.stdout) と一致する
- `ssd set id:A1,id:ALT1A,id:A1 label "確認対象" --dry-run docs/examples/minimal.ssd`
  - explicit id list は first-seen order で dedup しつつ、all-or-nothing validation 後の inline update preview を返してよい
  - 期待出力は [docs/examples/golden/set-id-list-A1-ALT1A-label.dryrun.stdout](docs/examples/golden/set-id-list-A1-ALT1A-label.dryrun.stdout) と一致する
- `ssd set id:A1,id:ALT1A label "確認対象" docs/examples/minimal.ssd`
  - explicit id list apply は dedup 後 target 群の inline field をまとめて更新してよい
  - 期待出力は [docs/examples/golden/set-id-list-A1-ALT1A-label.apply.stdout](docs/examples/golden/set-id-list-A1-ALT1A-label.apply.stdout) と一致する
- `ssd set id:A1,id:ALT1A label "確認対象" --stdout docs/examples/minimal.ssd`
  - explicit id list の `--stdout` は canonical inline `.ssd` を返してよい
  - 期待出力は [docs/examples/fixtures/set-id-list-A1-ALT1A-label.expected.ssd](docs/examples/fixtures/set-id-list-A1-ALT1A-label.expected.ssd) と一致する
- `ssd set id:A1,id:ALT1A label "確認対象" --out docs/examples/set-id-list-output.ssd docs/examples/minimal.ssd`
  - explicit id list の `--out` は source pair を変更せず canonical inline `.ssd` を別ファイルへ書いてよい
  - 期待出力は [docs/examples/golden/set-id-list-A1-ALT1A-label.out.stdout](docs/examples/golden/set-id-list-A1-ALT1A-label.out.stdout) と一致する
- `ssd set id:A1,id:ALT1A label "確認対象" --out docs/examples/set-id-list-output.ssd --dry-run docs/examples/minimal.ssd`
  - explicit id list の `--out --dry-run` は dedup 後 target 数に対する inline output path preview を返してよい
  - 期待出力は [docs/examples/golden/set-id-list-A1-ALT1A-label.out.dryrun.stdout](docs/examples/golden/set-id-list-A1-ALT1A-label.out.dryrun.stdout) と一致する
- `ssd set type:alternative label "候補更新" --allow-multi --dry-run docs/examples/minimal.ssd`
  - `type:<kind>` set は `--allow-multi` を必須にし、ascending id order の target list preview を返してよい
  - 期待出力は [docs/examples/golden/set-type-alternative-label-allow-multi.dryrun.stdout](docs/examples/golden/set-type-alternative-label-allow-multi.dryrun.stdout) と一致する
- `ssd set type:alternative label "候補更新" --allow-multi docs/examples/minimal.ssd`
  - `type:<kind>` set apply は matched target 全件の inline field をまとめて更新してよい
  - 期待出力は [docs/examples/golden/set-type-alternative-label-allow-multi.apply.stdout](docs/examples/golden/set-type-alternative-label-allow-multi.apply.stdout) と一致する
- `ssd set type:alternative label "候補更新" --allow-multi --stdout docs/examples/minimal.ssd`
  - `type:<kind>` set の `--stdout` は canonical inline `.ssd` を返してよい
  - 期待出力は [docs/examples/fixtures/set-type-alternative-label-allow-multi.expected.ssd](docs/examples/fixtures/set-type-alternative-label-allow-multi.expected.ssd) と一致する
- `ssd set type:alternative label "候補更新" --allow-multi --out docs/examples/set-type-alternative-output.ssd docs/examples/minimal.ssd`
  - `type:<kind>` set の `--out` は source pair を変更せず canonical inline `.ssd` を別ファイルへ書いてよい
  - 期待出力は [docs/examples/golden/set-type-alternative-label-allow-multi.out.stdout](docs/examples/golden/set-type-alternative-label-allow-multi.out.stdout) と一致する
- `ssd set type:alternative label "候補更新" --allow-multi --out docs/examples/set-type-alternative-output.ssd --dry-run docs/examples/minimal.ssd`
  - `type:<kind>` set の `--out --dry-run` は matched target 全件を検証した上で inline output path preview を返してよい
  - 期待出力は [docs/examples/golden/set-type-alternative-label-allow-multi.out.dryrun.stdout](docs/examples/golden/set-type-alternative-label-allow-multi.out.dryrun.stdout) と一致する
- `ssd set meta:A1.confidence 0.91 --dry-run docs/examples/minimal.ssd`
  - .ssm 側の変更予定のみを表示し、.ssd 本体の構造は変えない
  - 期待出力は [docs/examples/golden/set-meta-A1-confidence.dryrun.stdout](docs/examples/golden/set-meta-A1-confidence.dryrun.stdout) と一致する
- `ssd set meta:A1.confidence 0.91 --stdout docs/examples/minimal.ssd`
  - meta update の `--stdout` は canonical sidecar `.ssm` を返してよい
  - 期待出力は [docs/examples/fixtures/set-meta-A1-confidence.expected.ssm](docs/examples/fixtures/set-meta-A1-confidence.expected.ssm) と一致する
- `ssd set meta:A1.confidence 0.91 docs/examples/minimal.ssd`
  - `.ssm` 側だけを書き換え、`.ssd` 本体の構造は変えない
  - 期待出力は [docs/examples/golden/set-meta-A1-confidence.apply.stdout](docs/examples/golden/set-meta-A1-confidence.apply.stdout) と一致する
- `ssd set meta:A1.confidence 0.91 --out docs/examples/set-meta-output.ssm docs/examples/minimal.ssd`
  - sidecar update の `--out` は source pair を変更せず canonical sidecar `.ssm` を別ファイルへ書いてよい
  - 期待出力は [docs/examples/golden/set-meta-A1-confidence.out.stdout](docs/examples/golden/set-meta-A1-confidence.out.stdout) と一致する
- `ssd set meta:A1.confidence 0.91 --out docs/examples/set-meta-output.ssm --dry-run docs/examples/minimal.ssd`
  - sidecar update の `--out --dry-run` は sidecar output path preview を返してよい
  - 期待出力は [docs/examples/golden/set-meta-A1-confidence.out.dryrun.stdout](docs/examples/golden/set-meta-A1-confidence.out.dryrun.stdout) と一致する
- `ssd annotate id:H1 rationale "原文に主語がないため補完" --target sidecar --dry-run docs/examples/minimal.ssd`
  - 仮説 H1 に対する注記を .ssm 側へ追加予定として表示する
  - 期待出力は [docs/examples/golden/annotate-H1-rationale.dryrun.stdout](docs/examples/golden/annotate-H1-rationale.dryrun.stdout) と一致する
- `ssd annotate id:H1 todo "追加入力で主体を確認する" --target sidecar --stdout docs/examples/minimal.ssd`
  - paired input に対する sidecar annotate の `--stdout` は canonical `.ssm` を返してよい
  - 期待出力は [docs/examples/fixtures/annotate-H1-todo.expected.ssm](docs/examples/fixtures/annotate-H1-todo.expected.ssm) と一致する
- `ssd annotate id:H1 rationale "原文に主語がないため補完" --target sidecar --out docs/examples/annotate-sidecar-output.ssm --dry-run docs/examples/minimal.ssd`
  - sidecar annotate の `--out --dry-run` は sidecar output path を target file に向けた preview を返してよい
  - 期待出力は [docs/examples/golden/annotate-H1-rationale-sidecar.out.dryrun.stdout](docs/examples/golden/annotate-H1-rationale-sidecar.out.dryrun.stdout) と一致する
- `ssd add annotation docs/examples/minimal.ssd id=H1 kind=todo text=追加入力で主体を確認する --target sidecar --stdout`
  - metadata-only add の non-destructive stdout は canonical sidecar `.ssm` を返してよい
- `ssd add provenance docs/examples/minimal.ssd id=A1 provenance_kind=reviewed confidence=0.91 rationale=監査で人手確認した --target sidecar --out docs/examples/provenance-output.ssm`
  - standalone input に対する metadata-only add の `--out` は source `.ssd` を変更せず canonical sidecar `.ssm` を別ファイルへ書いてよい
- `ssd add annotation docs/examples/minimal.ssd id=H1 kind=todo text=追加入力で主体を確認する --target sidecar --out docs/examples/annotation-output.ssm --dry-run`
  - metadata-only add の `--out --dry-run` は sidecar output path を target file に向けた preview を返してよい
- `ssd annotate id:H1 todo "追加入力で主体を確認する" --target sidecar docs/examples/minimal.ssd`
  - 仮説 H1 に対する sidecar 注記を `.ssm` へ追加する
  - 期待出力は [docs/examples/golden/annotate-H1-todo.apply.stdout](docs/examples/golden/annotate-H1-todo.apply.stdout) と一致する
- `ssd annotate id:H1 status "検証待ち" --target inline docs/examples/minimal.inline.ssd`
  - standalone `.ssd` 上の supported target では inline annotate が成功する
  - 期待出力は [docs/examples/golden/annotate-H1-status-inline.apply.stdout](docs/examples/golden/annotate-H1-status-inline.apply.stdout) と一致する
- `ssd annotate id:H1 status "検証待ち" --target inline --stdout docs/examples/minimal.inline.ssd`
  - inline annotate の `--stdout` は canonical inline `.ssd` を返してよい
  - 期待出力は [docs/examples/fixtures/annotate-H1-status-inline.expected.ssd](docs/examples/fixtures/annotate-H1-status-inline.expected.ssd) と一致する
- `ssd annotate id:H1 status "検証待ち" --target inline --out docs/examples/annotate-inline-output.ssd --dry-run docs/examples/minimal.inline.ssd`
  - inline annotate の `--out --dry-run` は inline output path を target file に向けた preview を返してよい
  - 期待出力は [docs/examples/golden/annotate-H1-status-inline.out.dryrun.stdout](docs/examples/golden/annotate-H1-status-inline.out.dryrun.stdout) と一致する
- `ssd annotate id:R1 rationale "参照元を追記する" --target auto docs/examples/minimal.ssd`
  - standalone 入力でも inline 非対応 target では auto が sidecar へフォールバックする
  - 期待出力は [docs/examples/golden/annotate-R1-rationale-auto-sidecar.apply.stdout](docs/examples/golden/annotate-R1-rationale-auto-sidecar.apply.stdout) と一致する
- `ssd annotate id:R1 rationale "参照元を追記する" --target auto --out docs/examples/annotate-auto-output.ssm docs/examples/minimal.ssd`
  - standalone input で inline 非対応 target に対する auto annotate の `--out` は source `.ssd` を変更せず canonical sidecar `.ssm` を別ファイルへ書いてよい
  - 期待出力は [docs/examples/golden/annotate-R1-rationale-auto-sidecar.out.stdout](docs/examples/golden/annotate-R1-rationale-auto-sidecar.out.stdout) と一致する
- `ssd merge docs/examples/minimal.ssd --stdout`
  - .ssd と .ssm を統合した単一表現を標準出力へ出す
  - 期待出力は [docs/examples/minimal.inline.ssd](docs/examples/minimal.inline.ssd) と一致する
- `ssd split docs/examples/minimal.inline.ssd --dry-run`
  - インラインへ戻せる項目とサイドカーへ残る項目を確認できる
  - 期待出力は [docs/examples/golden/split-minimal.dryrun.stdout](docs/examples/golden/split-minimal.dryrun.stdout) と一致する
- `ssd split docs/examples/minimal.inline.ssd`
  - 対応する `.ssd` と `.ssm` へ分離を書き込み、標準出力では書き込んだパスと移動件数を報告する
  - 期待出力は [docs/examples/golden/split-minimal.apply.stdout](docs/examples/golden/split-minimal.apply.stdout) と一致する
- `ssd remove meta:A1.embedding docs/examples/minimal.ssd`
  - sidecar metadata subtree を削除し、構造整合性を壊さない場合は成功する
  - 期待出力は [docs/examples/golden/remove-A1-embedding.apply.stdout](docs/examples/golden/remove-A1-embedding.apply.stdout) と一致する
- `ssd remove id:ALT1B docs/examples/minimal.ssd`
  - dependent を持たない単一 structural target は安全に削除できる
  - 期待出力は [docs/examples/golden/remove-ALT1B.apply.stdout](docs/examples/golden/remove-ALT1B.apply.stdout) と一致する
- `ssd remove type:alternative --allow-multi docs/examples/minimal.ssd`
  - type selector の複数一致は `--allow-multi` 明示時だけ一括削除してよい
  - 期待出力は [docs/examples/golden/remove-type-alternative-allow-multi.apply.stdout](docs/examples/golden/remove-type-alternative-allow-multi.apply.stdout) と一致する
- `ssd remove id:ALT1B --dry-run docs/examples/minimal.ssd`
  - single-target structural remove は source file を変えずに preview できる
- `ssd remove id:ALT1B --stdout docs/examples/minimal.ssd`
  - structural remove の non-destructive stdout は canonical inline `.ssd` を返してよい
- `ssd remove id:ALT1B --out docs/examples/remove-ALT1B-output.ssd docs/examples/minimal.ssd`
  - structural remove の `--out` は source pair を保持したまま output file へ inline result を書いてよい
- `ssd remove meta:A1.embedding --stdout docs/examples/minimal.ssd`
  - metadata remove の non-destructive stdout も post-remove canonical inline view を返してよい
- `ssd remove id:ALT1A,id:ALT1B docs/examples/minimal.ssd`
  - explicit id list は `type:<kind>` へ還元せずに multi-target structural remove へ使ってよい
- `ssd remove id:H1,id:ALT1B --cascade --stdout docs/examples/minimal.ssd`
  - explicit structural list の cascade stdout は resolved union closure の canonical inline result を返してよい
- `ssd remove path:ALT1A.label,id:ALT1B --out docs/examples/remove-explicit-list-output.ssd docs/examples/minimal.ssd`
  - path/id mixed explicit list も structural target union として別ファイル出力してよい
- `ssd remove id:ALT1A,type:alternative --allow-multi --dry-run docs/examples/minimal.ssd`
  - explicit list に含まれる `type:<kind>` atom は `--allow-multi` と組み合わせて preview してよい
- `ssd remove type:hypothesis --allow-multi --cascade docs/examples/remove-multi-cascade-source.ssd`
  - matched hypotheses とその dependent alternatives を union closure ごと削除してよい
  - 期待出力は [docs/examples/golden/remove-type-hypothesis-allow-multi-cascade.apply.stdout](docs/examples/golden/remove-type-hypothesis-allow-multi-cascade.apply.stdout) と一致する
- `ssd remove type:hypothesis --allow-multi --cascade --out docs/examples/remove-multi-cascade-output.ssd --dry-run docs/examples/remove-multi-cascade-source.ssd`
  - multi-target cascade remove も output path preview を返してよい
- `ssd remove id:H1 --cascade docs/examples/minimal.ssd`
  - 仮説 H1 とその dependent alternatives を closure ごと削除する
  - 期待出力は [docs/examples/golden/remove-H1-cascade.apply.stdout](docs/examples/golden/remove-H1-cascade.apply.stdout) と一致する
- `ssd remove id:A1 --cascade docs/examples/minimal.ssd`
  - assertion 起点の cascade は hypothesis と alternatives まで transitive に削除する
  - 期待出力は [docs/examples/golden/remove-A1-cascade.apply.stdout](docs/examples/golden/remove-A1-cascade.apply.stdout) と一致する

これらの例は説明用サンプルではなく、仕様の期待動作を固定するための最小受け入れケースとして扱う。

失敗系ケースも同様に固定することを推奨する。

- `ssd remove type:alternative docs/examples/minimal.ssd`
  - 複数対象に一致するため既定で失敗する
  - 期待 stderr は [docs/examples/golden/remove-type-alternative.error.stderr](docs/examples/golden/remove-type-alternative.error.stderr) と一致する
- `ssd add annotation docs/examples/minimal.ssd id=H1 kind=todo text=bad --target sidecar --stdout --dry-run`
  - metadata-only add の `--stdout` と `--dry-run` 併用は失敗する
- `ssd add annotation docs/examples/minimal.ssd id=H1 kind=todo text=bad --target sidecar --out docs/examples/minimal.ssm`
  - metadata-only add の `--out` が source sibling `.ssm` を alias する場合は失敗する
- `ssd set path:A1.label bad --stdout --dry-run docs/examples/minimal.ssd`
  - set の `--stdout` と `--dry-run` 併用は失敗する
- `ssd set meta:A1.confidence 0.91 --stdout --out docs/examples/set-meta-output.ssm docs/examples/minimal.ssd`
  - set の `--stdout` と `--out` 併用は失敗する
- `ssd set path:A1.label bad --dry-run --out docs/examples/set-inline-output.ssd docs/examples/minimal.ssd`
  - set output option の未規定順序は失敗する
- `ssd set id:A1,path:ALT1A.label label bad docs/examples/minimal.ssd`
  - explicit id list slice は `id:` atom のみを受け付け、mixed selector list は失敗する
  - 期待 stderr は [docs/examples/golden/set-id-list-invalid-selector.error.stderr](docs/examples/golden/set-id-list-invalid-selector.error.stderr) と一致する
- `ssd set id:A1,id:Z9 label bad docs/examples/minimal.ssd`
  - explicit id list は 1 件でも missing target があれば全体失敗する
  - 期待 stderr は [docs/examples/golden/set-id-list-missing-target.error.stderr](docs/examples/golden/set-id-list-missing-target.error.stderr) と一致する
- `ssd set id:A1,id:H1 confidence 0.91 docs/examples/minimal.ssd`
  - explicit id list は current id-set ownership を維持し、metadata-owned field は wrong-layer として全体失敗する
  - 期待 stderr は [docs/examples/golden/set-id-list-wrong-layer.error.stderr](docs/examples/golden/set-id-list-wrong-layer.error.stderr) と一致する
- `ssd set type:assertion label bad docs/examples/minimal.ssd`
  - `type:<kind>` set slice は matched target 数に関わらず `--allow-multi` を必須にする
  - 期待 stderr は [docs/examples/golden/set-type-assertion-missing-allow-multi.error.stderr](docs/examples/golden/set-type-assertion-missing-allow-multi.error.stderr) と一致する
- `ssd set type:alternative label bad --dry-run --allow-multi docs/examples/minimal.ssd`
  - `type:<kind>` set surface では `--allow-multi` は output options より前に固定され、未規定順序は失敗する
  - 期待 stderr は [docs/examples/golden/set-type-alternative-order-invalid.error.stderr](docs/examples/golden/set-type-alternative-order-invalid.error.stderr) と一致する
- `ssd set type:alternative label bad --stdout --allow-multi docs/examples/minimal.ssd`
  - `type:<kind>` set surface では `--stdout` の前に `--allow-multi` を置かなければならず、reorder は失敗する
  - 期待 stderr は [docs/examples/golden/set-type-alternative-stdout-before-allow-multi.error.stderr](docs/examples/golden/set-type-alternative-stdout-before-allow-multi.error.stderr) と一致する
- `ssd set type:alternative label bad --out docs/examples/set-type-alternative-output.ssd --allow-multi docs/examples/minimal.ssd`
  - `type:<kind>` set surface では `--out` の前に `--allow-multi` を置かなければならず、reorder は失敗する
  - 期待 stderr は [docs/examples/golden/set-type-alternative-out-before-allow-multi.error.stderr](docs/examples/golden/set-type-alternative-out-before-allow-multi.error.stderr) と一致する
- `ssd set type:alternative source_segment S1 --allow-multi docs/examples/minimal.ssd`
  - `type:<kind>` set は current set taxonomy 上 unresolved な field が 1 件でもあれば missing-target failure として全体失敗する
  - 期待 stderr は [docs/examples/golden/set-type-alternative-missing-target.error.stderr](docs/examples/golden/set-type-alternative-missing-target.error.stderr) と一致する
- `ssd set type:assertion confidence 0.92 --allow-multi docs/examples/minimal.ssd`
  - `type:<kind>` set は current set ownership を維持し、metadata-owned field は wrong-layer として全体失敗する
  - 期待 stderr は [docs/examples/golden/set-type-assertion-wrong-layer-allow-multi.error.stderr](docs/examples/golden/set-type-assertion-wrong-layer-allow-multi.error.stderr) と一致する
- `ssd set path:A1.label bad --out docs/examples/minimal.ssd docs/examples/minimal.ssd`
  - inline set の `--out` が source `.ssd` を alias する場合は失敗する
- `ssd set meta:A1.confidence 0.91 --out docs/examples/minimal.ssm docs/examples/minimal.ssd`
  - paired input の meta set で `--out` が source sibling `.ssm` を alias する場合は失敗する
- `ssd set meta:A1.confidence 0.91 --out docs/examples/minimal.inline.ssm docs/examples/minimal.inline.ssd`
  - standalone input の meta set でも reserved sibling `.ssm` への alias は失敗する
- `ssd annotate id:H1 todo bad --target sidecar --stdout --dry-run docs/examples/minimal.ssd`
  - annotate の `--stdout` と `--dry-run` 併用は失敗する
- `ssd annotate id:H1 todo bad --target sidecar --stdout --out docs/examples/annotate-output.ssm docs/examples/minimal.ssd`
  - annotate の `--stdout` と `--out` 併用は失敗する
- `ssd annotate id:H1 status bad --target inline --out docs/examples/minimal.inline.ssd docs/examples/minimal.inline.ssd`
  - inline annotate の `--out` が source `.ssd` を alias する場合は失敗する
- `ssd annotate id:H1 todo bad --target sidecar --out docs/examples/minimal.ssm docs/examples/minimal.ssd`
  - paired input の sidecar annotate で `--out` が source sibling `.ssm` を alias する場合は失敗する
- `ssd annotate id:R1 rationale bad --target auto --out docs/examples/minimal.ssm docs/examples/minimal.ssd`
  - standalone input で auto が sidecar へ解決される場合も、`--out` は source sibling `.ssm` を alias してはならない
- `ssd annotate id:A1,id:H1,id:A1 todo 一括確認する --target sidecar --dry-run docs/examples/minimal.ssd`
  - explicit id-list annotate は first-seen order で dedup し、paired input の sidecar preview を返す
  - 期待 stdout は [docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.dryrun.stdout](docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.dryrun.stdout) と一致する
- `ssd annotate id:A1,id:H1 todo 一括確認する --target sidecar docs/examples/minimal.ssd`
  - explicit id-list sidecar annotate apply は両 id の metadata を一括更新し、source pair では `.ssm` だけを書き換える
  - 期待 stdout は [docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.apply.stdout](docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.apply.stdout) と一致する
- `ssd annotate id:A1,id:H1 todo 一括確認する --target sidecar --out docs/examples/annotate-id-list-output.ssm docs/examples/minimal.ssd`
  - explicit id-list sidecar annotate out は source pair を保持したまま canonical sidecar `.ssm` を別ファイルへ書く
  - 期待 stdout は [docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.out.stdout](docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.out.stdout) と一致する
- `ssd annotate id:A1,id:H1 todo 一括確認する --target sidecar --out docs/examples/annotate-id-list-output.ssm --dry-run docs/examples/minimal.ssd`
  - explicit id-list sidecar annotate out-dry-run は target file を output path に向けた preview として扱う
  - 期待 stdout は [docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.out.dryrun.stdout](docs/examples/golden/annotate-id-list-A1-H1-todo-sidecar.out.dryrun.stdout) と一致する
- `ssd annotate id:A1,id:H1 status 検証待ち --target inline --stdout docs/examples/minimal.inline.ssd`
  - explicit id-list inline annotate stdout は standalone inline document を変更せず canonical inline `.ssd` を返す
  - 期待 stdout は [docs/examples/golden/annotate-id-list-A1-H1-status-inline.stdout](docs/examples/golden/annotate-id-list-A1-H1-status-inline.stdout) と一致する
- `ssd annotate type:alternative rationale 候補を再確認する --target sidecar --allow-multi --dry-run docs/examples/minimal.ssd`
  - type allow-multi sidecar annotate は matched id の ascending order を current sidecar preview surface に載せて扱う
  - 期待 stdout は [docs/examples/golden/annotate-type-alternative-rationale-sidecar.dryrun.stdout](docs/examples/golden/annotate-type-alternative-rationale-sidecar.dryrun.stdout) と一致する
- `ssd annotate type:alternative rationale 候補を再確認する --target sidecar --allow-multi docs/examples/minimal.ssd`
  - type allow-multi sidecar annotate apply は matched kind set の metadata を一括更新し、source pair では `.ssm` だけを書き換える
  - 期待 stdout は [docs/examples/golden/annotate-type-alternative-rationale-sidecar.apply.stdout](docs/examples/golden/annotate-type-alternative-rationale-sidecar.apply.stdout) と一致する
- `ssd annotate type:alternative rationale 候補を再確認する --target sidecar --allow-multi --stdout docs/examples/minimal.ssd`
  - type allow-multi sidecar annotate stdout は source pair を変更せず canonical sidecar `.ssm` を返す
  - 期待 stdout は [docs/examples/golden/annotate-type-alternative-rationale-sidecar.stdout](docs/examples/golden/annotate-type-alternative-rationale-sidecar.stdout) と一致する
- `ssd annotate type:alternative rationale 候補を再確認する --target sidecar --allow-multi --out docs/examples/annotate-type-output.ssm docs/examples/minimal.ssd`
  - type allow-multi sidecar annotate out は source pair を保持したまま canonical sidecar `.ssm` を別ファイルへ書く
  - 期待 stdout は [docs/examples/golden/annotate-type-alternative-rationale-sidecar.out.stdout](docs/examples/golden/annotate-type-alternative-rationale-sidecar.out.stdout) と一致する
- `ssd annotate type:alternative rationale 候補を再確認する --target sidecar --allow-multi --out docs/examples/annotate-type-output.ssm --dry-run docs/examples/minimal.ssd`
  - type allow-multi sidecar annotate out-dry-run は target file を output path に向けた preview として扱う
  - 期待 stdout は [docs/examples/golden/annotate-type-alternative-rationale-sidecar.out.dryrun.stdout](docs/examples/golden/annotate-type-alternative-rationale-sidecar.out.dryrun.stdout) と一致する
- `ssd annotate id:A1,path:H1.summary todo bad --target sidecar docs/examples/minimal.ssd`
  - annotate id-list slice は comma-separated `id:` atoms だけを許可し、mixed selector list は failure とする
  - 期待 stderr は [docs/examples/golden/annotate-id-list-invalid-selector.error.stderr](docs/examples/golden/annotate-id-list-invalid-selector.error.stderr) と一致する
- `ssd annotate id:A1,id:H1 todo bad --target auto docs/examples/minimal.ssd`
  - annotate id-list slice では `--target auto` を許可しない
  - 期待 stderr は [docs/examples/golden/annotate-id-list-auto-target-unsupported.error.stderr](docs/examples/golden/annotate-id-list-auto-target-unsupported.error.stderr) と一致する
- `ssd annotate id:A1,id:Z9 todo bad --target sidecar docs/examples/minimal.ssd`
  - annotate id-list は 1 件でも missing target があれば全体 failure とする
  - 期待 stderr は [docs/examples/golden/annotate-id-list-missing-target.error.stderr](docs/examples/golden/annotate-id-list-missing-target.error.stderr) と一致する
- `ssd annotate id:A1,id:R1 status bad --target inline docs/examples/minimal.inline.ssd`
  - annotate id-list inline は 1 件でも inline unsupported target を含めば全体 failure とする
  - 期待 stderr は [docs/examples/golden/annotate-id-list-inline-unsupported.error.stderr](docs/examples/golden/annotate-id-list-inline-unsupported.error.stderr) と一致する
- `ssd annotate type:alternative rationale bad --target sidecar docs/examples/minimal.ssd`
  - type allow-multi annotate は matched target 数に関わらず `--allow-multi` を必須とする
  - 期待 stderr は [docs/examples/golden/annotate-type-alternative-missing-allow-multi.error.stderr](docs/examples/golden/annotate-type-alternative-missing-allow-multi.error.stderr) と一致する
- `ssd annotate type:alternative rationale bad --target inline --allow-multi docs/examples/minimal.ssd`
  - type allow-multi annotate slice は `--target sidecar` のみに固定し、inline は failure とする
  - 期待 stderr は [docs/examples/golden/annotate-type-alternative-inline-target.error.stderr](docs/examples/golden/annotate-type-alternative-inline-target.error.stderr) と一致する
- `ssd annotate type:alternative rationale bad --target auto --allow-multi docs/examples/minimal.ssd`
  - type allow-multi annotate slice は `--target auto` を許可しない
  - 期待 stderr は [docs/examples/golden/annotate-type-alternative-auto-target.error.stderr](docs/examples/golden/annotate-type-alternative-auto-target.error.stderr) と一致する
- `ssd annotate type:alternative rationale bad --target sidecar --dry-run --allow-multi docs/examples/minimal.ssd`
  - type allow-multi annotate surface では `--allow-multi` は output options より前に固定され、reorder は failure とする
  - 期待 stderr は [docs/examples/golden/annotate-type-alternative-order-invalid.error.stderr](docs/examples/golden/annotate-type-alternative-order-invalid.error.stderr) と一致する
- `ssd annotate type:alternative rationale bad --target sidecar --allow-multi --dry-run --out docs/examples/annotate-type-output.ssm docs/examples/minimal.ssd`
  - type allow-multi annotate surface では `--dry-run` 後に `--out` を続ける順序を許可しない
  - 期待 stderr は [docs/examples/golden/annotate-type-alternative-dryrun-out-invalid.error.stderr](docs/examples/golden/annotate-type-alternative-dryrun-out-invalid.error.stderr) と一致する
- `ssd annotate type:claim rationale bad --target sidecar --allow-multi docs/examples/minimal.ssd`
  - type allow-multi annotate は matched set が空なら missing-target failure とする
  - 期待 stderr は [docs/examples/golden/annotate-type-claim-missing-target.error.stderr](docs/examples/golden/annotate-type-claim-missing-target.error.stderr) と一致する
- `ssd annotate type:alternative rationale bad --target sidecar --allow-multi --out docs/examples/minimal.ssm docs/examples/minimal.ssd`
  - type allow-multi sidecar annotate の `--out` が source sibling `.ssm` を alias する場合は failure とする
  - 期待 stderr は [docs/examples/golden/annotate-type-alternative-out-alias-sidecar.error.stderr](docs/examples/golden/annotate-type-alternative-out-alias-sidecar.error.stderr) と一致する
- `ssd remove id:A1 docs/examples/minimal.ssd`
  - 参照されている構造要素のため既定で失敗する
  - 期待 stderr は [docs/examples/golden/remove-id-A1.error.stderr](docs/examples/golden/remove-id-A1.error.stderr) と一致する
- `ssd remove id:H1 docs/examples/minimal.ssd`
  - dependent alternative が残るため `--cascade` なしでは失敗する
  - 期待 stderr は [docs/examples/golden/remove-H1-references.error.stderr](docs/examples/golden/remove-H1-references.error.stderr) と一致する
- `ssd remove id:ALT1B --stdout --dry-run docs/examples/minimal.ssd`
  - `--stdout` と `--dry-run` の併用は失敗する
- `ssd remove id:ALT1B --out docs/examples/minimal.ssd docs/examples/minimal.ssd`
  - `--out` が source `.ssd` を alias する場合は失敗する
- `ssd remove id:ALT1B --out docs/examples/minimal.ssm docs/examples/minimal.ssd`
  - paired input の `--out` が source `.ssm` を alias する場合も失敗する
- `ssd remove meta:A1.embedding,meta:H1.rationale docs/examples/minimal.ssd`
  - meta selector list はこの slice では失敗する
- `ssd remove doc:self,id:ALT1B docs/examples/minimal.ssd`
  - `doc:self` selector list はこの slice では失敗する
- `ssd remove id:ALT1A,type:alternative docs/examples/minimal.ssd`
  - explicit list 内の `type:<kind>` atom が複数 target を持つ場合、`--allow-multi` なしでは失敗する
- `ssd remove id:H1,id:ALT1B --stdout --cascade docs/examples/minimal.ssd`
  - output-before-safety reorder は explicit list でも失敗する
- `ssd remove id:R1 docs/examples/minimal.ssd`
  - resource に対する source edge が残るため `--cascade` なしでは失敗する
  - 期待 stderr は [docs/examples/golden/remove-R1-references.error.stderr](docs/examples/golden/remove-R1-references.error.stderr) と一致する
- `ssd remove type:hypothesis --allow-multi docs/examples/remove-multi-cascade-source.ssd`
  - dependent alternatives が removal set の外に残るため `--cascade` なしでは失敗する
  - 期待 stderr は [docs/examples/golden/remove-type-hypothesis-allow-multi-without-cascade.error.stderr](docs/examples/golden/remove-type-hypothesis-allow-multi-without-cascade.error.stderr) と一致する
- `ssd remove type:hypothesis --cascade --allow-multi docs/examples/remove-multi-cascade-source.ssd`
  - 現行 slice では multi-target cascade の option 順序は明示的で、reordering は失敗する
  - 期待 stderr は [docs/examples/golden/remove-type-hypothesis-cascade-before-allow-multi.error.stderr](docs/examples/golden/remove-type-hypothesis-cascade-before-allow-multi.error.stderr) と一致する
- `ssd remove id:ALT1B --allow-multi docs/examples/minimal.ssd`
  - `--allow-multi` は `type:<kind>` selector 以外では失敗する
  - 期待 stderr は [docs/examples/golden/remove-id-allow-multi-invalid.error.stderr](docs/examples/golden/remove-id-allow-multi-invalid.error.stderr) と一致する
- `ssd annotate id:H1 note "補足" docs/examples/minimal.ssd`
  - 未定義 annotation kind のため失敗する
  - 期待 stderr は [docs/examples/golden/annotate-invalid-kind.error.stderr](docs/examples/golden/annotate-invalid-kind.error.stderr) と一致する
- `ssd annotate id:R1 rationale "bad" --target inline docs/examples/minimal.inline.ssd`
  - inline 非対応 kind のため失敗する
  - 期待 stderr は [docs/examples/golden/annotate-inline-unsupported-resource.error.stderr](docs/examples/golden/annotate-inline-unsupported-resource.error.stderr) と一致する
- `ssd annotate id:H1 todo "追加入力で主体を確認する" --target inline docs/examples/minimal.ssd`
  - paired input に対する inline annotate は許可しない
  - 期待 stderr は [docs/examples/golden/annotate-inline-requires-standalone.error.stderr](docs/examples/golden/annotate-inline-requires-standalone.error.stderr) と一致する
- `ssd set path:A1 0.91 docs/examples/minimal.ssd`
  - `path:` selector に dotted field path がないため字句または構文段階で失敗する
  - 期待 stderr は [docs/examples/golden/set-malformed-selector.error.stderr](docs/examples/golden/set-malformed-selector.error.stderr) と一致する
- `ssd check --strict docs/examples/minimal.ssd`
  - 未定義オプションのため option parsing 段階で失敗する
  - 期待 stderr は [docs/examples/golden/check-unknown-option.error.stderr](docs/examples/golden/check-unknown-option.error.stderr) と一致する
- `ssd check docs/examples/minimal.ssd --strict`
  - trailing にある未定義オプションも無視せず option parsing 段階で失敗する
  - 期待 stderr は [docs/examples/golden/check-trailing-unknown-option.error.stderr](docs/examples/golden/check-trailing-unknown-option.error.stderr) と一致する
- `ssd annotate id:H1 rationale "未完 docs/examples/minimal.ssd`
  - quoted string が閉じていないため字句段階で失敗する
  - 期待 stderr は [docs/examples/golden/annotate-unterminated-quoted-string.error.stderr](docs/examples/golden/annotate-unterminated-quoted-string.error.stderr) と一致する
- `ssd search docs/query/invalid-range-quoted-number-filter.ssq docs/examples/minimal.ssd`
  - range filter の rhs が quoted number の場合は失敗する
  - 期待 stderr は [docs/examples/golden/search-invalid-range-quoted-number-filter.error.stderr](docs/examples/golden/search-invalid-range-quoted-number-filter.error.stderr) と一致する
- `ssd search docs/query/invalid-unmatched-parenthesis-filter.ssq docs/examples/minimal.ssd`
  - closing されない grouping は失敗する
  - 期待 stderr は [docs/examples/golden/search-invalid-unmatched-parenthesis-filter.error.stderr](docs/examples/golden/search-invalid-unmatched-parenthesis-filter.error.stderr) と一致する
- `ssd search docs/query/invalid-empty-group-filter.ssq docs/examples/minimal.ssd`
  - empty grouping は失敗する
  - 期待 stderr は [docs/examples/golden/search-invalid-empty-group-filter.error.stderr](docs/examples/golden/search-invalid-empty-group-filter.error.stderr) と一致する
- `ssd search docs/query/invalid-group-adjacency-filter.ssq docs/examples/minimal.ssd`
  - term と group の間に operator がない式は失敗する
  - 期待 stderr は [docs/examples/golden/search-invalid-group-adjacency-filter.error.stderr](docs/examples/golden/search-invalid-group-adjacency-filter.error.stderr) と一致する
- `ssd search docs/query/invalid-dangling-not-filter.ssq docs/examples/minimal.ssd`
  - operand のない unary `not` は validation failure とする
  - 期待 stderr は [docs/examples/golden/search-invalid-dangling-not-filter.error.stderr](docs/examples/golden/search-invalid-dangling-not-filter.error.stderr) と一致する
- `ssd search docs/query/invalid-not-right-paren-filter.ssq docs/examples/minimal.ssd`
  - `not )` のように closing parenthesis が operand 位置へ現れる式は validation failure とする
  - 期待 stderr は [docs/examples/golden/search-invalid-not-right-paren-filter.error.stderr](docs/examples/golden/search-invalid-not-right-paren-filter.error.stderr) と一致する
- `ssd search docs/query/invalid-not-field-filter.ssq docs/examples/minimal.ssd`
  - current `where` profile では reserved keyword `not` を field name として使えない
  - 期待 stderr は [docs/examples/golden/search-invalid-not-field-filter.error.stderr](docs/examples/golden/search-invalid-not-field-filter.error.stderr) と一致する

成功系と失敗系の両方は、runner から収集可能な manifest 形式で保持する。

## 8.11 test runner 入力形式

CLI の受け入れ例と golden test は、runner が機械的に収集できる manifest 形式で保持することを推奨する。

初期形式は [docs/test-runner-format.md](docs/test-runner-format.md) で定義する。

初期 manifest は次の 2 つを持つ。

- [docs/examples/testcases/cli-success.json](docs/examples/testcases/cli-success.json)
- [docs/examples/testcases/cli-failure.json](docs/examples/testcases/cli-failure.json)

各 case は少なくとも以下を持つこと。

- id
- command
- argv
- cwd
- stdin
- environment
- expected_exit
- expected_stdout
- expected_stderr
- expected_output_kind
- notes

write case では、次の optional key を追加してよい。

- setup_files
- expected_files

`command` は人間可読の表示用、`argv` は runner の正規入力として扱うことを推奨する。

成功系と失敗系は同一構造で表し、expected_exit と stdout / stderr golden の組み合わせで区別する。

parser / lexer 系の失敗ケースでは、shell 差異を避けるため `argv` を正とし、必要なら `command` に人間向けの表記を残す。

## 8.12 selector とサンプルの対応

[docs/examples/minimal.ssd](docs/examples/minimal.ssd) と [docs/examples/minimal.ssm](docs/examples/minimal.ssm) には、少なくとも以下の selector 対応が存在することを推奨する。

- `id:D1`
  - 文書本体リソース
- `id:A1`
  - 主張
- `id:H1`
  - 仮説
- `path:A1.label`
  - .ssd 本体にある構造属性
- `meta:A1.confidence`
  - .ssm にある補助メタ情報
- `meta:A1.embedding.model`
  - .ssm にある埋め込みメタデータ
- `doc:self`
  - 文書自体

## 8.13 grammar artifact の優先順位

初期仕様で grammar の主対象として優先するのは、CLI argv ではなく SEMDL と query language である。

- `.ssd` の grammar
- `.ssq` の grammar
- 将来導入する場合の semql family grammar

これらの primary artifact は、初期仕様では少なくとも以下に対応すること。

- [docs/ssd.ebnf](docs/ssd.ebnf)
- [docs/ssq.ebnf](docs/ssq.ebnf)

CLI argv 仕様は secondary operational artifact として扱う。

`docs/cli.ebnf` は、初期 CLI 引数仕様に整合する補助的な formal notation artifact として残してよい。

この artifact は少なくとも以下を記述してよい。

- サブコマンド一覧
- add / set / remove / annotate の引数順序
- selector の構文
- 共通オプションの構文
- quoted string、number、boolean、identifier の字句境界
- `--allow-multi` のような更新系安全オプション

この artifact に対応する初期 parser / lexer failure cases も golden として維持してよい。

CLI の breaking change を行う場合は、受け入れ例だけでなく [docs/cli.ebnf](docs/cli.ebnf) も同一変更で更新してよい。

## 8.14 test runner 実行契約

manifest 形式だけでなく、runner がどう実行し、どう比較するかの契約も固定することを推奨する。

初期契約は [docs/test-runner-contract.md](docs/test-runner-contract.md) に置く。

この契約は少なくとも以下を固定する。

- manifest の探索単位
- `argv` を正規入力として使うこと
- `stdin` と `environment` の適用順序
- stdout / stderr / exit code の比較順序
- sandboxed write case で file output を比較すること
- `exact`、`empty`、`fixture-file` の比較意味
- shell を介さず直接プロセス起動すること

runner の公開契約を変更する場合は、architecture 変更として ADR 対象に含める。

## 8.15 selector 境界 failure cases

selector の構文だけでなく、解決境界の failure cases も最小集合として維持することを推奨する。

- `ssd set id:Z9 label "未知" docs/examples/minimal.ssd`
  - 存在しない ID のため失敗する
  - 期待 stderr は [docs/examples/golden/set-id-missing-target.error.stderr](docs/examples/golden/set-id-missing-target.error.stderr) と一致する
- `ssd set path:H1.caveat "注意" docs/examples/minimal.ssd`
  - `.ssd` 本体に存在しない field のため失敗する
  - 期待 stderr は [docs/examples/golden/set-path-wrong-layer.error.stderr](docs/examples/golden/set-path-wrong-layer.error.stderr) と一致する
- `ssd set id:H1 caveat "注意" docs/examples/minimal.ssd`
  - `id:` selector の set は inline field update として扱い、sidecar-only field には書き込まない
  - 期待 stderr は [docs/examples/golden/set-id-wrong-layer.error.stderr](docs/examples/golden/set-id-wrong-layer.error.stderr) と一致する
- `ssd set meta:A1.label "別名" docs/examples/minimal.ssd`
  - sidecar ではなく `.ssd` 本体に属する field のため失敗する
  - 期待 stderr は [docs/examples/golden/set-meta-wrong-layer.error.stderr](docs/examples/golden/set-meta-wrong-layer.error.stderr) と一致する

これらは selector 構文そのものではなく、selector が指す層と対象解決の境界を固定する failure cases である。

## 8.16 CLI help の情報設計

SEMDL の help 正本は CLI とし、`ssd help` と `ssd --help` は同一の root help を表示することを推奨する。

root help は少なくとも以下をこの順で持つこと。

1. 概要
2. 目次
3. 文法
4. リファレンス
5. 逆引きリファレンス
6. サンプル
7. 注意点、既知バグ、報告先

root help は全文を 1 面で表示してよいが、topic 別の導線も持つことを推奨する。

- `ssd help overview`
- `ssd help toc`
- `ssd help grammar`
- `ssd help reference`
- `ssd help recipes`
- `ssd help samples`
- `ssd help troubleshooting`

reference と recipes は追加の絞り込みを許容してよい。

それ以外の help topic は追加 target を受け付けない。

例:

- `ssd help reference check`
- `ssd help reference set`
- `ssd help recipes wrong-layer`

help は単なる usage 断片ではなく、少なくとも self-contained な operational syntax summary を持ち、必要な操作情報を `ssd help` だけで取得できること。

この要件は、外部 artifact 参照を消すだけでは満たされない。
利用者は root help、topic help、subcommand help を辿ることで、少なくとも以下を repository 文書なしで取得できること。

- help の入口形式と、target を受け付ける topic の境界
- selector 形式と layer ごとの使い分け
- scalar 値と text 値の受理形
- 共通 option とその使いどころ
- 同名 option の役割差。少なくとも help render 用の `--format semdl` と、command content/profile 用の `--format inline|sidecar` を区別して説明すること
- command-specific usage を辿る導線と、その先の具体的 usage 形

これは CLI の運用 guidance に関する要件であり、SEMDL や query language 自体の formal grammar を CLI help に背負わせることは意味しない。

`troubleshooting` topic は repository 文書参照ではなく、他の help topic / subcommand help / recipes topic への導線で閉じること。

[docs/cli.ebnf](docs/cli.ebnf) は repository 上の formal notation artifact として残してよいが、CLI help は利用者をそのファイル参照へ導いてはならない。

help は既定で人間向け text を返しつつ、opt-in の machine-readable surface として `--format semdl` を許容してよい。

- `ssd --help --format semdl`
- `ssd help grammar --format semdl`
- `ssd check --help --format semdl`

初期 slice では、`--format semdl` は CLI help 出力契約として固定すればよく、core parser がそのまま再読込できることまでは要求しない。
ただし出力は line-preserving な SEMDL profile とし、`document`、`resource`、`segment` block を使って help の文面順序を保てること。

## 8.17 help への導線

CLI は、少なくとも以下の入口から help へ辿れることを推奨する。

- `ssd --help`
- `ssd help`
- `ssd <subcommand> --help`
- unknown option error
- missing required argument error
- unknown help topic error
- unimplemented subcommand error
- explain target not found error

error message は root help 全文を再掲する必要はないが、少なくとも適切な topic か subcommand help への案内を含むこと。

初期受け入れ例には少なくとも以下を含めることを推奨する。

- `ssd help`
- `ssd help grammar`
- `ssd help reference check`
- `ssd help recipes wrong-layer`
- `ssd help --format semdl`
- `ssd --help --format semdl`
- `ssd help grammar --format semdl`
- `ssd help reference check --format semdl`
- `ssd help recipes explain-not-found`
- `ssd check --help`
- `ssd check --help --format semdl`
- `ssd search --help`
- `ssd extract --help`
- `ssd similarity --help`
- `ssd add --help`
- `ssd remove --help`
- `ssd split --help`
- `ssd merge --help`
- `ssd normalize --help`
- `ssd help unknown-topic`
- `ssd help --format json`
- `ssd help grammar extra`
- unknown option から `ssd help grammar` への導線
- explain の missing argument から `ssd help reference explain` への導線
- explain の target not found から `ssd help recipes explain-not-found` への導線
- 未実装 subcommand から `ssd help reference <subcommand>` への導線

split の初期 help と acceptance examples は、既存の受け入れ surface と揃えて `docs/examples/minimal.inline.ssd` を入力例としてよい。

check は少なくとも以下を検証できること。

- 構文妥当性
- 未定義参照
- 循環参照の扱い
- 代替案集合の整合性
- 確信度値の妥当範囲

# 9. 実装要件

- C または C++ で実装する
- 依存関係は最小限にする
- 標準入出力を扱いやすい CLI とする
- ライブラリ部と CLI 部を分離し、将来の組み込み利用に備える

## 9.0 アーキテクチャ判断の固定

SEMDL のアーキテクチャ判断は ADR で固定することを推奨する。

- 運用ルールは [docs/adr/README.md](docs/adr/README.md) に置く
- 初期 ADR は [docs/adr/0001-use-architecture-decision-records.md](docs/adr/0001-use-architecture-decision-records.md) とする
- 実装層分離の初期 ADR は [docs/adr/0002-separate-library-cli-and-runner.md](docs/adr/0002-separate-library-cli-and-runner.md) とする
- 実装骨格配置の初期 ADR は [docs/adr/0003-establish-initial-implementation-layout.md](docs/adr/0003-establish-initial-implementation-layout.md) とする
- help 構造と entrypoint の ADR は [docs/adr/0004-structure-cli-help-and-entrypoints.md](docs/adr/0004-structure-cli-help-and-entrypoints.md) とする
- help の opt-in SEMDL rendering profile の ADR は [docs/adr/0005-add-opt-in-semdl-help-rendering.md](docs/adr/0005-add-opt-in-semdl-help-rendering.md) とする

少なくとも以下は ADR 対象とする。

- format 境界の変更
- CLI 公開契約の変更
- selector 解決規則の変更
- test runner 契約の変更
- ライブラリ層と CLI 層の責務変更

## 9.2 実装レイヤの初期分離

実装フェーズへ入る前提として、少なくとも以下の 3 層を分離することを推奨する。

- core library
  - .ssd / .ssm の parse、validate、resolve、merge view を担当する
  - CLI 引数解釈や golden 比較には依存しない
- ssd CLI
  - 引数解釈、入出力、exit code、エラー整形、ファイル更新を担当する
  - 文書意味解釈そのものは core library に委譲する
- test runner
  - manifest 読み込み、argv 実行、stdout / stderr / exit code 比較を担当する
  - CLI の外側に位置し、core library API に直接依存しない

初期分離の判断理由と境界は [docs/adr/0002-separate-library-cli-and-runner.md](docs/adr/0002-separate-library-cli-and-runner.md) を正とする。

## 9.3 初期実装骨格と最初の change plan

初期実装の配置先と依存方向は [docs/adr/0003-establish-initial-implementation-layout.md](docs/adr/0003-establish-initial-implementation-layout.md) を正とする。

最初の implementation change plan は [docs/plans/initial-implementation-change-plan.md](docs/plans/initial-implementation-change-plan.md) に置くことを推奨する。

この plan は少なくとも以下を明記すること。

- どの prompt または skill を入口として想定するか
- 最初の実装 slice が core library、CLI、runner のどこまでを含むか
- requirements、EBNF、manifest、golden への波及有無
- success / failure のどの受け入れ例を最初に実装対象へ含めるか

## 9.1 埋め込み生成

埋め込み生成は以下の両方式をサポート可能な設計とする。

- ローカルモデルによる生成
- 外部埋め込みサービスによる生成

ただし初期実装では、片方のみ実装してもよい。未実装方式はインターフェースで差し替え可能であること。

# 10. 品質要件

- 仕様書と利用者向けドキュメントを提供すること
- フォーマット仕様には最小例と複合例を含めること
- パーサ、バリデータ、検索器のテストを整備すること
- 同一入力に対して再現可能な出力を優先すること
- 実装方針はテストファーストを基本とし、少なくとも parser、validator、CLI の期待動作を先に例示してから実装すること
- 仕様に追加する CLI 機能には、対応する受け入れ例または golden test 候補を同時に追加すること
- .ssd / .ssm のサンプルは、説明資料であると同時に回帰テスト資産として維持すること

# 11. 今後の拡張余地

以下は将来拡張として認めるが、初期必須要件には含めない。

- RDF / JSON-LD / PROV-O との相互変換
- completion、hover、rename、formatting、code action、semantic token のような richer editor features
- 分散インデックス
- 複数文書集合に対する増分更新
- 厳密な確率モデルやベイズ推論との連携

# 12. 参考にした記述原則

本仕様の整理にあたり、特に以下の考え方を参照した。

- W3C Web Annotation Data Model
  - 原文や原データの部分指定を Selector として扱う
  - 引用、位置、バイト範囲、絞り込みを分けて表現する
- W3C PROV-O
  - 主張や抽出結果の出所を Entity / Activity / Agent で追跡する
  - 関係そのものを修飾して追加情報を載せる
- RDF-star / qualified relation pattern
  - 主張そのものにメタデータを付与する

したがって SEMDL は、単なるタグ付きテキストではなく、
「意味構造 + 出所 + 不確実性 + セグメント指定 + 埋め込み」を一体で扱う軽量記述系として定義する。