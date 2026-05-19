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

Expected editor diagnostics can be suppressed for intentionally invalid fixtures with a same-directory `.semdl-diagnostics.json` file.
The initial policy surface is intentionally narrow:
- only the diagnosed file's own directory is consulted
- rules must match an exact file name in that directory
- each suppressed diagnostic must match an exact 1-based line number and exact message
- wildcard and inherited suppression are ignored

Example:

```json
{
	"expected_diagnostics": [
		{
			"file": "invalid-range-quoted-number-filter.ssq",
			"allow": [
				{
					"line": 3,
					"message": "Range filters require a numeric value."
				}
			]
		}
	]
}
```

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

## License

This extension is distributed under the Mozilla Public License 2.0.
The corresponding Source Code Form is available at https://github.com/mako10k/semdl.
