import test from 'node:test';
import assert from 'node:assert/strict';
import * as fs from 'node:fs';
import * as os from 'node:os';
import * as path from 'node:path';
import { listSemdlBinaryCandidates } from './semdlBinaryProxy';

test('binary candidates try PATH before project build fallback', () => {
  const temporaryProjectRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'semdl-project-'));
  const buildDirectory = path.join(temporaryProjectRoot, 'build');
  const buildBinary = path.join(buildDirectory, 'ssd');
  fs.mkdirSync(buildDirectory, { recursive: true });
  fs.writeFileSync(buildBinary, '#!/bin/sh\nexit 0\n');

  const candidates = listSemdlBinaryCandidates({ workspaceRoot: temporaryProjectRoot });
  assert.deepEqual(candidates, ['ssd', buildBinary]);
});

test('binary candidates honor SEMDL_PROJECT_ROOT when workspaceRoot is absent', () => {
  const temporaryProjectRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'semdl-project-env-'));
  const buildDirectory = path.join(temporaryProjectRoot, 'build');
  const buildBinary = path.join(buildDirectory, 'ssd');
  fs.mkdirSync(buildDirectory, { recursive: true });
  fs.writeFileSync(buildBinary, '#!/bin/sh\nexit 0\n');

  const candidates = listSemdlBinaryCandidates({ env: { SEMDL_PROJECT_ROOT: temporaryProjectRoot } });
  assert.deepEqual(candidates, ['ssd', buildBinary]);
});