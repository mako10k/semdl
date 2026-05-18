# SEMDL VS Code Extension

This directory contains the first implementation slice for SEMDL editor integration.

Current scope:
- VS Code extension manifest and extension entrypoint
- Language registration for `.ssd`, `.ssm`, and `.ssq`
- TextMate grammars for `.ssd`, `.ssm`, and `.ssq`
- Runtime LanguageClient wiring for `.ssd`, `.ssm`, and `.ssq`
- Basic syntax diagnostics and top-level document symbols
- TypeScript build wiring for extension and language server
- Language server entrypoint

Intentionally deferred:
- Richer diagnostics, hover, completion, rename, formatting, code actions, and semantic tokens

## Local development

Install dependencies and build the scaffold:

```bash
cd editors/vscode
npm install
npm run compile
```

Bundle the runtime files used by the packaged extension:

```bash
cd editors/vscode
npm install
npm run bundle
```

Build a VSIX package locally:

```bash
cd editors/vscode
npm install
npm run package:vsix
```

The generated package is written to `dist/semdl-<version>.vsix`.
The packaged runtime is emitted to `bundle/extension.js` and `bundle/server.js`.

The extension currently contributes a single command:
- `SEMDL: Show Language Server Status`

That command reports the current extension and LSP status. The language server starts automatically for `.ssd`, `.ssm`, and `.ssq` documents.
