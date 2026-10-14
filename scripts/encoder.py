#!/usr/bin/env python3
"""Build the separate recording executable from pinned LGPL source.

Windows: run with MSYS2 MINGW64 Python, GCC, make, pkg-config and nasm.
Linux/macOS: Python 3.12+, a C compiler, make, pkg-config and nasm (x86).
No GPL/nonfree codecs, network input, device capture, or external media libraries.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import platform
import shlex
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    ("https://ffmpeg.org/releases/ffmpeg-7.1.5.tar.xz",
     "de668509caf9e35e3cd162473441fdb29538c6d96ed080292b3cf9e6fc5d558f", "ffmpeg-7.1.5"),
    ("https://codeload.github.com/FFmpeg/nv-codec-headers/tar.gz/refs/tags/n12.2.72.0",
     "dbeaec433d93b850714760282f1d0992b1254fc3b5a6cb7d76fc1340a1e47563", "nv-codec-headers-n12.2.72.0"),
]

def fetch(work):
    result = []
    for url, digest, name in SOURCES:
        archive = work / (name + (".tar.xz" if url.endswith(".xz") else ".tar.gz"))
        if not archive.exists():
            with urllib.request.urlopen(url, timeout=60) as response, archive.open("wb") as output:
                shutil.copyfileobj(response, output)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source hash mismatch: {archive}")
        source = work / name
        if not source.exists():
            with tarfile.open(archive) as package:
                package.extractall(work, filter="data")
        result.append((source, archive))
    return result

def build(work, destination, jobs=2):
    work = work.resolve(); destination = destination.resolve()
    work.mkdir(parents=True, exist_ok=True)
    ffmpeg, headers = fetch(work)
    windows = os.name == "nt" or platform.system().startswith(("MINGW", "MSYS"))
    mac = platform.system() == "Darwin"
    env = dict(os.environ)
    prefix = work / "headers"
    (prefix / "include/ffnvcodec").mkdir(parents=True, exist_ok=True)
    for header in (headers[0] / "include/ffnvcodec").glob("*.h"):
        shutil.copy2(header, prefix / "include/ffnvcodec" / header.name)
    pc = prefix / "lib/pkgconfig"; pc.mkdir(parents=True, exist_ok=True)
    (pc / "ffnvcodec.pc").write_text(
        f"prefix={prefix.as_posix()}\nincludedir=${{prefix}}/include\nName: ffnvcodec\n"
        "Description: NVIDIA codec headers\nVersion: 12.2.72.0\nCflags: -I${includedir}\nLibs:\n")
    env["PKG_CONFIG_PATH"] = str(pc) + os.pathsep + env.get("PKG_CONFIG_PATH", "")
    base = ["--disable-everything", "--disable-autodetect", "--disable-gpl", "--disable-nonfree", "--disable-network", "--disable-doc",
            "--disable-debug", "--disable-ffplay", "--disable-ffprobe", "--disable-shared",
            "--enable-static", "--enable-ffmpeg", "--enable-protocol=file,pipe",
            "--enable-demuxer=rawvideo", "--enable-decoder=rawvideo",
            "--enable-encoder=mpeg4" + ("" if mac else ",h264_nvenc"),
            "--enable-muxer=mp4,matroska,null", "--enable-filter=scale,format,color,null",
            "--enable-indev=lavfi", "--enable-avfilter", "--enable-avdevice", "--enable-swscale"]
    if not mac:
        base += ["--enable-ffnvcodec", "--enable-nvenc"]
    if windows:
        base += ["--target-os=mingw32", "--extra-ldflags=-static"]
    binaries, commands = [], []
    for arch in (["arm64", "x86_64"] if mac else [platform.machine()]):
        obj = work / ("obj-" + arch); obj.mkdir(exist_ok=True)
        flags = base[:]
        if mac:
            flags += ["--cc=clang -arch " + arch, "--arch=" + ("aarch64" if arch == "arm64" else "x86_64"),
                      "--target-os=darwin", "--enable-cross-compile",
                      "--extra-cflags=-mmacosx-version-min=11.0", "--extra-ldflags=-mmacosx-version-min=11.0"]
        # In the MSYS2 GitHub runner, `bash` can resolve to the Windows WSL
        # launcher instead of the active MSYS2 shell. Use MSYS2's POSIX sh
        # explicitly for the configure script; macOS keeps using bash.
        shell = "sh" if windows else "bash"
        command = [shell, str(ffmpeg[0] / "configure"), *flags]
        commands.append(shlex.join(command))
        subprocess.run(command, cwd=obj, env=env, check=True)
        subprocess.run(["make", f"-j{jobs}"], cwd=obj, env=env, check=True)
        binaries.append(obj / ("ffmpeg.exe" if windows else "ffmpeg"))
    destination.mkdir(parents=True, exist_ok=True)
    binary = destination / ("ffmpeg.exe" if windows else "ffmpeg")
    if mac:
        subprocess.run(["lipo", "-create", *map(str, binaries), "-output", str(binary)], check=True)
    else:
        shutil.copy2(binaries[0], binary)
    binary.chmod(0o755)
    license_text = subprocess.check_output([str(binary), "-L"], stderr=subprocess.STDOUT, text=True)
    build_config = subprocess.check_output([str(binary), "-buildconf"], stderr=subprocess.STDOUT, text=True)
    license_lower = license_text.lower()
    forbidden = re.compile(r"(^|\s)--enable-(?:gpl|nonfree)(?:\s|$)", re.IGNORECASE)
    if not ("lesser general public license" in license_lower or "lgpl" in license_lower) or forbidden.search(build_config):
        raise RuntimeError("Encoder licensing configuration did not pass")
    source_output = destination / "Source"; source_output.mkdir(exist_ok=True)
    for _, archive in (ffmpeg, headers):
        shutil.copy2(archive, source_output / archive.name)
    shutil.copy2(Path(__file__), source_output / "encoder.py")
    shutil.copy2(ffmpeg[0] / "COPYING.LGPLv2.1", source_output)
    # This tag keeps copyright/license notices in each header, not a LICENSE file.
    shutil.copytree(headers[0] / "include", source_output / "nv-codec-headers-include", dirs_exist_ok=True)
    (source_output / "BUILD.json").write_text(json.dumps({"commands": commands, "modified_sources": False,
        "sources": SOURCES, "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest()}, indent=2))
    (destination / "LICENSE.txt").write_text(license_text)
    # Verify the actual raw RGB -> MPEG-4 path, both containers, without a GPU or desktop.
    for suffix in ("mp4", "mkv"):
        output = work / ("encoder-check." + suffix)
        subprocess.run([str(binary), "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24",
            "-s", "320x180", "-r", "24", "-i", "pipe:0", "-an", "-c:v", "mpeg4", "-b:v", "2000k",
            "-pix_fmt", "yuv420p", str(output)], input=bytes([30, 90, 220]) * 320 * 180 * 24,
            check=True, timeout=30)
        if output.stat().st_size < 1024:
            raise RuntimeError("Encoder smoke test produced an empty file")
    print(f"Built and tested recording encoder: {binary}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--work", type=Path, default=ROOT / "build/encoder-source")
    parser.add_argument("--destination", type=Path, default=ROOT / "build/encoder")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args(); build(args.work, args.destination, args.jobs)
