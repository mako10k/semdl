import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { getSemdlToolContracts } from './shared/toolContracts';
import { invokeSemdlTool } from './shared/semdlToolExecutors';
import { jsonObjectSchemaToZodObject, validateMcpToolInput } from './shared/mcpSchema';

async function main(): Promise<void> {
  const server = new McpServer({
    name: 'semdl-mcp',
    version: '0.0.1'
  });

  for (const contract of getSemdlToolContracts()) {
    server.registerTool(
      contract.name,
      {
        title: contract.displayName,
        description: contract.modelDescription,
        inputSchema: jsonObjectSchemaToZodObject(contract.inputSchema).shape
      },
      async (args) => {
        const validatedInput = validateMcpToolInput(contract.inputSchema, args as Record<string, unknown>);
        const execution = await invokeSemdlTool(contract.name, validatedInput, {
          binaryPath: process.env.SEMDL_BINARY,
          workspaceRoot: process.cwd()
        });

        return {
          content: [
            {
              type: 'text',
              text: execution.rendered
            }
          ]
        };
      }
    );
  }

  const transport = new StdioServerTransport();
  await server.connect(transport);
  console.error(`SEMDL MCP server started with tools: ${getSemdlToolContracts().map((tool) => tool.name).join(', ')}`);
}

void main().catch((error: unknown) => {
  const message = error instanceof Error ? `${error.message}\n${error.stack ?? ''}` : String(error);
  console.error(`SEMDL MCP server failed: ${message}`);
  process.exitCode = 1;
});