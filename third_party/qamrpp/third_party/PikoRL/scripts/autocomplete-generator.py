#!/usr/bin/env python3
"""Generate a deterministic keyword completion asset."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", default="default")
    parser.add_argument("--keyword", action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    keywords = sorted(set(args.keyword or ["help", "load", "quit", "set"]))
    values = ", ".join(f'"{keyword}"' for keyword in keywords)
    content = (
        "return {\n"
        f'  name = "{args.name}",\n'
        f"  keywords = {{{values}}}\n"
        "}\n"
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(content, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
