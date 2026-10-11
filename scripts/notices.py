#!/usr/bin/env python3
"""Include upstream license documents from the exact dependency checkouts."""
import argparse
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
def collect(build, stage):
    manifest = json.loads((build / "grid-dependencies.json").read_text())
    output = stage / "ThirdPartyNotices"
    for dependency, directory in manifest.items():
        if not directory: continue
        source = Path(directory)
        if not source.exists(): raise RuntimeError(f"Missing dependency source for notices: {dependency}")
        for path in source.rglob("*"):
            name = path.name.lower()
            if path.is_file() and ("license" in name or "licence" in name or "copying" in name):
                if ".git" in path.relative_to(source).parts: continue
                target = output / dependency / path.relative_to(source)
                target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(path, target)
    if not (output / "juce/LICENSE.md").is_file(): raise RuntimeError("JUCE license notice was not collected")
    print("Collected dependency license documents")
if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--stage", type=Path, default=ROOT / "dist/stage")
    args = parser.parse_args(); collect(args.build, args.stage)
