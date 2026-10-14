#!/usr/bin/env python3
"""Generate SWIG bindings for PikoRL.

By default this script emits Python and Ruby wrappers into bindings/gen.
It can also emit C# wrappers with --lang csharp.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def run(cmd: list[str], cwd: Path) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd), check=True)


def swig_target(lang: str) -> tuple[list[str], str]:
    if lang == "python":
        return ["-python"], "PikoRL.py"
    if lang == "ruby":
        return ["-ruby"], "PikoRL.rb"
    if lang == "csharp":
        return ["-csharp"], "PikoRL.cs"
    raise ValueError(f"Unsupported language: {lang}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate SWIG bindings for PikoRL")
    parser.add_argument(
        "--lang",
        action="append",
        choices=["python", "ruby", "csharp"],
        help="Binding language to generate (default: python + ruby)",
    )
    parser.add_argument(
        "--out-dir",
        default="bindings/gen",
        help="Directory for generated wrapper files",
    )
    parser.add_argument(
        "--swig",
        default="swig",
        help="Path to the swig executable",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    interface_file = Path(__file__).resolve().parent / "PikoRL.i"
    out_dir = (repo_root / args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if shutil.which(args.swig) is None:
        print(f"error: swig executable not found: {args.swig}", file=sys.stderr)
        return 1

    languages = args.lang if args.lang else ["python", "ruby"]

    for lang in languages:
        lang_flags, generated_lang_file = swig_target(lang)
        cxx_out = out_dir / f"PikoRL_{lang}_wrap.cxx"
        run(
            [
                args.swig,
                "-c++",
                *lang_flags,
                "-I.",
                "-Ithird_party/QaMRpp",
                "-Ithird_party/SerdeTk",
                "-o",
                str(cxx_out),
                "-outdir",
                str(out_dir),
                str(interface_file),
            ],
            cwd=repo_root,
        )
        print(f"generated: {cxx_out}")
        print(f"generated: {out_dir / generated_lang_file}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
