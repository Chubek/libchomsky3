#!/usr/bin/env bash
set -euo pipefail
entry=''
if [ -f ./MANIFEST.json ]; then
  entry="$(python3 - <<'PY'
import json, pathlib
m = json.loads(pathlib.Path('MANIFEST.json').read_text())
print(m.get('entrypoint', ''))
PY
)"
fi
if [ -n "$entry" ] && [ ! -f "$entry" ]; then entry=''; fi
if [ -z "$entry" ] && [ -f ./entrypoint.sh ]; then entry=./entrypoint.sh; fi
if [ -z "$entry" ] && [ -f ./entrypoint.py ]; then entry=./entrypoint.py; fi
if [ -z "$entry" ]; then
  entry="$(find . -maxdepth 2 -type f -name 'entrypoint*' | head -n 1 || true)"
fi
if [ -z "$entry" ]; then
  echo "No entrypoint found in package." >&2
  exit 1
fi
case "$entry" in
  /*|./*|../*) ;;
  *) entry="./$entry" ;;
esac
case "$entry" in
  *.py) exec python3 "$entry" ;;
  *) chmod +x "$entry"; exec "$entry" ;;
esac
