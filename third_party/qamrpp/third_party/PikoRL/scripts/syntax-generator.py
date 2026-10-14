#!/usr/bin/env python3
"""Generate a deterministic plain syntax asset."""

from __future__ import annotations

import argparse
from pathlib import Path


TEMPLATE = """return {{
  name = "{name}",
  theme = {{
    comment = "bright-black",
    string = "green",
    number = "magenta",
    operator = "white",
    identifier = "default"
  }},
  patterns = {{
    comment = "#.*$",
    string = "\\\"([^\\\"\\\\\\\\]|\\\\\\\\.)*\\\"|'([^'\\\\\\\\]|\\\\\\\\.)*'",
    number = "%f[%d]%d+%.?%d*%f[^%d]",
    operator = "[+%-%*/=<>!&|%^~]+",
    identifier = "[_%a][_%w]*"
  }}
}}
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", default="plain")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(TEMPLATE.format(name=args.name), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
