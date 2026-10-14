#!/usr/bin/env python3
import argparse
import pathlib
import subprocess
import sys


def run(cmd):
    print("+", " ".join(cmd))
    subprocess.run(cmd, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate IPCtk SWIG bindings")
    parser.add_argument("--swig", default="swig", help="Path to swig executable")
    parser.add_argument("--lang", choices=["python", "ruby"], required=True)
    parser.add_argument("--out-dir", default="bindings/generated", help="Output directory")
    parser.add_argument("--interface", default="bindings/IPCtk.i", help="SWIG interface file")
    args = parser.parse_args()

    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    lang_flag = "-python" if args.lang == "python" else "-ruby"
    wrapper = out_dir / f"IPCtk_{args.lang}_wrap.cxx"

    cmd = [
        args.swig,
        "-c++",
        lang_flag,
        "-I.",
        "-o",
        str(wrapper),
        args.interface,
    ]

    run(cmd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
