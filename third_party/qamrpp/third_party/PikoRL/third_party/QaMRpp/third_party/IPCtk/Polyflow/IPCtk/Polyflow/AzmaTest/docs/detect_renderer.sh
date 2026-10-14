#!/usr/bin/env sh
set -eu

for tool in pandoc mdbook marked markdown; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf '%s\n' "$tool"
    exit 0
  fi
done

echo "error: no supported docs renderer found (tried: pandoc mdbook marked markdown)" >&2
exit 1
