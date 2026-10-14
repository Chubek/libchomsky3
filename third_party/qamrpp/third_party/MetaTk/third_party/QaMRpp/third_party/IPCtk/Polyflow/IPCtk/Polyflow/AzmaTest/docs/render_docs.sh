#!/usr/bin/env sh
set -eu

if [ "$#" -lt 3 ]; then
  echo "usage: $0 <renderer> <out_dir> <chapter...>" >&2
  exit 2
fi

renderer="$1"
out_dir="$2"
shift 2

mkdir -p "$out_dir"

for src in "$@"; do
  base=$(basename "$src" .rst)
  out="$out_dir/$base.html"
  case "$renderer" in
    pandoc)
      pandoc "$src" -f rst -t html -o "$out"
      ;;
    marked)
      marked "$src" -o "$out"
      ;;
    markdown)
      markdown "$src" > "$out"
      ;;
    mdbook)
      echo "error: mdbook detected but per-file rendering is not supported by this target" >&2
      exit 1
      ;;
    *)
      echo "error: unsupported renderer '$renderer'" >&2
      exit 1
      ;;
  esac
  echo "rendered $out"
done
