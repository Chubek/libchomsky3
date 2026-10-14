#!/usr/bin/env python3
"""Generate validated SWIG bindings for PikoRL."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path


LANGUAGES = {
    "python": ("Python", ["-python"], "PikoRL.py"),
    "ruby": ("Ruby", ["-ruby"], "PikoRL.rb"),
    "csharp": ("C#", ["-csharp"], "PikoRL.cs"),
    "java": ("Java", ["-java"], "PikoRL.java"),
    "lua": ("Lua", ["-lua"], "PikoRL.lua"),
}


def run(cmd: list[str], cwd: Path) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=str(cwd), check=True)


def load_xfeatures(path: Path) -> dict[str, set[str]]:
    features: dict[str, set[str]] = {}
    feature_id: str | None = None
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        match = re.match(r'^\s*-\s+id:\s*"([^"]+)"\s*$', line)
        if match:
            feature_id = match.group(1)
            continue
        match = re.match(r"^\s*languages:\s*\[([^\]]*)\]\s*$", line)
        if match and feature_id:
            languages = {
                language.strip().strip('"')
                for language in match.group(1).split(",")
                if language.strip()
            }
            features[feature_id] = languages
            feature_id = None
        elif line.lstrip().startswith("- id:"):
            raise ValueError(f"{path}:{line_number}: malformed xfeature id")
    if not features:
        raise ValueError(f"{path}: no xfeatures found")
    return features


def requested_features(values: list[str] | None) -> list[str]:
    features: list[str] = []
    for value in values or []:
        features.extend(
            item.lstrip("+").strip()
            for item in value.split(",")
            if item.lstrip("+").strip()
        )
    return features


def validate_features(
    features: list[str], manifest: dict[str, set[str]], language: str
) -> None:
    language_name = LANGUAGES[language][0]
    for feature in features:
        if feature not in manifest:
            raise ValueError(f"unknown xfeature '{feature}'")
        if language_name not in manifest[feature]:
            supported = ", ".join(sorted(manifest[feature]))
            raise ValueError(
                f"xfeature '{feature}' is unsupported for {language_name}; "
                f"supported languages: {supported}"
            )


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate SWIG bindings for PikoRL")
    parser.add_argument(
        "--lang",
        action="append",
        choices=sorted(LANGUAGES),
        help="Binding language to generate (default: python + ruby)",
    )
    parser.add_argument(
        "--xfeats",
        default="bindings/XFeats.yaml",
        help="XFeats manifest used for feature validation",
    )
    parser.add_argument(
        "--enable-xfeats",
        action="append",
        metavar="FEATURE[,FEATURE...]",
        help="Enable validated xfeatures; a leading '+' is accepted",
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
    bindings_dir = Path(__file__).resolve().parent
    interface_file = bindings_dir / "PikoRL.i"
    manifest_path = (repo_root / args.xfeats).resolve()
    out_dir = (repo_root / args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    if shutil.which(args.swig) is None:
        print(f"error: swig executable not found: {args.swig}", file=sys.stderr)
        return 1

    languages = args.lang if args.lang else ["python", "ruby"]
    try:
        manifest = load_xfeatures(manifest_path)
        features = requested_features(args.enable_xfeats)
        for language in languages:
            validate_features(features, manifest, language)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    for lang in languages:
        _, lang_flags, generated_lang_file = LANGUAGES[lang]
        cxx_out = out_dir / f"PikoRL_{lang}_wrap.cxx"
        xfeature_defines = [
            f"-DPIKORL_XFEAT_{feature.upper().replace('-', '_')}"
            for feature in features
        ]
        run(
            [
                args.swig,
                "-c++",
                *lang_flags,
                *xfeature_defines,
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
