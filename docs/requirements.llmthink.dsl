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

step S11:
  decision D11 based_on D9:
    |
      test runner manifest の正規入力は command 文字列だけでなく argv 配列としても保持する。
      stdin、environment、expected_output_kind も case 単位で固定し、runner 実装差を減らす。
    annotation rationale:
      "parser や lexer の失敗系は shell の再解釈に左右されやすく、argv を正にした方が再現性が高い"

step S12:
  decision D12 based_on D8, D10:
    |
      EBNF に対応する初期 parser / lexer failure cases として、
      malformed selector、unknown option、unterminated quoted string を golden 化する。
      これらは command 表示とは別に argv ベースで実行条件を固定する。
    annotation rationale:
      "字句境界と option parsing の回帰は成功系だけでは検出しづらく、EBNF に直結した失敗系が必要である"

problem P4:
  "SEMDL の architecture 変更と runner 公開契約を、どの文書単位で固定するか"
  annotation rationale:
    "requirements だけでは判断理由や supersede 関係が埋もれやすく、runner 契約変更も追跡しづらい"

step S13:
  decision D13 based_on P4:
    |
      SEMDL の architecture 判断は docs/adr 以下の ADR で管理する。
      format 境界、CLI 公開契約、selector 解決規則、runner 契約の変更は ADR 対象とする。
    annotation rationale:
      "要求仕様と判断理由を分けると、why と boundary の変更履歴を保持しやすい"

step S14:
  decision D14 based_on D11, D13:
    |
      test runner には manifest 形式とは別に実行契約文書を持たせる。
      runner は argv を正規入力とし、shell を介さずにプロセスを起動し、
      expected_output_kind に従って stdout / stderr を比較する。
    annotation rationale:
      "manifest だけでは discovery と比較順序が曖昧になりやすく、実行契約を別文書で固定した方が実装差を減らせる"

step S15:
  decision D15 based_on D3, D10:
    |
      selector failure cases には字句失敗だけでなく、
      missing target、wrong layer for path、wrong layer for meta を含める。
      これにより selector の解決対象と層境界も golden 化する。
    annotation rationale:
      "selector は構文だけ合っていても誤った層に向けると破綻するため、解決境界の failure cases が必要である"

problem P5:
  "implementation フェーズへ入る前に、core library、ssd CLI、test runner の責務をどう分離するか"
  annotation rationale:
    "実装着手前に層境界を固定しないと、CLI 引数処理、文書意味解釈、golden 比較が癒着しやすい"

step S16:
  decision D16 based_on P5, D14:
    |
      初期実装では core library、ssd CLI、test runner を分離する。
      core library は parse、validate、resolve、merge view を担当し、
      ssd CLI は引数解釈と入出力とファイル更新を担当し、
      test runner は manifest 読み込みと argv 実行と golden 比較を担当する。
    annotation rationale:
      "意味解釈、CLI 契約、テスト実行契約を分離すると、将来の組み込み利用と回帰テスト維持がしやすい"

step S17:
  decision D17 based_on D16:
    |
      test runner は CLI の外側に位置し、初期段階では core library API に直接依存しない。
      期待動作の正本は CLI 契約と golden assets に置き、ライブラリ内部 API を runner 契約へ露出しない。
    annotation rationale:
      "runner が library へ直接結合すると、CLI 公開契約と golden 回帰の境界が崩れやすい"

step S18:
  decision D18 based_on D16:
    |
      初期実装では repository layout も 3 層境界に合わせて分ける。
      core library、ssd CLI、test runner は別ディレクトリとして配置し、依存方向は library <- CLI <- runner ではなく、
      library を CLI が利用し、runner は CLI を外部プロセスとして実行する構成を保つ。
    annotation rationale:
      "責務だけでなく配置先も分けると、実装初期から層の癒着を防ぎやすい"

step S19:
  decision D19 based_on D18, D6:
    |
      最初の implementation change plan は、既存の plan-semdl-spec-change prompt または semdl-spec-change skill を入口として起こす。
      初手の実装 slice は core library の parse / validate / resolve と、CLI の check / explain に必要な最小経路を優先し、
      runner 自体の実装は後続 slice とする。
    annotation rationale:
      "最小の read-only 経路から入ると、golden success cases と requirements への適合を狭い範囲で検証しやすい"