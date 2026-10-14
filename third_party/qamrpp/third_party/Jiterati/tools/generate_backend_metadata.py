#!/usr/bin/env python3
from pathlib import Path


def string_after(line: str, key: str) -> str:
    return line.split(key, 1)[1].split('"', 1)[0]


def dataset_stats(path: Path):
    arch = None
    source = None
    mnemonics = []
    instruction_count = 0
    for line in path.read_text().splitlines():
        stripped = line.strip()
        if stripped.startswith('arch = "'):
            arch = string_after(stripped, 'arch = "')
        elif stripped.startswith('source = "'):
            source = string_after(stripped, 'source = "')
        elif stripped.startswith('name = "'):
            instruction_count += 1
        elif stripped.startswith('mnemonic = "'):
            mnemonic = string_after(stripped, 'mnemonic = "')
            if mnemonic not in mnemonics:
                mnemonics.append(mnemonic)
    return arch, source, instruction_count, len(mnemonics)


def main():
    for target in ("amd64", "aarch64", "rv64", "wasm"):
        arch, source, instruction_count, mnemonic_count = dataset_stats(Path(f".agents/datasets/{target}.lua"))
        print(f'{{"{target}", "{arch}", "{source}", {instruction_count}, {mnemonic_count}}},')


if __name__ == "__main__":
    main()
