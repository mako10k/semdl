# MCP Templates

このディレクトリには、互換 SEMDL MCP サーバを各 client に接続するための template artifacts を置きます。

重要: ここにある file はそのまま production config に使う前提ではなく、placeholder を実環境値へ置き換えるための template です。
また、stdio と HTTP の例は代替例です。同一名の server が両 transport を同時に提供することを意味しません。
利用可能な transport、server 名、required args は接続先 server 実装に従ってください。

## Placeholders

- `__SEMDL_MCP_COMMAND__`: local stdio server executable path
- `__SEMDL_MCP_URL__`: remote streamable HTTP endpoint
- `__SEMDL_PROJECT_ROOT__`: target project root path used by the first-party stdio server when `SEMDL_BINARY` is unset and `build/ssd` fallback should be available
- `__SEMDL_MCP_TOKEN__`: bearer token などの credential placeholder

## Files

- `vscode.mcp.template.jsonc`: VS Code workspace / user `mcp.json` 向け template
- `claude-desktop.template.jsonc`: Claude Desktop 向け template
- `claude-code.project.template.jsonc`: Claude Code project `.mcp.json` 向け template
- `generic-stdio.template.jsonc`: other stdio clients 向け generic template
- `generic-http.template.jsonc`: other remote HTTP clients 向け generic template
- `claude-code.commands.sh`: Claude Code CLI で server を追加する example commands

## Secret Handling

- token や API key を git 管理下へ commit しないでください
- template 中の placeholder は client の secret 機構や env expansion へ置き換えてください
- project-shared config を使う場合は secret を外出ししてください