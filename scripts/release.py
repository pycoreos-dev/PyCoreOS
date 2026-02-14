#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import tarfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
RELEASES = BUILD / "releases"
RELEASE_HEADER = ROOT / "kernel" / "include" / "kernel" / "release.h"


def run(cmd: list[str]) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=ROOT)


def release_define(name: str) -> str:
    content = RELEASE_HEADER.read_text(encoding="utf-8")
    match = re.search(rf'^\s*#define\s+{re.escape(name)}\s+"([^"]+)"\s*$', content, re.MULTILINE)
    if not match:
        raise RuntimeError(f"Could not parse {name} from {RELEASE_HEADER}")
    return match.group(1)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def build_release_bundle() -> Path:
    version = release_define("PYCOREOS_VERSION")
    channel = release_define("PYCOREOS_CHANNEL")
    codename = release_define("PYCOREOS_CODENAME")

    run(["python3", "scripts/build.py", "test"])

    iso_src = BUILD / "pycoreos.iso"
    if not iso_src.exists():
        raise RuntimeError(f"ISO not found: {iso_src}")

    tag = f"pycoreos-{version}"
    bundle_dir = RELEASES / tag
    if bundle_dir.exists():
        shutil.rmtree(bundle_dir)
    bundle_dir.mkdir(parents=True, exist_ok=True)

    iso_out = bundle_dir / f"{tag}.iso"
    shutil.copy2(iso_src, iso_out)

    docs = [
        "README.md",
        "CHANGELOG.md",
        "BETA_TESTING.md",
        "RELEASE_CHECKLIST.md",
    ]
    for name in docs:
        src = ROOT / name
        if src.exists():
            shutil.copy2(src, bundle_dir / name)

    metadata = {
        "name": "PyCoreOS",
        "version": version,
        "channel": channel,
        "codename": codename,
        "built_utc": datetime.now(timezone.utc).isoformat(),
        "artifacts": [p.name for p in sorted(bundle_dir.iterdir()) if p.is_file()],
    }
    (bundle_dir / "release.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")

    checksums: list[str] = []
    for artifact in sorted(bundle_dir.iterdir()):
        if not artifact.is_file() or artifact.name == "SHA256SUMS":
            continue
        checksums.append(f"{sha256(artifact)}  {artifact.name}")
    (bundle_dir / "SHA256SUMS").write_text("\n".join(checksums) + "\n", encoding="utf-8")

    archive = RELEASES / f"{tag}-bundle.tar.gz"
    if archive.exists():
        archive.unlink()
    with tarfile.open(archive, "w:gz") as tar:
        tar.add(bundle_dir, arcname=bundle_dir.name)

    return archive


def main() -> int:
    parser = argparse.ArgumentParser(description="Create PyCoreOS release bundles")
    parser.add_argument("target", choices=["beta"])
    args = parser.parse_args()
    if args.target == "beta":
        archive = build_release_bundle()
        print(f"Beta release bundle ready: {archive}")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
