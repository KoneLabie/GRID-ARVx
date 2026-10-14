#!/usr/bin/env python3
"""Stage compiled JUCE products; fail before making an incomplete installer."""
import argparse
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]
def stage(build: Path, destination: Path, platform: str, config="Release", clap=True):
    plugin = build / "GridARVxPlugin_artefacts" / config
    app = build / "GridARVxApp_artefacts" / config
    encoder = build / "encoder"
    binary = encoder / ("ffmpeg.exe" if platform == "Windows" else "ffmpeg")
    if not binary.is_file() or not (encoder / "Source/BUILD.json").is_file():
        raise RuntimeError("Missing tested encoder and corresponding source. Run scripts/encoder.py first.")
    if destination.is_symlink() or destination.resolve() in (ROOT.resolve(), build.resolve()) or destination.resolve() in ROOT.resolve().parents:
        raise RuntimeError("Unsafe staging destination")
    if destination.exists(): shutil.rmtree(destination)
    destination.mkdir(parents=True)
    required = ["VST3"] + (["AU"] if platform == "macOS" else []) + (["CLAP"] if clap else [])
    for name in required:
        source = plugin / name
        if not source.is_dir() or not any(source.iterdir()): raise RuntimeError(f"Missing built {name}: {source}")
        shutil.copytree(source, destination / name, symlinks=True)
    product = app / ("GRID-ARVx.app" if platform == "macOS" else "GRID-ARVx.exe" if platform == "Windows" else "GRID-ARVx")
    if not product.exists(): raise RuntimeError(f"Missing standalone product: {product}")
    standalone = destination / "Standalone"; standalone.mkdir()
    if product.is_dir(): shutil.copytree(product, standalone / product.name, symlinks=True)
    else: shutil.copy2(product, standalone / product.name)
    shutil.copy2(ROOT / "README.md", destination)
    shutil.copy2(ROOT / "LICENSE-NOTICE.md", destination)
    shutil.copytree(ROOT / "docs", destination / "docs")
    shutil.copytree(encoder, destination / "Tools")
    print(f"Staged {platform} products in {destination}")
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", choices=["Windows", "macOS", "Linux"])
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--destination", type=Path, default=ROOT / "dist/stage")
    parser.add_argument("--without-clap", action="store_true")
    args = parser.parse_args(); stage(args.build, args.destination, args.platform, clap=not args.without_clap)
