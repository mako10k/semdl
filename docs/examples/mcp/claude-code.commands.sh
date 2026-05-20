#!/usr/bin/env sh

# Replace the placeholders before running these commands.

# Add a project-scoped stdio server.
claude mcp add --transport stdio --scope project semdl -- "__SEMDL_MCP_COMMAND__"

# Add a user-scoped remote HTTP server.
claude mcp add --transport http --scope user semdl-remote "__SEMDL_MCP_URL__"

# Add the same server from inline JSON instead of flags.
claude mcp add-json semdl '{"type":"stdio","command":"__SEMDL_MCP_COMMAND__","args":[],"env":{"SEMDL_PROJECT_ROOT":"__SEMDL_PROJECT_ROOT__"}}'