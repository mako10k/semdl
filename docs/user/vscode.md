# VS Code Guide

SEMDL には `.ssd`（SEMDL Canvas）、`.ssm`（SEMDL Sidekick）、`.ssq`（SEMDL Lens）向けの first-party VS Code extension があります。

現在の主な範囲は次のとおりです。

- file type registration
- TextMate syntax highlighting
- basic diagnostics
- top-level document symbols

## Local Build

現時点では、手元で build して使うのが基本です。

```sh
cd editors/vscode
npm install
npm run bundle
```

VSIX を作る場合は次を使います。

```sh
cd editors/vscode
npm install
npm run package:vsix
```

生成物は `editors/vscode/dist/` に出ます。

## What To Expect

- `.ssd`、`.ssm`、`.ssq` の language identification
- syntax diagnostics
- top-level document symbols

次の richer IDE features は後続 slice 扱いです。

- hover
- completion
- rename
- formatting
- semantic tokens

## More Detail

build や packaging の詳細は [../../editors/vscode/README.md](../../editors/vscode/README.md) を参照してください。