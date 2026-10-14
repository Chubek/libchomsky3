#!/usr/bin/env sh
set -eu

cc -std=c11 -Wall -Wextra -Werror -I. AzmaIDL.c Main.c -o /tmp/azmaidl-cli

cat > /tmp/azma-valid.azmaidl <<'SRC'
metadata project = "demo";
config retries = 3;
SRC

cat > /tmp/azma-invalid.azmaidl <<'SRC'
config broken = [1 2];
SRC

/tmp/azmaidl-cli /tmp/azma-valid.azmaidl > /tmp/azma-valid.out 2> /tmp/azma-valid.err

if /tmp/azmaidl-cli /tmp/azma-invalid.azmaidl > /tmp/azma-invalid.out 2> /tmp/azma-invalid.err; then
  echo "runtime smoke failed: invalid input unexpectedly succeeded" >&2
  exit 1
fi

if ! rg -q "expected ',' or ']' in list" /tmp/azma-invalid.err; then
  echo "runtime smoke failed: expected diagnostic message not found" >&2
  exit 1
fi

echo "core runtime smoke: ok"
