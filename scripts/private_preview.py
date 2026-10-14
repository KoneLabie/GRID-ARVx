#!/usr/bin/env python3
"""Stage the authorized Windows standalone preview, without personal state."""
import argparse
import ctypes
import os
import hashlib
import json
import re
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ENGINE_HASH = "110757113effef4c55d033904b9819a856a6c57f0749dc7b4809740e6c0a6b59"


def executable_version(path: Path):
    """Read the Windows file version without loading or running the executable."""
    if os.name != "nt":
        raise RuntimeError("Windows release verification must run on Windows")
    library = ctypes.WinDLL("version", use_last_error=True)
    library.GetFileVersionInfoSizeW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p]
    library.GetFileVersionInfoW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    library.VerQueryValueW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint32)]
    size = library.GetFileVersionInfoSizeW(str(path), None)
    data = ctypes.create_string_buffer(size)
    if not size or not library.GetFileVersionInfoW(str(path), 0, size, data):
        raise RuntimeError("Executable is missing Windows version information")
    pointer, length = ctypes.c_void_p(), ctypes.c_uint32()
    if not library.VerQueryValueW(data, "\\", ctypes.byref(pointer), ctypes.byref(length)) or length.value < 52:
        raise RuntimeError("Executable has invalid Windows version information")
    fixed = ctypes.cast(pointer, ctypes.POINTER(ctypes.c_uint32))
    if fixed[0] != 0xFEEF04BD:
        raise RuntimeError("Executable has invalid version signature")
    return ".".join(str(value) for value in (fixed[2] >> 16, fixed[2] & 0xffff, fixed[3] >> 16))


def stage(build: Path, destination: Path, release_directory: Path | None = None):
    build = build.resolve()
    destination = destination.resolve()
    archive = destination.parent / (destination.name + ".zip")
    if destination.exists():
        raise RuntimeError("Choose a new staging directory; existing files are never deleted.")
    if archive.exists():
        raise RuntimeError("Archive already exists; choose another destination")
    if destination == ROOT or destination in ROOT.parents or destination == build:
        raise RuntimeError("Unsafe staging destination")
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(GRID_ARVx\s+VERSION\s+([0-9.]+)\s", cmake)
    if not match:
        raise RuntimeError("Cannot determine the source project version")
    version = match.group(1)
    if "GRID_USE_VEL_ENGINE:BOOL=ON" not in cache:
        raise RuntimeError("This package requires a native-engine build.")
    versioned = build / "GridARVxApp_artefacts" / ("Release-" + version)
    release = release_directory.resolve() if release_directory else versioned if versioned.is_dir() else build / "GridARVxApp_artefacts/Release"
    app = release / "GRID-ARVx.exe"
    bundle = release / "Engine/Vel Engine.vst3"
    runtime = bundle / "Contents/x86_64-win/Vel Engine.vst3"
    if not app.is_file() or not runtime.is_file():
        raise RuntimeError("Build the native standalone target first.")
    if executable_version(app) != version:
        raise RuntimeError("Executable version does not match the source project; rebuild or select --release-dir")
    if hashlib.sha256(runtime.read_bytes()).hexdigest() != ENGINE_HASH:
        raise RuntimeError("Vel Engine fingerprint mismatch")
    engine_files = [
        "Contents/x86_64-win/Vel Engine.vst3",
        "Contents/Resources/moduleinfo.json",
        "Plugin.ico",
        "desktop.ini",
    ]
    for name in engine_files:
        if not (bundle / name).is_file():
            raise RuntimeError(f"Missing bundled engine resource: {name}")
    destination.mkdir(parents=True)
    shutil.copy2(app, destination / app.name)
    for name in engine_files:
        target = destination / "Engine/Vel Engine.vst3" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(bundle / name, target)
    for name in ["LICENSE-NOTICE.md", "Launch-GRID-ARVx.cmd"]:
        shutil.copy2(ROOT / name, destination / name)
    shutil.copy2(ROOT / "docs/PRIVATE-TESTING.md", destination / "READ-ME-FIRST.md")
    docs = destination / "docs"
    docs.mkdir()
    for name in ["ORIGINAL-ENGINE.md", "VALIDATION.md", "AUDIO.md"]:
        shutil.copy2(ROOT / "docs" / name, docs / name)
    # License documents only: no dependency trees, credentials or user settings.
    from notices import collect
    collect(build, destination)
    manifest = {
        "product": "GRID-ARVx", "version": version, "platform": "Windows x64",
        "purpose": "Authorized private testing only; no resale or public upload",
        "engine": "Vel Engine (Greyland Audio Visual Lab Pro 1.2.0); installed engine is never loaded",
        "requires": ["OpenGL-capable GPU"],
        "unverified": ["Activation on a clean tester machine", "Sustained performance", "DAW hosting"],
        "recording": "Select an existing FFmpeg encoder in Settings; no encoder is bundled",
        "files": {},
    }
    for path in sorted(destination.rglob("*")):
        if path.is_file():
            manifest["files"][path.relative_to(destination).as_posix()] = {
                "bytes": path.stat().st_size,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            }
    (destination / "package-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
        for path in sorted(destination.rglob("*")):
            if path.is_file():
                output.write(path, Path(destination.name) / path.relative_to(destination))
    print(f"Private preview staged: {destination}")
    print(f"Private archive: {archive}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--release-dir", type=Path, help="Explicit standalone output directory; defaults to the current versioned Release")
    args = parser.parse_args()
    stage(args.build, args.destination, args.release_dir)
