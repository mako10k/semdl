#!/bin/sh
set -eu

if [ "$#" -lt 3 ]; then
  echo "unexpected arguments" >&2
  exit 64
fi

subcommand="$1"
model="$2"
prompt="$3"

if [ "$subcommand" != "embeddings" ]; then
  echo "unsupported subcommand: $subcommand" >&2
  exit 64
fi

if [ "$model" = "fail-model" ]; then
  echo "simulated openai failure" >&2
  exit 1
fi

fixture_file="$(dirname "$0")/openai-embeddings.psv"

vector="$({ awk -F '|' -v model="$model" -v prompt="$prompt" 'NR > 1 && $2 == model && $3 == prompt { print $4; found = 1; exit } END { if (!found) exit 1 }' "$fixture_file"; } 2>/dev/null)" || {
  echo "unsupported prompt: $prompt" >&2
  exit 66
}

printf '%s\n' "$vector"
exit 0
