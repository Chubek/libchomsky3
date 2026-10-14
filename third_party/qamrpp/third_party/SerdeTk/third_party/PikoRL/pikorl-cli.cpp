#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void print_help(std::ostream& os) {
  os
    << "pikorl-cli - scaffold and package PikoRL runnable bundles\n\n"
    << "USAGE\n"
    << "  pikorl-cli [--stub FILE] <bundle-dir>\n"
    << "  pikorl-cli --help\n"
    << "  pikorl-cli -h\n"
    << "  pikorl-cli -\n\n"
    << "DESCRIPTION\n"
    << "  Creates a new bundle directory with Lua feature files, MANIFEST.json,\n"
    << "  package.sh, and entrypoint scripts. package.sh builds a self-extracting\n"
    << "  runnable archive using third_party/makeself/makeself.sh.\n\n"
    << "SCAFFOLD OUTPUT\n"
    << "  colors.lua\n"
    << "  history.lua\n"
    << "  macros.lua\n"
    << "  MANIFEST.json\n"
    << "  syntax.lua\n"
    << "  theme.lua\n"
    << "  web.lua\n"
    << "  package.sh\n"
    << "  entrypoint.sh\n"
    << "  .pikorl_run_entrypoint.sh\n"
    << "  .pikorl_stub.sh\n"
    << "  ddrv\n"
    << "  code/\n"
    << "  static/\n\n"
    << "OPTIONS\n"
    << "  --stub FILE\n"
    << "      Use FILE as the makeself header stub template for this bundle.\n"
    << "      Default: third_party/makeself/makeself-header.sh\n"
    << "  --help, -h, -\n"
    << "      Show this help and exit.\n\n"
    << "PACKAGING\n"
    << "  Run ./package.sh [archive-name] from inside a generated bundle.\n"
    << "  Default archive name: bundle.pikorl\n\n"
    << "  package.sh behavior:\n"
    << "    1) Reads MANIFEST.json.\n"
    << "    2) Stages files using MANIFEST bundle include/exclude globs.\n"
    << "    3) Applies .bundler rules (keep/remove glob patterns).\n"
    << "    4) Executes MANIFEST commands during build.\n"
    << "    5) Applies MANIFEST stub override (inline or file).\n"
    << "    6) Creates a runnable makeself archive.\n"
    << "    7) Writes trail metadata/files under .trail/ in extracted bundle.\n\n"
    << "MANIFEST FIELDS (generated template)\n"
    << "  entrypoint: string\n"
    << "      Preferred runtime program/script to execute after extraction.\n"
    << "  theme.mode: string\n"
    << "      Theme mode hint (for bundle metadata).\n"
    << "  theme.inline: string\n"
    << "      Theme source hint (for bundle metadata).\n"
    << "  bundle.include: [glob]\n"
    << "      Include patterns; default [\"**\"].\n"
    << "  bundle.exclude: [glob]\n"
    << "      Exclude patterns.\n"
    << "  stub.inline: string|null\n"
    << "      Inline makeself header content to use as stub.\n"
    << "  stub.file: string|null\n"
    << "      Relative path to stub file in bundle.\n"
    << "  commands: [string]\n"
    << "      Shell commands run while creating archive.\n"
    << "  trail.enabled: bool\n"
    << "      Enable trail metadata/data generation.\n"
    << "  trail.files: [string|object]\n"
    << "      Trail files to capture into .trail/data with JSON index.\n\n"
    << "ENVIRONMENT FOR package.sh\n"
    << "  PIKORL_STUB         Override stub path (default ./.pikorl_stub.sh)\n"
    << "  PIKORL_MAKESELF_BIN Override makeself path\n"
    << "  PIKORL_MANIFEST     Override manifest path\n\n"
    << "RUNTIME ENTRYPOINT RESOLUTION\n"
    << "  1) MANIFEST entrypoint (if exists)\n"
    << "  2) ./entrypoint.sh\n"
    << "  3) ./entrypoint.py\n"
    << "  4) first file matching entrypoint*\n\n"
    << "DATA DRIVER (ddrv)\n"
    << "  ddrv get line N FILE\n"
    << "  ddrv grep [grep-args]\n"
    << "  ddrv fuzzydiff FILE1 FILE2\n"
    << "  ddrv trail-index\n";
}

static std::string default_manifest() {
  return R"({
  "entrypoint": "entrypoint.sh",
  "theme": {
    "mode": "inline",
    "inline": "theme.lua"
  },
  "bundle": {
    "include": ["**"],
    "exclude": []
  },
  "stub": {
    "inline": null,
    "file": null
  },
  "commands": [],
  "trail": {
    "enabled": false,
    "files": []
  },
  "features": [
    { "name": "colors",  "path": "colors.lua",  "required": false },
    { "name": "history", "path": "history.lua", "required": false },
    { "name": "macros",  "path": "macros.lua",  "required": false },
    { "name": "syntax",  "path": "syntax.lua",  "required": false },
    { "name": "theme",   "path": "theme.lua",   "required": false },
    { "name": "web",     "path": "web.lua",     "required": false }
  ]
}
)";
}

static std::string default_package_script() {
  return R"PIK(#!/usr/bin/env bash
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
)PIK";
}

static void write_file(const fs::path& path, const std::string& content) {
  std::ofstream out(path, std::ios::binary);
  out << content;
}

int main(int argc, char** argv) {
  std::string stub_path;
  std::vector<std::string> positional;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help" || arg == "-h" || arg == "-") {
      print_help(std::cout);
      return 0;
    }
    if (arg == "--stub") {
      if (i + 1 >= argc) {
        std::cerr << "--stub requires a file path\n";
        return 1;
      }
      stub_path = argv[++i];
      continue;
    }
    positional.push_back(arg);
  }

  if (positional.size() != 1) {
    std::cerr << "Usage: pikorl-cli [--stub FILE] <bundle-dir>\n"
              << "Try: pikorl-cli --help\n";
    return 1;
  }

  fs::path bundle_dir = positional[0];
  if (fs::exists(bundle_dir)) {
    std::cerr << "Target already exists: " << bundle_dir << "\n";
    return 1;
  }

  fs::create_directories(bundle_dir / "code");
  fs::create_directories(bundle_dir / "static");

  write_file(bundle_dir / "colors.lua", "-- colors feature hook\nreturn {}\n");
  write_file(bundle_dir / "history.lua", "-- history feature hook\nreturn {}\n");
  write_file(bundle_dir / "macros.lua", "-- macros feature hook\nreturn {}\n");
  write_file(bundle_dir / "syntax.lua", "-- syntax feature hook\nreturn {}\n");
  write_file(bundle_dir / "theme.lua", "-- theme feature hook\nreturn {}\n");
  write_file(bundle_dir / "web.lua", "-- web feature hook\nreturn {}\n");
  write_file(bundle_dir / "entrypoint.sh", "#!/usr/bin/env bash\necho \"PikoRL bundle entrypoint\"\n");
  write_file(bundle_dir / ".pikorl_run_entrypoint.sh",
             "#!/usr/bin/env bash\n"
             "set -euo pipefail\n"
             "entry=''\n"
             "if [ -f ./MANIFEST.json ]; then\n"
             "  entry=\"$(python3 - <<'PY'\n"
             "import json, pathlib\n"
             "m = json.loads(pathlib.Path('MANIFEST.json').read_text())\n"
             "print(m.get('entrypoint', ''))\n"
             "PY\n"
             ")\"\n"
             "fi\n"
             "if [ -n \"$entry\" ] && [ ! -f \"$entry\" ]; then entry=''; fi\n"
             "if [ -z \"$entry\" ] && [ -f ./entrypoint.sh ]; then entry=./entrypoint.sh; fi\n"
             "if [ -z \"$entry\" ] && [ -f ./entrypoint.py ]; then entry=./entrypoint.py; fi\n"
             "if [ -z \"$entry\" ]; then\n"
             "  entry=\"$(find . -maxdepth 2 -type f -name 'entrypoint*' | head -n 1 || true)\"\n"
             "fi\n"
             "if [ -z \"$entry\" ]; then\n"
             "  echo \"No entrypoint found in package.\" >&2\n"
             "  exit 1\n"
             "fi\n"
             "case \"$entry\" in\n"
             "  /*|./*|../*) ;;\n"
             "  *) entry=\"./$entry\" ;;\n"
             "esac\n"
             "case \"$entry\" in\n"
             "  *.py) exec python3 \"$entry\" ;;\n"
             "  *) chmod +x \"$entry\"; exec \"$entry\" ;;\n"
             "esac\n");
  write_file(bundle_dir / "ddrv",
             "#!/usr/bin/env bash\n"
             "set -euo pipefail\n"
             "self=\"${DDRV_ARCHIVE:-$0}\"\n"
             "cmd=\"${1:-}\"; shift || true\n"
             "extract_trail() {\n"
             "  if [ -f ./.trail/index.json ]; then cat ./.trail/index.json; fi\n"
             "}\n"
             "case \"$cmd\" in\n"
             "  get)\n"
             "    mode=\"${1:-}\"; shift || true\n"
             "    if [ \"$mode\" = \"line\" ]; then\n"
             "      n=\"${1:-1}\"; f=\"${2:-}\"; sed -n \"${n}p\" \"$f\"\n"
             "    fi\n"
             "    ;;\n"
             "  grep)\n"
             "    grep \"$@\"\n"
             "    ;;\n"
             "  fuzzydiff)\n"
             "    diff -u \"$1\" \"$2\" || true\n"
             "    ;;\n"
             "  trail-index)\n"
             "    extract_trail\n"
             "    ;;\n"
             "  *)\n"
             "    echo \"Usage: ddrv {get line N FILE|grep ...|fuzzydiff A B|trail-index}\" >&2\n"
             "    exit 1\n"
             "    ;;\n"
             "esac\n");
  write_file(bundle_dir / "MANIFEST.json", default_manifest());
  if (stub_path.empty()) {
    fs::copy_file("third_party/makeself/makeself-header.sh", bundle_dir / ".pikorl_stub.sh");
  } else {
    write_file(bundle_dir / ".pikorl_stub.sh", "");
  }
  if (!stub_path.empty()) {
    std::ifstream in(stub_path, std::ios::binary);
    if (!in) {
      std::cerr << "Could not read stub: " << stub_path << "\n";
      return 1;
    }
    std::ofstream out(bundle_dir / ".pikorl_stub.sh", std::ios::binary);
    out << in.rdbuf();
  }
  write_file(bundle_dir / "package.sh", default_package_script());

  fs::permissions(bundle_dir / "entrypoint.sh",
                  fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                  fs::perm_options::add);
  fs::permissions(bundle_dir / ".pikorl_run_entrypoint.sh",
                  fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                  fs::perm_options::add);
  fs::permissions(bundle_dir / "ddrv",
                  fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                  fs::perm_options::add);
  fs::permissions(bundle_dir / "package.sh",
                  fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                  fs::perm_options::add);
  fs::permissions(bundle_dir / ".pikorl_stub.sh",
                  fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                  fs::perm_options::add);

  std::cout << "Created bundle scaffold: " << bundle_dir << "\n";
  return 0;
}
