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

- 意味的記述フォーマット .ssd
- .ssd を対象とする検索クエリ記法 .ssq
- .ssd の検証、抽出、検索、比較を行う CLI ツール ssd
- 埋め込みベクトルを併用した意味類似検索機能

SEMDL が対象外とするものは以下とする。

- 汎用の知識表現標準そのものの再実装
- 完全な自動意味解析器の内蔵
- 数学的に厳密な確率推論エンジンの汎用実装
- 生バイナリをそのまま人手編集するための重量級フォーマット

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

# 5. .ssd フォーマット要件

## 5.1 基本要件

- 拡張子は .ssd とする
- UTF-8 テキストで保存できること
- コメント記法を持つこと
- 安定した識別子参照ができること
- パーサが一意に解釈できること

## 5.2 記述プロファイル

SEMDL は、少なくとも次の 2 つの記述プロファイルを許容すること。

- インラインプロファイル
  - 意味構造とメタ情報を 1 つの .ssd にまとめて記述する方式
- サイドカープロファイル
  - 意味構造の本体を .ssd に記述し、可読性を損なうメタ情報を別ファイルへ分離する方式

サイドカープロファイルは、可読性を優先するための推奨方式として扱ってよい。
一方で、単独配布やレビュー容易性を優先する場合はインラインプロファイルも許容する。

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

ただしサイドカープロファイルでは、上記のうち一部を別ファイルへ退避できること。

## 5.4 本体とサイドカーの責務分担

.ssd 本体には、意味構造を読むために最低限必要な情報を優先して残すこと。

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

## 5.5 サイドカー形式

サイドカープロファイルでは、.ssd と同じベース名を持つメタ情報ファイルを関連付けられること。

例:

- sample.ssd
- sample.ssm

ここで .ssm は SEMDL metadata sidecar を表す拡張子とする。

.ssm は少なくとも以下を満たすこと。

- .ssd 内の識別子を参照できること
- .ssd 本体を変更せずに付加情報だけ更新できること
- 複数の主張やリソースに対するメタ情報をまとめて保持できること
- パーサが .ssd と .ssm の対応関係を一意に解決できること

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

サイドカープロファイルでは、詳細な selector、state、rendering 情報を .ssm へ分離してよい。
ただし本体には「原文対応あり」の最小表現が残ること。

## 5.8 推測の明示

原文に明示されていない補完や省略復元を記述する場合は、以下が必須であること。

- 推測であるという種別
- その推測の根拠
- 競合する他候補がある場合の列挙

サイドカープロファイルでは、詳細な根拠説明や評価メモを .ssm へ分離してよい。
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

サイドカープロファイルでは、文書全体メタデータの詳細版を .ssm に置けること。

# 6. .ssq フォーマット要件

## 6.1 基本要件

- 拡張子は .ssq とする
- .ssd を検索対象とする
- テキストとして保存・共有できること
- 初期仕様では参照系の問い合わせ専用とし、更新構文は持たないこと

## 6.2 検索機能

.ssq は少なくとも以下の検索条件を表現できること。

- ノード型による検索
- 関係型による検索
- 属性値による検索
- 原文対応の有無による検索
- 出所情報による検索
- 仮説 / 事実 / 代替案の種別による検索
- 埋め込みベースの類似検索条件

## 6.3 結果の意味

検索結果は少なくとも以下のどちらかを返せること。

- 一致した主張またはリソース
- 一致箇所を含む部分グラフ

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

# 7. 類似度と不確実性

## 7.1 類似度

SEMDL は埋め込みベクトルに基づく類似度計算を提供する。

- 主張単位または文書単位で比較できること
- 使用した埋め込みモデルを結果に含められること
- 類似度指標を明示できること

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
- ssd similarity <target1> <target2>
  - 類似度を計算する
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
  - オプションによりインライン出力または .ssm 分離出力を選べること
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

merge 時に .ssd と .ssm の間で競合がある場合、CLI は少なくとも以下のいずれかを選べること。

- エラー終了する
- 警告付きで優先規則に従う
- 明示オプションにより優先元を指定する

優先元を暗黙に切り替えてはならない。

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

ssd add <kind> は、少なくとも以下の kind を扱えることを推奨する。

- resource
- segment
- assertion
- hypothesis
- alternative
- provenance
- annotation

各 kind は、最低限 required field を持つこと。
required field が不足する場合は対話補完ではなく失敗を返し、明示入力を促す。

## 8.6.3 set と annotate の責務差分

set は、既存要素の正規フィールドまたはメタフィールドを更新する操作とする。

- 例: label、confidence、status、source_ref

annotate は、説明、注意、根拠、TODO などの付加注記を追加する操作とする。

- 例: rationale、caveat、todo

この区別により、構造変更と説明文追加を分離する。

## 8.6.4 remove の安全制約

remove は、初期仕様では以下の制約を持つことを推奨する。

- 参照されている構造要素の削除は既定で失敗する
- 補助メタ情報の削除は、構造整合性を壊さない限り許可する
- --cascade が明示された場合のみ従属要素削除を検討してよい

注記: これにより remove を最小限に保ちつつ、誤削除を避ける。

## 8.7 更新 CLI の共通オプション

更新系 CLI は少なくとも以下の共通オプションを持つことを推奨する。

- --target inline|sidecar|auto
  - 更新先を .ssd 本体、.ssm、または自動決定から選ぶ
- --dry-run
  - 実際には書き込まず変更内容のみ表示する
- --out <file>
  - 破壊的上書きではなく別ファイルへ出力する
- --format inline|sidecar
  - 出力プロファイルを指定する
- --fail-on-conflict
  - 競合検出時に失敗させる

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
- [docs/examples/golden/set-meta-A1-confidence.dryrun.stdout](docs/examples/golden/set-meta-A1-confidence.dryrun.stdout)
- [docs/examples/golden/annotate-H1-rationale.dryrun.stdout](docs/examples/golden/annotate-H1-rationale.dryrun.stdout)
- [docs/examples/golden/split-minimal.dryrun.stdout](docs/examples/golden/split-minimal.dryrun.stdout)
- [docs/examples/golden/remove-type-alternative.error.stderr](docs/examples/golden/remove-type-alternative.error.stderr)
- [docs/examples/golden/remove-id-A1.error.stderr](docs/examples/golden/remove-id-A1.error.stderr)
- [docs/examples/golden/annotate-invalid-kind.error.stderr](docs/examples/golden/annotate-invalid-kind.error.stderr)
- [docs/examples/golden/set-malformed-selector.error.stderr](docs/examples/golden/set-malformed-selector.error.stderr)
- [docs/examples/golden/check-unknown-option.error.stderr](docs/examples/golden/check-unknown-option.error.stderr)
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
- `ssd set path:A1.label "売上報告書" --dry-run docs/examples/minimal.ssd`
  - 本体フィールド変更予定のみを表示し、ファイルは変更しない
  - 期待出力は [docs/examples/golden/set-path-A1-label.dryrun.stdout](docs/examples/golden/set-path-A1-label.dryrun.stdout) と一致する
- `ssd set meta:A1.confidence 0.91 --dry-run docs/examples/minimal.ssd`
  - .ssm 側の変更予定のみを表示し、.ssd 本体の構造は変えない
  - 期待出力は [docs/examples/golden/set-meta-A1-confidence.dryrun.stdout](docs/examples/golden/set-meta-A1-confidence.dryrun.stdout) と一致する
- `ssd annotate id:H1 rationale "原文に主語がないため補完" --target sidecar --dry-run docs/examples/minimal.ssd`
  - 仮説 H1 に対する注記を .ssm 側へ追加予定として表示する
  - 期待出力は [docs/examples/golden/annotate-H1-rationale.dryrun.stdout](docs/examples/golden/annotate-H1-rationale.dryrun.stdout) と一致する
- `ssd merge docs/examples/minimal.ssd --stdout`
  - .ssd と .ssm を統合した単一表現を標準出力へ出す
  - 期待出力は [docs/examples/minimal.inline.ssd](docs/examples/minimal.inline.ssd) と一致する
- `ssd split docs/examples/minimal.ssd --dry-run`
  - インラインへ戻せる項目とサイドカーへ残る項目を確認できる
  - 期待出力は [docs/examples/golden/split-minimal.dryrun.stdout](docs/examples/golden/split-minimal.dryrun.stdout) と一致する

これらの例は説明用サンプルではなく、仕様の期待動作を固定するための最小受け入れケースとして扱う。

失敗系ケースも同様に固定することを推奨する。

- `ssd remove type:alternative docs/examples/minimal.ssd`
  - 複数対象に一致するため既定で失敗する
  - 期待 stderr は [docs/examples/golden/remove-type-alternative.error.stderr](docs/examples/golden/remove-type-alternative.error.stderr) と一致する
- `ssd remove id:A1 docs/examples/minimal.ssd`
  - 参照されている構造要素のため既定で失敗する
  - 期待 stderr は [docs/examples/golden/remove-id-A1.error.stderr](docs/examples/golden/remove-id-A1.error.stderr) と一致する
- `ssd annotate id:H1 note "補足" docs/examples/minimal.ssd`
  - 未定義 annotation kind のため失敗する
  - 期待 stderr は [docs/examples/golden/annotate-invalid-kind.error.stderr](docs/examples/golden/annotate-invalid-kind.error.stderr) と一致する
- `ssd set path:A1 0.91 docs/examples/minimal.ssd`
  - `path:` selector に dotted field path がないため字句または構文段階で失敗する
  - 期待 stderr は [docs/examples/golden/set-malformed-selector.error.stderr](docs/examples/golden/set-malformed-selector.error.stderr) と一致する
- `ssd check --strict docs/examples/minimal.ssd`
  - 未定義オプションのため option parsing 段階で失敗する
  - 期待 stderr は [docs/examples/golden/check-unknown-option.error.stderr](docs/examples/golden/check-unknown-option.error.stderr) と一致する
- `ssd annotate id:H1 rationale "未完 docs/examples/minimal.ssd`
  - quoted string が閉じていないため字句段階で失敗する
  - 期待 stderr は [docs/examples/golden/annotate-unterminated-quoted-string.error.stderr](docs/examples/golden/annotate-unterminated-quoted-string.error.stderr) と一致する

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

## 8.13 CLI 引数仕様の EBNF

初期 CLI 引数仕様は、曖昧な自然言語だけでなく、形式文法でも固定することを推奨する。

正式な初期文法は [docs/cli.ebnf](docs/cli.ebnf) に置く。

この EBNF は少なくとも以下を固定する。

- サブコマンド一覧
- add / set / remove / annotate の引数順序
- selector の構文
- 共通オプションの構文
- quoted string、number、boolean、identifier の字句境界
- `--allow-multi` のような更新系安全オプション

この EBNF に対応する初期 parser / lexer failure cases も golden として維持することを推奨する。

CLI の breaking change を行う場合は、受け入れ例だけでなく [docs/cli.ebnf](docs/cli.ebnf) も同一変更で更新すること。

## 8.14 test runner 実行契約

manifest 形式だけでなく、runner がどう実行し、どう比較するかの契約も固定することを推奨する。

初期契約は [docs/test-runner-contract.md](docs/test-runner-contract.md) に置く。

この契約は少なくとも以下を固定する。

- manifest の探索単位
- `argv` を正規入力として使うこと
- `stdin` と `environment` の適用順序
- stdout / stderr / exit code の比較順序
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
- `ssd set meta:A1.label "別名" docs/examples/minimal.ssd`
  - sidecar ではなく `.ssd` 本体に属する field のため失敗する
  - 期待 stderr は [docs/examples/golden/set-meta-wrong-layer.error.stderr](docs/examples/golden/set-meta-wrong-layer.error.stderr) と一致する

これらは selector 構文そのものではなく、selector が指す層と対象解決の境界を固定する failure cases である。

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
- LSP ベースのエディタ支援
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