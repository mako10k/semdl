# SEMDL VS Code Scaffold

This directory contains the first implementation slice for SEMDL editor integration.

Current scope:
- VS Code extension manifest and extension entrypoint
- TypeScript build wiring for extension and language server
- Stub language server process entrypoint

Intentionally deferred:
- TextMate grammars for .ssd, .ssm, and .ssq
- VS Code language contributions and file associations
- Runtime LanguageClient wiring
- Diagnostics, symbols, hover, completion, and other language features

## Local development

Install dependencies and build the scaffold:

```bash
cd editors/vscode
npm install
npm run compile
```

The extension currently contributes a single command:
- `SEMDL: Show Language Server Scaffold Status`

That command confirms the scaffold is present. It does not launch the server yet.
