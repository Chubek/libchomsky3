#!/usr/bin/env bash
set -euo pipefail
bundle_dir="$(cd "$(dirname "$0")" && pwd)"
archive_name="${1:-bundle.pikorl}"
stub_file="${PIKORL_STUB:-$bundle_dir/.pikorl_stub.sh}"
makeself_bin="${PIKORL_MAKESELF_BIN:-$bundle_dir/../third_party/makeself/makeself.sh}"

if [ ! -f "$stub_file" ]; then
  echo "Missing stub file: $stub_file" >&2
  exit 1
fi

if [ ! -x "$makeself_bin" ]; then
  echo "Missing makeself: $makeself_bin" >&2
  exit 1
fi

"$makeself_bin" --nocomp --header "$stub_file" \
  "$bundle_dir" "$bundle_dir/$archive_name" \
  "PikoRL Bundle: $(basename "$bundle_dir")" "./.pikorl_run_entrypoint.sh"
echo "Created runnable archive: $bundle_dir/$archive_name"
