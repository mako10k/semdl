import test from 'node:test';
import assert from 'node:assert/strict';
import * as path from 'node:path';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import { listSemdlToolNames } from './shared/toolContracts';

test('mcp server lists the shared SEMDL tools over stdio', async () => {
  const vscodeRoot = path.resolve(__dirname, '..', '..');
  const serverEntry = path.resolve(__dirname, 'mcpServer.js');
  const transport = new StdioClientTransport({
    command: process.execPath,
    args: [serverEntry],
    cwd: vscodeRoot,
    stderr: 'pipe'
  });
  const client = new Client({
    name: 'semdl-mcp-smoke-test',
    version: '0.0.1'
  });

  let stderr = '';
  transport.stderr?.on('data', (chunk) => {
    stderr += chunk.toString();
  });

  try {
    await client.connect(transport);
    const result = await client.listTools();
    assert.deepEqual(
      result.tools.map((tool) => tool.name),
      listSemdlToolNames()
    );
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    throw new Error(`${message}\nMCP stderr:\n${stderr}`);
  } finally {
    await client.close();
  }
});