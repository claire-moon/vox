#!/usr/bin/env python3
"""Generate the minimal SPDX 2.3 JSON SBOM shipped with VOX + DIGS."""

# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
from typing import Dict, List


PROJECT_LICENSE = "GPL-3.0-or-later"
SDL_LICENSE = "Zlib"


def digest(path: Path, algorithm: str) -> str:
    value = hashlib.new(algorithm)
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def file_types(relative: str) -> List[str]:
    suffix = Path(relative).suffix.lower()
    if relative.startswith("bin/"):
        return ["BINARY", "APPLICATION"]
    if suffix in {".sh", ".py"}:
        return ["SOURCE"]
    if suffix in {".md", ".txt", ".csv"}:
        return ["TEXT"]
    if suffix in {".xlsx", ".gz"}:
        return ["ARCHIVE"]
    if suffix in {".ppm", ".png", ".jpg", ".jpeg"}:
        return ["IMAGE"]
    return ["BINARY"]


def make_file(root: Path, path: Path) -> Dict[str, object]:
    relative = path.relative_to(root).as_posix()
    identifier = hashlib.sha1(relative.encode("utf-8")).hexdigest()[:20]
    return {
        "fileName": "./" + relative,
        "SPDXID": "SPDXRef-File-" + identifier,
        "checksums": [
            {"algorithm": "SHA1", "checksumValue": digest(path, "sha1")},
            {"algorithm": "SHA256", "checksumValue": digest(path, "sha256")},
        ],
        "fileTypes": file_types(relative),
        "licenseConcluded": PROJECT_LICENSE,
        "licenseInfoInFiles": [PROJECT_LICENSE],
        "copyrightText": "NOASSERTION",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--epoch", required=True, type=int)
    parser.add_argument("--document-name")
    parser.add_argument(
        "--purpose",
        choices=("APPLICATION", "SOURCE"),
        default="APPLICATION",
    )
    parser.add_argument("--namespace-label", default="binary")
    args = parser.parse_args()

    root = args.root.resolve()
    output = args.output.resolve()
    if not root.is_dir():
        parser.error("--root must name the staged bundle directory")
    try:
        output.relative_to(root)
    except ValueError:
        parser.error("--output must be inside --root")
    if args.epoch < 0:
        parser.error("--epoch must be non-negative")

    paths = sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and path.resolve() != output
    )
    files = [make_file(root, path) for path in paths]
    verification_input = "".join(
        sorted(
            checksum["checksumValue"]
            for item in files
            for checksum in item["checksums"]
            if checksum["algorithm"] == "SHA1"
        )
    )
    verification_code = hashlib.sha1(
        verification_input.encode("ascii")
    ).hexdigest()
    created = datetime.datetime.fromtimestamp(
        args.epoch, datetime.timezone.utc
    ).strftime("%Y-%m-%dT%H:%M:%SZ")
    safe_version = "".join(
        character if character.isalnum() or character in ".-" else "-"
        for character in args.version
    )
    safe_label = "".join(
        character if character.isalnum() or character in ".-" else "-"
        for character in args.namespace_label
    )
    document_name = args.document_name or (
        f"VOX + DIGS {args.version} Linux x86_64 binary bundle"
    )
    distribution_kind = (
        "source archive" if args.purpose == "SOURCE" else "binary bundle"
    )

    project_id = "SPDXRef-Package-VOX-DIGS"
    sdl_id = "SPDXRef-Package-SDL2-System"
    document = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": document_name,
        "documentNamespace": (
            "https://spdx.org/spdxdocs/vox-digs-"
            f"{safe_version}-{args.commit}-{safe_label}-"
            f"{verification_code[:16]}"
        ),
        "creationInfo": {
            "created": created,
            "creators": ["Tool: VOX tools/generate-spdx-sbom.py"],
        },
        "documentDescribes": [project_id],
        "packages": [
            {
                "name": "VOX + DIGS",
                "SPDXID": project_id,
                "versionInfo": args.version,
                "downloadLocation": "NOASSERTION",
                "filesAnalyzed": True,
                "licenseConcluded": PROJECT_LICENSE,
                "licenseDeclared": PROJECT_LICENSE,
                "copyrightText": "NOASSERTION",
                "primaryPackagePurpose": args.purpose,
                "packageVerificationCode": {
                    "packageVerificationCodeValue": verification_code,
                    "packageVerificationCodeExcludedFiles": [
                        "./SBOM.spdx.json"
                    ],
                },
                "hasFiles": [item["SPDXID"] for item in files],
            },
            {
                "name": "SDL2",
                "SPDXID": sdl_id,
                "versionInfo": "2.x (minimum supported 2.0.10)",
                "downloadLocation": "https://github.com/libsdl-org/SDL/tree/SDL2",
                "filesAnalyzed": False,
                "licenseConcluded": SDL_LICENSE,
                "licenseDeclared": SDL_LICENSE,
                "copyrightText": "NOASSERTION",
                "primaryPackagePurpose": "LIBRARY",
                "comment": (
                    "External, system-provided dynamic runtime dependency. "
                    f"SDL2 is not included in this {distribution_kind}."
                ),
            },
        ],
        "files": files,
        "relationships": [
            {
                "spdxElementId": "SPDXRef-DOCUMENT",
                "relationshipType": "DESCRIBES",
                "relatedSpdxElement": project_id,
            },
            {
                "spdxElementId": project_id,
                "relationshipType": "DEPENDS_ON",
                "relatedSpdxElement": sdl_id,
                "comment": "External system dependency; not bundled.",
            },
        ]
        + [
            {
                "spdxElementId": project_id,
                "relationshipType": "CONTAINS",
                "relatedSpdxElement": item["SPDXID"],
            }
            for item in files
        ],
    }

    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as target:
        json.dump(document, target, indent=2, sort_keys=True)
        target.write("\n")
    os.replace(temporary, output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
