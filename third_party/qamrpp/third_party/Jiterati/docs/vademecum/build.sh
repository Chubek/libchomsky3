#!/usr/bin/env bash
# build.sh — lint-then-build wrapper for "Jiterati: A Vademecum".
#
# Runs the required chktex step first, then builds the PDF with latexmk.
# Exits non-zero if chktex reports errors or if the build fails.
#
# Usage:
#   ./build.sh            chktex + build
#   ./build.sh check      chktex only
#   ./build.sh build      build only (skip chktex)
#   ./build.sh clean      remove aux files
#   ./build.sh distclean  remove build/ entirely

set -euo pipefail

cd "$(dirname "$0")"

DOC="Jiterati-Vademcum.tex"

run_chktex() {
    echo "==> chktex ${DOC}"
    # Per the project requirement: `chktex Jiterati-Vademcum.tex`.
    # The local .chktexrc supplies resource paths and warning policy.
    if ! chktex -q "${DOC}"; then
        status=$?
        # chktex exit code is a bitmask: bit 0 = errors, bit 1 = warnings.
        if [ $(( status & 1 )) -ne 0 ]; then
            echo "chktex reported errors (exit ${status})." >&2
            exit 1
        fi
        echo "chktex reported warnings (exit ${status})."
    else
        echo "chktex: clean"
    fi
}

run_build() {
    echo "==> latexmk ${DOC}"
    # Engine + paths come from .latexmkrc in this directory.
    latexmk "${DOC}"
}

case "${1:-all}" in
    all)
        run_chktex
        run_build
        ;;
    check)
        run_chktex
        ;;
    build)
        run_build
        ;;
    clean)
        latexmk -c
        ;;
    distclean)
        latexmk -C
        rm -rf build
        ;;
    *)
        echo "usage: $0 [all|check|build|clean|distclean]" >&2
        exit 2
        ;;
esac
