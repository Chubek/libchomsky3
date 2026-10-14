#!/usr/bin/env bash
set -euo pipefail
self="$0"
marker="__PIKORL_ARCHIVE_BELOW__"
tmpdir="$(mktemp -d)"
cleanup() { rm -rf "$tmpdir"; }
trap cleanup EXIT
line="$(awk "/^${marker}$/ { print NR + 1; exit }" "$self")"
tail -n +"$line" "$self" > "$tmpdir/payload.tar"
tar -xf "$tmpdir/payload.tar" -C "$tmpdir"
entry=""
if [ -f "$tmpdir/entrypoint.sh" ]; then entry="$tmpdir/entrypoint.sh"; fi
if [ -f "$tmpdir/entrypoint.py" ]; then entry="$tmpdir/entrypoint.py"; fi
if [ -z "$entry" ]; then
  entry="$(find "$tmpdir" -maxdepth 2 -type f -name 'entrypoint*' | head -n 1 || true)"
fi
if [ -z "$entry" ]; then
  echo "No entrypoint found in package." >&2
  exit 1
fi
case "$entry" in
  *.py) exec python3 "$entry" ;;
  *) chmod +x "$entry"; exec "$entry" ;;
esac
__PIKORL_ARCHIVE_BELOW__
