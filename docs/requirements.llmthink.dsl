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

step S27:
  decision D27 based_on D26:
    |
      self-contained canonical help の要件は、単に外部参照を外すことではない。
      利用者は `ssd help` を入口に、help topic と subcommand help だけで、
      help 入口形式、target 境界、selector 形式、値の受理形、共通 option、
      command-specific usage を取得できなければならない。
    annotation rationale:
      "参照を消しただけの薄い summary では、CLI help を正本とする要件を満たしたことにならないため"

step S28:
  decision D28 based_on D27:
    |
      help は homonymous option の役割差も説明しなければならない。
      少なくとも `--format semdl` と `--format inline|sidecar` は help 面の中で区別可能であること。
      `troubleshooting` は repository 文書参照に逃がさず、他の help surface への導線で閉じる。
    annotation rationale:
      "正本情報が help 内だけで取得できるなら、同名 option の意味衝突も help 内で解ける必要があるため"

step S2:
  decision D2 based_on D1:
    "初期更新機能は言語拡張ではなく CLI の最小集合 add / set / remove / annotate として提供する"
    annotation rationale:
      "対象ファイル、サイドカー書込先、dry-run、競合失敗、出力プロファイルをコマンドオプションで明示しやすい"
    annotation caveat:
      "条件式ベース更新や複数対象一括更新は初期必須要件に含めない"

step S2A:
  decision D2A based_on D2:
    |
      初期 `ssd add` slice は inline structural kind に限定し、
      resource、segment、assertion、hypothesis、alternative を対象にする。
      `provenance`、`annotation`、sidecar-targeted add、`--out`、`--stdout` は後続 slice に分離する。
    annotation rationale:
      "既存 renderer と acceptance fixture に乗る構造追加を先に実装すると、更新系最小集合の入口を小さく閉じられる"

step S2B:
  decision D2B based_on D2A, D3:
    |
      後続 `ssd add` slice では metadata-only kind として `annotation` と `provenance` を許可してよい。
      この slice の metadata add は `--target sidecar` 必須とし、既存 target id に対する create-only metadata field 作成だけを扱う。
      requested field が既に存在する場合は conflict failure とし、更新責務は `ssd set` または `ssd annotate` に残す。
      paired `.ssm` がない入力では sidecar を新規作成してよい。
      `--out` と `--stdout` は引き続き後続 slice に分離する。
    annotation rationale:
      "metadata add を create-only に限定すると、新規作成と既存値更新の責務が分離され、`add` と `set` と `annotate` の重なりを最小化できる"

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
      check、explain、set path dry-run/apply、set meta dry-run/apply、annotate dry-run/apply、split dry-run/apply、remove apply stdout の golden 出力を固定し、
      merge --stdout の期待値は minimal.inline.ssd として保持する。
    annotation rationale:
      "入力だけでなく出力も固定すると、CLI 表示仕様の回帰を早い段階で検出できる"

step S8:
  decision D8 based_on D3:
    |
      CLI の初期引数仕様は secondary operational artifact として EBNF でも整合確認できるようにする。
      サブコマンド、selector、共通オプション、字句境界に対応する補助的 formal notation artifact は docs/cli.ebnf に保持する。
    annotation rationale:
      "selector やオプション順序の曖昧さを減らし、将来の parser 実装とテストケース生成の基準にできる"

step S8A:
  decision D8A based_on D3:
    |
      grammar の主対象として優先するのは CLI argv ではなく、`.ssd` と `.ssq`、および将来の semql family である。
      CLI grammar は運用上必要でも、language grammar より上位の規範対象としては扱わない。
    annotation rationale:
      "SEMDL 自体と query layer の grammar を先に整えるほうが、仕様の焦点として自然であるため"

step S8B:
  decision D8B based_on D8A:
    |
      `.ssd` / `.ssm` の primary grammar artifact は docs/ssd.ebnf に、
      `.ssq` の primary grammar artifact は docs/ssq.ebnf に置く。
      ただし `.ssq` は initial minimal profile から始め、full query language の完成版を直ちに要求しない。
    annotation rationale:
      "priority shift を concrete artifact に落としつつ、sample 不足の `.ssq` で過剰設計しないため"

step S8C:
  decision D8C based_on D8B:
    |
      `.ssq` の initial minimal filter profile は、presence check と scalar equality check に限定する。
      `where` は sample-backed な最小構文のみを formalize し、scalar equality の右辺は quoted string、number、boolean に限定する。
      range 条件、論理結合、関数呼び出しは後続 slice に分離する。
    annotation rationale:
      "filter slot の存在だけを先に置くと中身が空洞化する一方、豊富な条件構文を先回りで固定すると sample 不足の段階では過剰設計になるため"

step S8J:
  decision D8J based_on D8C:
    |
      後続 filter slice では `.ssq` の `where` に numeric range comparison と boolean expression を追加してよい。
      range operator は `>`, `>=`, `<`, `<=` に限定し、右辺は number に限る。
      logical composition は `and` / `or` の mixed expression と parenthesized grouping を受け付けてよく、precedence は `and` を `or` より高く扱ってよい。
      unary `not` は filter term または parenthesized group の前に置いてよく、`and` より高い precedence で repeated unary `not` も許可してよい。
      unmatched parenthesis、empty grouping、group adjacency の欠落、operand のない dangling `not` は validation failure とする。
      current `where` profile では operand position の `not` は reserved keyword とし、field name `not` は受け付けない。
      関数呼び出しと新 predicate は引き続き deferred とする。
      `=` は既存互換の token equality を維持し、range comparison で左辺 field が欠損または非数値の candidate は failure ではなく no-match とする。
    annotation rationale:
      "full expression language へは踏み込まず、sample-backed な precedence と grouping だけを先に formalize するため"

step S8D:
  decision D8D based_on D8B, D4:
    |
      埋め込み生成は read path ではなく explicit write path の責務とする。
      `ssd search`、`ssd similarity`、`.ssq` evaluation は既存埋め込みを読むだけに留め、暗黙の再計算や自動永続化を行わない。
    annotation rationale:
      "read-only query language と検索 command の責務を保ったまま、provider 設定や永続化ポリシーの混入を避けるため"

step S8E:
  decision D8E based_on D8D:
    |
      埋め込みの persist 境界は、`.ssd` 本体には existence だけを残し、ベクトル本体と詳細メタデータは `.ssm` または外部参照へ置く。
      ranking や一時 index のような derived data は persist 必須にしない。
    annotation rationale:
      "本体の可読性を保ちつつ、検索に必要な補助データを sidecar / external reference に逃がせるようにするため"

step S8F:
  decision D8F based_on D8D, D8E:
    |
      初期 similarity resolution は precomputed embedding 前提とし、`.ssq` の `similar` は既存 target 基準、`ssd similarity` は既存 2 target の pairwise 比較とする。
      同一 model と dimensions の組だけを既定で比較対象にし、similarity metric は execution policy 側の source of truth とする。
      初期既定値は cosine としてよいが、結果には left/right operand、使用 metric、model、dimensions、score を明示し、free-text query や cross-model alignment は後続 slice に分離する。
      first slice の CLI operand は `ssd similarity <target1> <target2> <file>` とし、embedding の `vector` と `vector_ref` の両方を解決できるようにする。
      `.ssq` の `similar` は統合ビュー全体で anchor/candidate を解決し、anchor 自身を除外した上で、`return: matches` では score 降順と安定 tie-break で返す。
      search-time similarity で anchor/candidate の embedding が欠落または不正な場合は skip に劣化させず failure とし、relative `vector_ref` は embedding record を宣言したファイル基準で解決する。
    annotation rationale:
      "similarity slot と pairwise command の責務を明確にし、未承認の text-to-vector や cross-model 変換を同時に持ち込まないため"

step S8G:
  decision D8G based_on D8D, D8E:
    |
      最初の埋め込み生成 command owner は `ssd extract` とし、初期 slice では既存 `.ssd` を semantic source として受け取り、
      `--embed-provider <provider> --embed-model <model>` を明示した場合だけ paired `.ssm` に embedding record を生成する。
      初期 supported provider は `ollama` と `openai` に限定し、互換 SDK 前提ではなく explicit adapter 境界で分離する。
      `--embed-provider` と `--embed-model` は両方そろったときだけ有効とし、片側だけなら failure とする。
      embeddable text は kind ごとの canonical field を 1 つだけ持ち、record は model、dimensions、generated_at、provider、source_field、vector を保持する。
    annotation rationale:
      "dedicated command の増設を避けつつ provider surface を explicit adapter で広げ、ADR 0008 の explicit write-path 境界を保つため"

step S8H:
  decision D8H based_on D8F:
    |
      `ssd search` の `return: subgraph` は structural query だけでなく `similar` を伴う query でも受け付けてよい。
      similarity-backed subgraph result でも grouped result を維持し、top-level には anchor、metric、model、dimensions を常に出す。
      各 group には match_file、match_id、match_kind、score、context_nodes を持たせ、context expansion は既存 outbound one-hop 規則を再利用する。
      result 順序は score 降順と既存 stable tie-break に従い、anchor 自身は結果から除外する。
    annotation rationale:
      "ADR 0010 の grouped structural contract を保ったまま similarity search へ拡張し、matches/subgraph 間の result symmetry を保つため"

step S8I:
  decision D8I based_on D8G:
    |
      `ssd extract` は plain `.txt` input を受け付け、初期 raw extraction では document D1、resource R1、non-empty line ごとの segment S<n> を生成して skeletal `.ssd` を返してよい。
      document.title と resource.label は input stem を使い、document.source_ref には command line で指定した input path を保持する。
      no-provider の `--stdout` は 1 件以上の existing `.ssd` / plain `.txt` input を受け付け、resolved view を 1 つの canonical inline `.ssd` として返してよい。
      raw `.txt` input に対する `--stdout` は generated `.ssd` だけを返し、embedding option との併用は failure とする。
      embedding-enabled `--stdout` は single existing `.ssd` に限って `--format inline|sidecar` を受け付けてよく、`--format` omitted と `--format sidecar` は generated `.ssm` profile text を返してよい。
      embedding-enabled `--stdout --format inline` は generated embeddings を含む single-file `.ssd` text を返してよく、assertion / hypothesis は inline `meta` を使い、other embeddable kinds は same payload 内の top-level `meta <id>` block を使ってよい。
      embedding-enabled `--stdout --format bundle` は line-preserving plain-text bundle を返してよく、header 順序は `stdout_profile`、`inline_profile`、`sidecar_profile`、payload section は `inline_document:` と `sidecar_document:` に固定し、payload line は `|` prefix で保持する。
      embedding-enabled `--stdout` は one or more existing `.ssd` input を受け付けてよいが、multi-input のとき omitted `--format` は失敗とし、`inline|sidecar|bundle` を明示必須とする。
      multi-input embedding-enabled `--stdout --format inline` は rebased merged view の canonical inline `.ssd` を返し、`--format sidecar` は rebased merged `.ssm` を返し、`--format bundle` は両 payload を同じ rebased view から返してよい。
      no-provider `--stdout` と `--out <output.ssd>` はこの slice でも `--format bundle` を受け付けず、raw `.txt` を含む embedding-enabled stdout は single/multi とも failure とする。
      raw `.txt` input の embedding generation は `--out <output.ssd>` 成功時だけ許可し、embeddable target は generated resource.label と segment.text_quote に限定し、document D1 は対象に含めない。
      後続 multi-input out slice では `ssd extract --out <output.ssd> <input>...` に限って mixed `.ssd` / `.txt` input を受け付け、resolved view を 1 つの canonical inline `.ssd` に集約してよい。
      source 間の entity ID 衝突は deterministic rebasing で解決し、`alternative_group` / `alternative.group` token も source ごとに rebased value へ書き換える。
      embedding option を伴う場合の generated `.ssm` は rebased merged view に対して 1 つだけ生成し、`--out` は source input、source sidecar、generated sidecar と alias する equivalent path を拒否する。
    annotation rationale:
      "single-input omitted default を壊さずに multi-input embedding stdout と explicit bundle multiplexing を追加し、既存 aggregate/rebase path を再利用できるため"

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
      stdin、environment、expected_output_kind も case 単位で固定し、
      file-writing case では setup_files と expected_files により sandboxed file output も固定して、runner 実装差を減らす。
    annotation rationale:
      "parser や lexer の失敗系は shell の再解釈に左右されやすく、argv を正にした方が再現性が高い"

step S12:
  decision D12 based_on D8, D10:
    |
      EBNF に対応する初期 parser / lexer failure cases として、
      malformed selector、leading / trailing unknown option、unterminated quoted string を golden 化する。
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
      write case では setup_files を sandbox へ複製し、declared expected_files を fixture と比較する。
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

step S18A:
  decision D18A based_on D18:
    |
      初期 `ssd normalize` の stdout 面は `--stdout` で canonical inline view を返す。
      paired `.ssm` があれば統合し、なければ standalone `.ssd` を block / field の canonical order へ整形する。
      bare `ssd normalize <input.ssd>` は standalone と paired の両方で apply を許可し、paired 入力では canonical inline `.ssd` へ収束させたうえで sibling `.ssm` を削除する。
      bare `ssd merge <input.ssd>` は paired `.ssm` がある入力に限って apply を許可し、統合後の inline `.ssd` を書き戻したうえで sibling `.ssm` を削除する。
      paired sidecar がない入力に対する bare merge apply は、この段階では merge と normalize の責務境界を保つため失敗する。
      `--out`、`--inline`、`--sidecar`、`--dry-run` は後続 slice に分離する。
    annotation rationale:
      "paired apply を inline `.ssd` へ収束させて sibling `.ssm` を除去すれば stale sidecar state を残さず、merge と normalize の write path を最小対称面で解禁できるため"

step S18B:
  decision D18B based_on D18A:
    |
      後続 transform slice では `merge` と `normalize` の両方に `--dry-run` と `--out <output.ssd>` を追加してよい。
      `merge` の `--dry-run` と `--out` は paired `.ssm` がある入力に限って成功し、standalone 入力には広げない。
      `normalize` の `--dry-run` と `--out` は standalone / paired の両方で成功してよい。
      `--out` は non-destructive とし、source `.ssd` と paired `.ssm` を変更または削除しない。
      また `--out` は source `.ssd` や paired `.ssm` を alias してはならない。
      `--inline`、`--sidecar`、profile selection、conflict policy は引き続き後続 slice に分離する。
    annotation rationale:
      "merge と normalize の option 面を `--dry-run` と `--out` に絞ると、paired apply の責務境界を保ったまま非破壊 preview と別ファイル出力を対称に追加できる"

step S18C:
  decision D18C based_on D2B, D18B:
    |
      後続 update slice では `ssd annotate` に `--target inline|sidecar|auto` を追加してよい。
      `--target inline` は standalone `.ssd` 入力に限り成功し、対象 kind は `assertion` と `hypothesis` に限定する。
      `--target auto` は paired 入力なら sidecar を選び、standalone 入力なら inline が許される target だけ inline を選び、それ以外は sidecar へ落としてよい。
      同じ slice で `ssd remove` は structural single-target delete を許可してよいが、inbound reference がある structural target は既定で failure とする。
      `--cascade` が明示されたときだけ direct / transitive dependent を同時削除でき、initial dependency edge は `assertion <- hypothesis.about`、`hypothesis <- alternative.group`、`resource <- segment.source` に固定する。
      `type:<kind> --allow-multi` 以外の multi-target structural remove は引き続き後続 slice に分離する。
    annotation rationale:
      "annotate の target 行列と remove cascade edge を同時に固定すると、inline / sidecar write-path と安全削除境界を acceptance でぶらさず実装できる"

step S18D:
  decision D18D based_on D18B:
    |
      次の transform profile slice では `normalize` の non-destructive surface に限って `--format inline|sidecar` を追加してよい。
      `normalize --dry-run` と `normalize --out <output.ssd>` は `--format` 未指定時に inline を既定とし、`--format sidecar` では output `.ssd` と sibling `.ssm` pair を preview または生成してよい。
      bare `normalize <input.ssd>` と `normalize --stdout` は inline-only のままとし、この slice では `--format` を受けない。
      `merge` は inline-oriented command、`split` は sidecar-oriented command のままとし、profile selection は広げない。
      conflict policy は引き続き後続 slice に分離してよい。
    annotation rationale:
      "non-destructive normalize surface に profile selection を閉じると、merge / split の既存責務境界を壊さずに profile option を先に固定できる"

step S18E:
  decision D18E based_on D18B, D18D:
    |
      次の merge conflict slice では `ssd merge` に限って trailing `--fail-on-conflict` を追加してよい。
      `merge --fail-on-conflict` は merged view ではなく pre-merge source comparison で differing duplicate を検出し、stdout、dry-run、out、apply のいずれでも file mutation や output 前に failure する。
      comparison scope は inline source の document body にある `version` / `generator` と、inline `meta {}`、paired `meta`、paired `document_meta` に現れる field に限定してよい。
      `embedding.*` は leaf field 単位で比較し、same-value duplicate は success してよい。
      flag なしの `merge` は current behavior を明示し、paired `.ssm` 側を sidecar-owned duplicate field の既定優先元として扱ってよい。
      warning mode と preferred-source selection は引き続き後続 slice に分離する。
    annotation rationale:
      "既存 merge 出力互換を保ったまま、source-aware な safety option を merge にだけ狭く追加できるため"

step S27A:
  decision D27A based_on D27, D8A:
    |
      `ssd help grammar` は language grammar の完全記述ではなく、CLI を使うための operational syntax summary とする。
      detailed command usage は reference help で補い、language grammar work は `.ssd` と `.ssq` / semql 側へ寄せる。
    annotation rationale:
      "help を入口として保ちつつ、grammar の主戦場を CLI 以外へ移すため"

step S19:
  decision D19 based_on D18, D6:
    |
      最初の implementation change plan は、既存の plan-semdl-spec-change prompt または semdl-spec-change skill を入口として起こす。
      初手の実装 slice は core library の parse / validate / resolve と、CLI の check / explain に必要な最小経路を優先し、
      runner 自体の実装は後続 slice とする。
    annotation rationale:
      "最小の read-only 経路から入ると、golden success cases と requirements への適合を狭い範囲で検証しやすい"

problem P6:
  "SEMDL の利用者が CLI から仕様、文法、例、注意点へ一貫した導線で到達するには、help をどう構造化するか"
  annotation rationale:
    "usage 断片と docs 参照だけでは、unknown option や missing argument から適切な次アクションへ辿りにくい"

step S20:
  decision D20 based_on P6, D13:
    |
      SEMDL の help 正本は CLI とする。
      `ssd help` と `ssd --help` は root help を表示し、
      overview、toc、grammar、reference、recipes、samples、troubleshooting の topic 導線を持つ。
    annotation rationale:
      "CLI を正本にすると、利用者の最初の入口と error からの導線を同じ public contract に載せられる"

step S21:
  decision D21 based_on D20:
    |
      root help は少なくとも 概要、目次、文法、リファレンス、逆引き、サンプル、注意点・既知バグ・報告先 の 7 節を持つ。
      `ssd help reference <subcommand>` と `ssd help recipes <topic>` のような topic 細分化も許容する。
    annotation rationale:
      "長い root help を topic help で分割できると、構造を保ったまま入口ごとに短く案内できる"

step S22:
  decision D22 based_on D20, D12:
    |
      unknown option、missing required argument、unknown help topic、unimplemented subcommand、explain target not found は、
      対応する help topic または subcommand help への案内を含む。
      help 自体も success/failure golden と manifest で受け入れ面を固定する。
    annotation rationale:
      "error を単独文字列で終わらせず、次の学習・修正経路へ接続すると CLI 全体の理解コストを下げられる"

problem P7:
  "SEMDL の help を人間向け text と機械処理向け表現の両方で出したいとき、既定の可読性を崩さずにどう共存させるか"
  annotation rationale:
    "help を完全に機械向けへ寄せると CLI の入口として読みにくくなり、逆に text 固定だと外部ツールから構造的に扱いにくい"

step S23:
  decision D23 based_on P7, D20:
    |
      help の既定出力は従来どおり text のまま維持し、
      opt-in で `--format semdl` を受け付ける。
      対象は `ssd help`、`ssd --help`、`ssd <subcommand> --help` とする。
    annotation rationale:
      "既定の使い勝手を壊さずに machine-readable surface を追加できる"

step S24:
  decision D24 based_on D23:
    |
      `--format semdl` の help 出力は line-preserving な SEMDL profile とし、
      `document`、`resource`、`segment` block で文面順序を保持する。
      初期 slice では CLI 出力契約として固定し、core parser の再読込互換までは要求しない。
    annotation rationale:
      "help の文面を既存 text 契約と 1 対 1 で対応付けやすく、golden 管理もしやすい"

step S25:
  decision D25 based_on D21, D22:
    |
      未実装 subcommand であっても、公開された command 名には command-specific help を用意する。
      初期受け入れ面では search、extract、similarity、add、normalize の `--help` も固定する。
    annotation rationale:
      "未実装時でも使い方と状態を CLI 上で説明できると、subcommand_not_implemented error からの導線が途切れない"

step S26:
  decision D26 based_on D20, D21:
    |
      grammar topic を含む CLI help は user-facing な規範的要約を自前で保持する。
      `docs/cli.ebnf` は repository 上の aligned formal notation artifact として残してよいが、
      CLI help は利用者をそのファイル参照へ導いてはならない。
    annotation rationale:
      "help を正本とする原則を保つには、grammar help が別 artifact への単純な権威移譲にならないことを明示する必要がある"