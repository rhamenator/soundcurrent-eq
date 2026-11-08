#!/usr/bin/env python3
"""Verify that cable/native installers came from one app payload and source revision."""
from __future__ import annotations

import argparse
import json
import pathlib
import re


def load(path: pathlib.Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    required = {"schema", "product", "version", "audioRoute", "sourceCommit", "installer", "installerSha256", "commonPayload"}
    if set(value) != required:
        raise SystemExit(f"{path}: manifest keys differ from the closed schema")
    if value["schema"] != 1 or not re.fullmatch(r"[0-9a-f]{40}", value["sourceCommit"]):
        raise SystemExit(f"{path}: invalid schema or source commit")
    if not re.fullmatch(r"[0-9a-f]{64}", value["installerSha256"]):
        raise SystemExit(f"{path}: invalid installer hash")
    if not isinstance(value["commonPayload"], dict) or not value["commonPayload"]:
        raise SystemExit(f"{path}: common payload is empty")
    for name, digest in value["commonPayload"].items():
        if pathlib.PurePosixPath(name).is_absolute() or ".." in pathlib.PurePosixPath(name).parts:
            raise SystemExit(f"{path}: unsafe payload path {name!r}")
        if not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise SystemExit(f"{path}: invalid payload hash for {name}")
    return value


def verify(cable: dict, native: dict) -> dict:
    if cable["audioRoute"] != "Cable" or native["audioRoute"] != "Native":
        raise SystemExit("expected one Cable and one Native manifest")
    for field in ("product", "version", "sourceCommit", "commonPayload"):
        if cable[field] != native[field]:
            raise SystemExit(f"variant parity failed: {field} differs")
    if cable["installer"] == native["installer"]:
        raise SystemExit("variant parity failed: installer filenames collide")
    return {
        "schema": 1, "product": cable["product"], "version": cable["version"],
        "sourceCommit": cable["sourceCommit"],
        "cable": {"installer": cable["installer"], "sha256": cable["installerSha256"]},
        "native": {"installer": native["installer"], "sha256": native["installerSha256"]},
        "commonPayloadFiles": len(cable["commonPayload"]),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cable", type=pathlib.Path, required=True)
    parser.add_argument("--native", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()
    rendered = json.dumps(verify(load(args.cable), load(args.native)), indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")


if __name__ == "__main__":
    main()
