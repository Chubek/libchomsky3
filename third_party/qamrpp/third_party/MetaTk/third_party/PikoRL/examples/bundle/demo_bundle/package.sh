#!/usr/bin/env bash
set -euo pipefail
bundle_dir="$(cd "$(dirname "$0")" && pwd)"
archive_name="${1:-bundle.pikorl}"
stub_file="${PIKORL_STUB:-$bundle_dir/.pikorl_stub.sh}"

if [ ! -f "$stub_file" ]; then
  echo "Missing stub file: $stub_file" >&2
  exit 1
fi

tar_bin="${PIKORL_TAR_BIN:-$bundle_dir/../third_party/libarchive/tar/bsdtar}"
if [ ! -x "$tar_bin" ]; then
  tar_bin="tar"
fi

tmp_tar="$(mktemp)"
trap 'rm -f "$tmp_tar"' EXIT

cd "$bundle_dir"
"$tar_bin" -cf "$tmp_tar" \
  colors.lua history.lua macros.lua MANIFEST.json syntax.lua theme.lua web.lua \
  entrypoint.sh code static
cat "$stub_file" "$tmp_tar" > "$archive_name"
chmod +x "$archive_name"
echo "Created runnable archive: $bundle_dir/$archive_name"
