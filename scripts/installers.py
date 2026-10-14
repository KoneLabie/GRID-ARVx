#!/usr/bin/env python3
"""Build native installers from a verified stage, never from source placeholders."""
import argparse
import io
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
def version():
    return re.search(r"project\(GRID_ARVx VERSION ([0-9.]+)", (ROOT / "CMakeLists.txt").read_text(encoding='utf-8')).group(1)
def linux(stage, output):
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz") as archive:
        for child in stage.iterdir(): archive.add(child, arcname=child.name)
    payload = buffer.getvalue()
    header = (ROOT / "installer/linux/header.sh").read_bytes().replace(b"@PAYLOAD_BYTES@", str(len(payload)).encode())
    output.write_bytes(header + payload); output.chmod(0o755)
def macos(stage, output):
    work = output.parent / "macos-work"
    if work.exists(): shutil.rmtree(work)
    work.mkdir(parents=True)
    distribution = ET.Element("installer-gui-script", minSpecVersion="2")
    ET.SubElement(distribution, "title").text = "GRID-ARVx by Cenaceae"
    ET.SubElement(distribution, "options", customize="always", **{"require-scripts": "false", "hostArchitectures": "arm64,x86_64"})
    outline = ET.SubElement(distribution, "choices-outline")
    notices_root = work / "Notices"
    notices = notices_root / "Library/Application Support/Cenaceae/GRID-ARVx"
    notices.mkdir(parents=True)
    shutil.copy2(stage / "LICENSE-NOTICE.md", notices)
    for folder in ["docs", "ThirdPartyNotices"]:
        if (stage / folder).exists(): shutil.copytree(stage / folder, notices / folder)
    shutil.copytree(stage / "Tools", notices / "tools")
    notice_id = "com.cenaceae.gridarvx.notices"
    subprocess.run(["pkgbuild", "--root", str(notices_root), "--identifier", notice_id,
                    "--version", version(), "--install-location", "/", str(work / "Notices.pkg")], check=True)
    ET.SubElement(outline, "line", choice="Notices")
    notice_choice = ET.SubElement(distribution, "choice", id="Notices", visible="false", enabled="false", start_selected="true")
    ET.SubElement(notice_choice, "pkg-ref", id=notice_id)
    ET.SubElement(distribution, "pkg-ref", id=notice_id, version=version()).text = "Notices.pkg"
    destinations = {"Standalone": "Applications", "VST3": "Library/Audio/Plug-Ins/VST3",
                    "AU": "Library/Audio/Plug-Ins/Components", "CLAP": "Library/Audio/Plug-Ins/CLAP"}
    for component, location in destinations.items():
        source = stage / component
        if not source.exists(): raise RuntimeError(f"Missing {component} stage")
        payload = work / component / location
        shutil.copytree(source, payload, symlinks=True)
        identifier = "com.cenaceae.gridarvx." + component.lower()
        package = work / (component + ".pkg")
        subprocess.run(["pkgbuild", "--root", str(work / component), "--identifier", identifier,
                        "--version", version(), "--install-location", "/", str(package)], check=True)
        ET.SubElement(outline, "line", choice=component)
        choice = ET.SubElement(distribution, "choice", id=component, title=component, start_selected="true")
        ET.SubElement(choice, "pkg-ref", id=identifier)
        ET.SubElement(distribution, "pkg-ref", id=identifier, version=version()).text = package.name
    xml = work / "distribution.xml"; ET.ElementTree(distribution).write(xml, encoding="utf-8", xml_declaration=True)
    subprocess.run(["productbuild", "--distribution", str(xml), "--package-path", str(work), str(output)], check=True)
def windows(stage, output):
    iscc = shutil.which("ISCC")
    if not iscc:
        candidate = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Inno Setup 6/ISCC.exe"
        if candidate.exists(): iscc = str(candidate)
    if not iscc: raise RuntimeError("Inno Setup 6 is required to generate the Windows installer")
    subprocess.run([iscc, f"/DStageDir={stage.resolve()}", f"/DAppVersion={version()}",
                    f"/O{output.parent.resolve()}", str(ROOT / "installer/windows/GRID-ARVx.iss")], check=True)
def build(platform, stage, output):
    required = ["Standalone", "VST3", "CLAP", "Tools"] + (["AU"] if platform == "macOS" else [])
    for component in required:
        path = stage / component
        if not path.is_dir() or not any(path.iterdir()): raise RuntimeError(f"Missing installer component: {component}")
    output.parent.mkdir(parents=True, exist_ok=True)
    {"Windows": windows, "macOS": macos, "Linux": linux}[platform](stage, output)
    print(f"Installer: {output}")
if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("platform", choices=["Windows", "macOS", "Linux"])
    parser.add_argument("--stage", type=Path, default=ROOT / "dist/stage")
    args = parser.parse_args()
    extension = {"Windows": "exe", "macOS": "pkg", "Linux": "run"}[args.platform]
    build(args.platform, args.stage, ROOT / f"dist/GRID-ARVx-Installer-{args.platform}.{extension}")
