#!/bin/sh
set -eu

if [ "$#" -lt 3 ]; then
  echo "unexpected arguments" >&2
  exit 64
fi

subcommand="$1"
model="$2"
prompt="$3"

if [ "$subcommand" != "run" ]; then
  echo "unsupported subcommand: $subcommand" >&2
  exit 64
fi

if [ "$model" = "fail-model" ]; then
  echo "simulated ollama failure" >&2
  exit 1
fi

if [ "$model" != "nomic-embed-text:latest" ]; then
  echo "unsupported model: $model" >&2
  exit 65
fi

fixture_file="$(dirname "$0")/ollama-embeddings.psv"

vector="$({ awk -F '|' -v prompt="$prompt" -v model="$model" 'NR > 1 && $4 == prompt && $6 == model { print $9; found = 1; exit } END { if (!found) exit 1 }' "$fixture_file"; } 2>/dev/null)" || {
  echo "unsupported prompt: $prompt" >&2
  exit 66
}

printf '%s\n' "$vector"
exit 0
