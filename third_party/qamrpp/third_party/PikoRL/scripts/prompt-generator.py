#!/usr/bin/env python3
"""Generate a deterministic prompt asset."""

from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", default="default")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    content = (
        "return {\n"
        f'  name = "{args.name}",\n'
        '  render = function(state)\n'
        '    return (state and state.mode or "pikorl") .. "> "\n'
        "  end\n"
        "}\n"
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(content, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
