framework SemdlRequirements:
  requires problem and decision

domain SEMDL:
  description "SEMDL requirements decisions for query, mutation, and CLI update boundaries"

problem P1:
  "SEMDL で将来 semql を導入する場合、参照系と更新系の責務をどう分離するか"
  annotation rationale:
    "検索言語に更新を混在させると、サイドカー更新先、競合解決、dry-run、整形方針まで同時設計になり過剰化しやすい"

step S1:
  decision D1 based_on P1:
    |
      初期仕様では .ssq を参照専用に維持する。
      将来 semql を導入する場合は query と mutation を分離し、
      split・merge・normalize は admin または transform レイヤとして分ける。
    annotation rationale:
      "参照系と更新系を分離すると、読み取り専用の理解容易性を保ちつつ、運用変換を mutation から切り離せる"

step S2:
  decision D2 based_on D1:
    "初期更新機能は言語拡張ではなく CLI の最小集合 add / set / remove / annotate として提供する"
    annotation rationale:
      "対象ファイル、サイドカー書込先、dry-run、競合失敗、出力プロファイルをコマンドオプションで明示しやすい"
    annotation caveat:
      "条件式ベース更新や複数対象一括更新は初期必須要件に含めない"

problem P2:
  "初期更新 CLI において、更新対象 selector と各レイヤの責務をどの粒度まで固定するか"
  annotation rationale:
    "selector が曖昧だと単一更新が事故りやすく、レイヤ責務が曖昧だと .ssd と .ssm と CLI と semql の境界が崩れる"

step S3:
  decision D3 based_on P2, D2:
    |
      初期更新 CLI の selector は id:<id>、path:<id>.<field>、
      meta:<id>.<field>、doc:self を中心とした単一対象解決の最小構文に限定する。
      複数対象一致は既定で失敗とする。
    annotation rationale:
      "初期段階では一意解決を優先し、条件式更新や複数対象更新は後回しにした方が安全である"

step S4:
  decision D4 based_on D3:
    |
      責務対応は .ssd を意味構造本体、.ssm を補助メタ情報、
      .ssq を参照クエリ、ssd CLI を当面の更新経路、
      semql を将来の query / mutation / admin 分離レイヤとして整理する。
    annotation rationale:
      "レイヤごとの役割を表で固定すると、何を人手編集し、何を検索専用に保ち、何を先に CLI で実装するかを説明しやすい"

problem P3:
  "SEMDL の仕様追加と例追加を、どの開発順序で固定するか"
  annotation rationale:
    "サイドカー、selector、CLI 更新のように相互依存がある領域では、実装先行だと期待動作がぶれやすい"

step S5:
  decision D5 based_on P3:
    |
      SEMDL の parser、validator、CLI はテストファーストを基本方針とする。
      仕様を追加するときは、先に受け入れ例または golden test 候補を固定してから実装する。
    annotation rationale:
      "CLI 期待動作を先に固定すると、.ssd / .ssm の責務分担や selector 解決規則のぶれを抑えられる"

step S6:
  decision D6 based_on D5, D3:
    |
      初期受け入れ例には、minimal.ssd と minimal.ssm を用いた
      check、explain、set path、set meta、annotate、merge、split を含める。
      これらの例は説明用サンプルではなく、将来の acceptance test 資産として維持する。
    annotation rationale:
      "単一対象 selector とサイドカー更新を同時に検証できる最小ケースを早めに固定すると回帰防止に効く"

step S7:
  decision D7 based_on D6:
    |
      minimal 系サンプルには、入力ファイルだけでなく expected stdout も用意する。
      check、explain、set path dry-run、set meta dry-run、annotate dry-run、split dry-run の golden 出力を固定し、
      merge --stdout の期待値は minimal.inline.ssd として保持する。
    annotation rationale:
      "入力だけでなく出力も固定すると、CLI 表示仕様の回帰を早い段階で検出できる"

step S8:
  decision D8 based_on D3:
    |
      CLI の初期引数仕様は自然言語説明だけでなく EBNF でも固定する。
      サブコマンド、selector、共通オプション、字句境界は docs/cli.ebnf に集約する。
    annotation rationale:
      "selector やオプション順序の曖昧さを減らし、将来の parser 実装とテストケース生成の基準にできる"

step S9:
  decision D9 based_on D7:
    |
      CLI の受け入れ例と golden test は、runner が機械収集できる JSON manifest 形式で保持する。
      case ごとに command、expected_exit、expected_stdout、expected_stderr を固定し、
      成功系と失敗系を同じ構造で扱う。
    annotation rationale:
      "runner 入力形式を先に固定すると、テスト実装が docs 依存で自動収集しやすくなる"

step S10:
  decision D10 based_on D9, D6:
    |
      初期 failure cases として、複数対象 selector の既定失敗、
      参照中構造要素の remove 失敗、未定義 annotation kind の parse 失敗を golden 化する。
      失敗系は stderr golden と expected_exit を必須にする。
    annotation rationale:
      "成功系だけでなく失敗系を早めに固定すると、安全制約とエラーメッセージ方針の回帰を防ぎやすい"