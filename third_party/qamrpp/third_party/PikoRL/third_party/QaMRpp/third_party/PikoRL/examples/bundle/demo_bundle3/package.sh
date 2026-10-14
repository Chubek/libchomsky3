#!/usr/bin/env bash
set -euo pipefail
bundle_dir="$(cd "$(dirname "$0")" && pwd)"
archive_name="${1:-bundle.pikorl}"
stub_file="${PIKORL_STUB:-$bundle_dir/.pikorl_stub.sh}"
makeself_bin="${PIKORL_MAKESELF_BIN:-$bundle_dir/../third_party/makeself/makeself.sh}"
manifest_file="${PIKORL_MANIFEST:-$bundle_dir/MANIFEST.json}"

if [ ! -f "$stub_file" ]; then
  echo "Missing stub file: $stub_file" >&2
  exit 1
fi
if [ ! -f "$manifest_file" ]; then
  echo "Missing manifest: $manifest_file" >&2
  exit 1
fi

if [ ! -x "$makeself_bin" ]; then
  echo "Missing makeself: $makeself_bin" >&2
  exit 1
fi

tmp_stage="$(mktemp -d)"
tmp_trail="$(mktemp -d)"
trap 'rm -rf "$tmp_stage" "$tmp_trail"' EXIT

# Build staging set from MANIFEST + .bundler (keep/remove glob lines).
python3 - "$bundle_dir" "$manifest_file" "$tmp_stage" <<'PY'
import fnmatch
import json
import os
import pathlib
import shutil
import sys

bundle = pathlib.Path(sys.argv[1]).resolve()
manifest_path = pathlib.Path(sys.argv[2]).resolve()
stage = pathlib.Path(sys.argv[3]).resolve()
manifest = json.loads(manifest_path.read_text())
stage.mkdir(parents=True, exist_ok=True)

includes = manifest.get("bundle", {}).get("include", ["**"])
excludes = set(manifest.get("bundle", {}).get("exclude", []))

bundler = bundle / ".bundler"
if bundler.exists():
    for raw in bundler.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("keep "):
            includes.append(line[5:].strip())
        elif line.startswith("remove "):
            excludes.add(line[7:].strip())
        else:
            excludes.add(line)

def match_any(path, patterns):
    return any(fnmatch.fnmatch(path, p) for p in patterns)

for entry in bundle.rglob("*"):
    if entry == stage:
        continue
    rel = entry.relative_to(bundle).as_posix()
    if rel.startswith(".git/"):
        continue
    if rel == archive_name if False else False:
        continue
    if not match_any(rel, includes):
        continue
    if match_any(rel, excludes):
        continue
    target = stage / rel
    if entry.is_dir():
        target.mkdir(parents=True, exist_ok=True)
    elif entry.is_file():
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(entry, target)
PY

# Run manifest build commands while creating executable.
python3 - "$manifest_file" "$bundle_dir" <<'PY'
import json, pathlib, subprocess, sys
manifest = json.loads(pathlib.Path(sys.argv[1]).read_text())
cwd = pathlib.Path(sys.argv[2])
for cmd in manifest.get("commands", []):
    if isinstance(cmd, str) and cmd.strip():
        subprocess.run(cmd, shell=True, check=True, cwd=cwd)
PY

# Manifest stub override: stub.inline or stub.file.
python3 - "$manifest_file" "$bundle_dir" "$stub_file" <<'PY'
import json, pathlib, shutil, sys
manifest = json.loads(pathlib.Path(sys.argv[1]).read_text())
bundle = pathlib.Path(sys.argv[2])
stub_out = pathlib.Path(sys.argv[3])
stub = manifest.get("stub", {})
inline = stub.get("inline")
stub_file = stub.get("file")
if isinstance(inline, str) and inline.strip():
    stub_out.write_text(inline)
elif isinstance(stub_file, str) and stub_file.strip():
    src = (bundle / stub_file).resolve()
    shutil.copy2(src, stub_out)
PY

"$makeself_bin" --nocomp --header "$stub_file" \
  "$tmp_stage" "$bundle_dir/$archive_name" \
  "PikoRL Bundle: $(basename "$bundle_dir")" "./.pikorl_run_entrypoint.sh"

# Optional trail section represented as extracted files under .trail/.
python3 - "$manifest_file" "$bundle_dir" "$tmp_trail/index.json" "$tmp_trail/data" <<'PY'
import json, pathlib, random, sys
manifest = json.loads(pathlib.Path(sys.argv[1]).read_text())
bundle = pathlib.Path(sys.argv[2])
index_path = pathlib.Path(sys.argv[3])
data_dir = pathlib.Path(sys.argv[4])
trail = manifest.get("trail", {})
if not trail.get("enabled", False):
    index_path.write_text(json.dumps({"enabled": False}))
    raise SystemExit(0)
data_dir.mkdir(parents=True, exist_ok=True)
items = []
for n, item in enumerate(trail.get("files", []), start=1):
    if isinstance(item, str):
        src = item
        name = pathlib.Path(item).name
    else:
        src = item.get("source", "")
        name = item.get("name", pathlib.Path(src).name)
    p = (bundle / src).resolve()
    if p.exists() and p.is_file():
        sentinel = str(random.randint(10**12, 10**15 - 1))
        out_name = f"{n:06d}_{name}"
        (data_dir / out_name).write_bytes(p.read_bytes())
        items.append({"name": name, "source": src, "sentinel": sentinel, "stored_as": out_name})
index_path.write_text(json.dumps({"enabled": True, "files": items}, indent=2))
PY

if [ -f "$tmp_trail/index.json" ]; then
  mkdir -p "$tmp_stage/.trail"
  cp "$tmp_trail/index.json" "$tmp_stage/.trail/index.json"
  if [ -d "$tmp_trail/data" ]; then
    cp -r "$tmp_trail/data" "$tmp_stage/.trail/data"
  fi
fi

echo "Created runnable archive: $bundle_dir/$archive_name"
