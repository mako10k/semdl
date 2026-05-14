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