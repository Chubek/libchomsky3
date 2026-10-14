#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DEFAULT_DOC_DIR="$ROOT_DIR/MiniZIP"
DEFAULT_BUILD_ROOT="$ROOT_DIR/build"

COLOR_RESET=$'\033[0m'
COLOR_BOLD=$'\033[1m'
COLOR_BLUE=$'\033[34m'
COLOR_CYAN=$'\033[36m'
COLOR_GREEN=$'\033[32m'
COLOR_YELLOW=$'\033[33m'
COLOR_RED=$'\033[31m'

if [[ -n "${NO_COLOR:-}" ]] || [[ ! -t 1 ]]; then
  COLOR_RESET=""
  COLOR_BOLD=""
  COLOR_BLUE=""
  COLOR_CYAN=""
  COLOR_GREEN=""
  COLOR_YELLOW=""
  COLOR_RED=""
fi

print_color() {
  local color="$1"
  local msg="$2"
  printf '%s%s%s\n' "$color" "$msg" "$COLOR_RESET"
}

usage() {
  cat <<'USAGE'
Usage: docs/build.sh [OPTIONS]

Build Sphinx documentation with a simple, colorful CLI.

Source/Output:
  -d, --doc DIR              documentation source directory (default: docs/MiniZIP)
  --build-dir DIR            custom build root (default: docs/build)

Format selection:
  --html                     build regular HTML output
  --singlehtml               build single-file HTML output
  --singehtml                alias of --singlehtml
  --latex                    build LaTeX artifacts
  --pdf                      build PDF from LaTeX (uses latex builder + pdflatex)
  --all                      build HTML, singlehtml and LaTeX (+PDF if possible)
  -f, --format FMT           one of html|singlehtml|latex|pdf|all

Build options:
  --out-dir DIR              override output dir for the selected format
  -j, --jobs N              Sphinx job count
  --clean                    remove existing output directory before build
  -v, --verbose              enable verbose sphinx output
  --fresh-env                ignore any cached environment in build dir
  --werror                   treat warnings as errors
  --open                     open built output when done

Helpers:
  -h, --help                 show this help
USAGE
}

log_step() {
  print_color "$COLOR_BOLD$COLOR_BLUE" "[*] $1"
}

log_ok() {
  print_color "$COLOR_GREEN" "[OK] $1"
}

log_warn() {
  print_color "$COLOR_YELLOW" "[WARN] $1"
}

log_err() {
  print_color "$COLOR_RED" "[ERR] $1" >&2
}

require_sphinx() {
  if ! command -v sphinx-build >/dev/null 2>&1; then
    log_err "sphinx-build not found. Install Sphinx first."
    exit 1
  fi
}

run_sphinx() {
  local builder="$1"
  local source="$2"
  local outdir="$3"
  local -a args=(-b "$builder" "$source" "$outdir")

  [[ "$VERBOSE" == "1" ]] && args+=(-v)
  [[ "$FRESH_ENV" == "1" ]] && args+=(--fresh-env)
  [[ "$WERROR" == "1" ]] && args+=(-W)
  [[ "$JOBS" != "1" ]] && args+=(-j "$JOBS")

  sphinx-build "${args[@]}"
}

build_html=false
build_single=false
build_latex=false
build_pdf=false
DOC_DIR="$DEFAULT_DOC_DIR"
BUILD_DIR="$DEFAULT_BUILD_ROOT"
OVERRIDE_OUT_DIR=""
VERBOSE=0
FRESH_ENV=0
JOBS=1
WERROR=0
CLEAN=0
OPEN_OUTPUT=0

if [[ $# -eq 0 ]]; then
  build_html=true
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    -d|--doc)
      shift
      if [[ $# -eq 0 ]]; then
        log_err "Missing argument for --doc"
        exit 1
      fi
      DOC_DIR="${1:-}"
      ;;
    --build-dir)
      shift
      if [[ $# -eq 0 ]]; then
        log_err "Missing argument for --build-dir"
        exit 1
      fi
      BUILD_DIR="${1:-}"
      ;;
    --html)
      build_html=true build_single=false build_latex=false build_pdf=false
      ;;
    --singlehtml|--single-html|--singehtml)
      build_single=true build_html=false build_latex=false build_pdf=false
      ;;
    --latex)
      build_latex=true build_html=false build_single=false build_pdf=false
      ;;
    --pdf)
      build_pdf=true
      ;;
    --all)
      build_html=true
      build_single=true
      build_latex=true
      ;;
    -f|--format)
      shift
      if [[ $# -eq 0 ]]; then
        log_err "Missing argument for --format"
        exit 1
      fi
      case "${1:-}" in
        html) build_html=true build_single=false build_latex=false build_pdf=false ;;
        singlehtml) build_single=true build_html=false build_latex=false build_pdf=false ;;
        latex) build_latex=true build_html=false build_single=false build_pdf=false ;;
        pdf) build_pdf=true ;;
        all) build_html=true; build_single=true; build_latex=true ;;
        *) log_err "Invalid format: ${1:-}"; exit 1 ;;
      esac
      ;;
    --out-dir)
      shift
      if [[ $# -eq 0 ]]; then
        log_err "Missing argument for --out-dir"
        exit 1
      fi
      OVERRIDE_OUT_DIR="${1:-}"
      ;;
    -j|--jobs)
      shift
      if [[ $# -eq 0 ]]; then
        log_err "Missing argument for --jobs"
        exit 1
      fi
      JOBS="${1:-1}"
      ;;
    --clean)
      CLEAN=1
      ;;
    -v|--verbose)
      VERBOSE=1
      ;;
    --fresh-env)
      FRESH_ENV=1
      ;;
    --werror)
      WERROR=1
      ;;
    --open)
      OPEN_OUTPUT=1
      ;;
    *)
      log_err "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
  shift
done

DOC_DIR="${DOC_DIR%/}"
BUILD_DIR="${BUILD_DIR%/}"
if [[ ! -d "$DOC_DIR" ]]; then
  log_err "Documentation source directory not found: $DOC_DIR"
  exit 1
fi
if [[ ! -f "$DOC_DIR/conf.py" ]]; then
  log_err "No conf.py in source directory: $DOC_DIR"
  exit 1
fi
if [[ ! -f "$DOC_DIR/index.rst" ]]; then
  log_err "No index.rst in source directory: $DOC_DIR"
  exit 1
fi

require_sphinx
mkdir -p "$BUILD_DIR"

do_build() {
  local builder="$1"
  local out="$2"
  local source="$DOC_DIR"
  [[ -n "$OVERRIDE_OUT_DIR" ]] && out="$OVERRIDE_OUT_DIR"

  if [[ "$CLEAN" == "1" ]]; then
    rm -rf "$out"
  fi
  mkdir -p "$out"

  log_step "Running: sphinx-build -b $builder $source -> $out"
  run_sphinx "$builder" "$source" "$out"
  log_ok "Built: $out"
}

if [[ "$build_pdf" == "true" ]]; then
  # PDF requires LaTeX artifacts as an intermediate.
  build_latex=true
fi

if [[ "$build_html" == "true" ]]; then
  target="$BUILD_DIR/html"
  [[ -n "$OVERRIDE_OUT_DIR" ]] && target="$OVERRIDE_OUT_DIR"
  do_build html "$target"
  if [[ "$OPEN_OUTPUT" == "1" ]]; then
    if [[ -f "$target/index.html" ]]; then
      log_step "Opening $target/index.html"
      (xdg-open "$target/index.html" >/dev/null 2>&1 || true)
    fi
  fi
fi

if [[ "$build_single" == "true" ]]; then
  target="$BUILD_DIR/singlehtml"
  [[ -n "$OVERRIDE_OUT_DIR" ]] && target="$OVERRIDE_OUT_DIR"
  do_build singlehtml "$target"
  if [[ "$OPEN_OUTPUT" == "1" ]]; then
    if [[ -f "$target/index.html" ]]; then
      log_step "Opening $target/index.html"
      (xdg-open "$target/index.html" >/dev/null 2>&1 || true)
    fi
  fi
fi

if [[ "$build_latex" == "true" ]]; then
  target="$BUILD_DIR/latex"
  [[ -n "$OVERRIDE_OUT_DIR" ]] && target="$OVERRIDE_OUT_DIR"
  do_build latex "$target"

  if [[ "$build_pdf" == "true" ]]; then
    pdf_target="$BUILD_DIR/pdf"
    mkdir -p "$pdf_target"
    if ! command -v pdflatex >/dev/null 2>&1; then
      log_warn "pdflatex not found; skipping PDF emission."
    else
      pdf_out="$BUILD_DIR/pdf"
      [[ -n "$OVERRIDE_OUT_DIR" ]] && pdf_out="$OVERRIDE_OUT_DIR"
      if [[ -n "$OVERRIDE_OUT_DIR" ]] && [[ "$OVERRIDE_OUT_DIR" != "$pdf_target" ]]; then
        pdf_out_final="$OVERRIDE_OUT_DIR"
      else
        pdf_out_final="$pdf_out"
      fi
      log_step "Building PDF in $pdf_out_final"
      run_sphinx latexpdf "$DOC_DIR" "$pdf_out_final"
      for f in "$pdf_out_final"/*.pdf; do
        cp "$f" "$pdf_target/"
      done
      if [[ "$OPEN_OUTPUT" == "1" ]] && compgen -G "$pdf_target/*.pdf" >/dev/null; then
        pdf_file="$(find "$pdf_target" -maxdepth 1 -name '*.pdf' -print -quit)"
        log_step "Opening $pdf_file"
        (xdg-open "$pdf_file" >/dev/null 2>&1 || true)
      fi
      log_ok "PDFs in $pdf_target"
    fi
  fi

  if [[ "$OPEN_OUTPUT" == "1" && "$build_pdf" != "true" ]]; then
    tex_file="$(find "$target" -maxdepth 1 -name '*.tex' -print -quit || true)"
    [[ -n "$tex_file" ]] && log_step "LaTeX sources in $target"
  fi
fi

if [[ "$build_html" == "false" && "$build_single" == "false" && "$build_latex" == "false" && "$build_pdf" == "false" ]]; then
  log_err "No build target selected."
  exit 1
fi

log_ok "Done."
