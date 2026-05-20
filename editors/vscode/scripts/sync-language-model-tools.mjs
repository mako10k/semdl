import fs from 'node:fs';
import path from 'node:path';

const repoRoot = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..');
const packageJsonPath = path.join(repoRoot, 'package.json');
const catalogPath = path.join(repoRoot, 'tooling', 'semdl-tool-contracts.json');

const packageJson = JSON.parse(fs.readFileSync(packageJsonPath, 'utf8'));
const catalog = JSON.parse(fs.readFileSync(catalogPath, 'utf8'));

packageJson.contributes ??= {};
packageJson.contributes.languageModelTools = catalog.tools.map((tool) => ({
  name: tool.name,
  displayName: tool.displayName,
  userDescription: tool.userDescription,
  modelDescription: tool.modelDescription,
  inputSchema: tool.inputSchema,
  canBeReferencedInPrompt: tool.canBeReferencedInPrompt,
  toolReferenceName: tool.toolReferenceName,
  when: tool.when
}));

fs.writeFileSync(packageJsonPath, `${JSON.stringify(packageJson, null, 2)}\n`);