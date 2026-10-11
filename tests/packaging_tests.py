#!/usr/bin/env python3
"""Exercise staging, macOS payload assembly, and Linux installation with fixtures."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
from unittest.mock import patch
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
def module(name):
    specification = importlib.util.spec_from_file_location(name, ROOT / f"scripts/{name}.py")
    result = importlib.util.module_from_spec(specification); specification.loader.exec_module(result); return result
staging, installers = module("stage"), module("installers")
with tempfile.TemporaryDirectory() as directory:
    root = Path(directory); build = root / "build"; stage = root / "stage"
    for component, filename in [("VST3", "GRID-ARVx.vst3"), ("CLAP", "GRID-ARVx.clap")]:
        product = build / "GridARVxPlugin_artefacts/Release" / component / filename
        product.parent.mkdir(parents=True); product.write_text("fixture")
    app = build / "GridARVxApp_artefacts/Release/GRID-ARVx"; app.parent.mkdir(parents=True)
    app.write_text("#!/bin/sh\necho fixture\n"); app.chmod(0o755)
    staging.stage(build, stage, "Linux")
    output = root / "installer.run"; installers.build("Linux", stage, output)
    for options, expected in [("--standalone", "Standalone"), ("--vst3", "VST3"), ("--clap", "CLAP"), ("--all", "All")]:
        home = root / options[2:]; home.mkdir()
        subprocess.run(["bash", str(output), options], env={**os.environ, "HOME": str(home)}, check=True, capture_output=True)
        assert (home / ".vst3/GRID-ARVx.vst3").exists() == (expected in ("VST3", "All"))
        assert (home / ".clap/GRID-ARVx.clap").exists() == (expected in ("CLAP", "All"))
        assert (home / ".local/opt/KoneLabie/GRID-ARVx/GRID-ARVx").exists() == (expected in ("Standalone", "All"))
        if expected in ("Standalone", "All"):
            assert (home / ".local/share/applications/konelabie-grid-arvx.desktop").exists()
    no_changes = root / "invalid"; no_changes.mkdir()
    result = subprocess.run(["bash", str(output), "--standalone", "--invalid"],
        env={**os.environ, "HOME": str(no_changes)}, capture_output=True)
    assert result.returncode != 0 and not list(no_changes.iterdir()), "Bad selection partially installed"
    incomplete = root / "incomplete"; incomplete.mkdir()
    try: installers.build("Linux", incomplete, root / "invalid.run")
    except RuntimeError: pass
    else: raise AssertionError("Incomplete payload was accepted")
    (stage / "AU").mkdir(); (stage / "AU/GRID-ARVx.component").mkdir()
    bundle = stage / "Standalone/GRID-ARVx.app/Contents"; bundle.mkdir(parents=True)
    (bundle / "Info.plist").write_text("fixture")
    (stage / "Standalone/GRID-ARVx").unlink()
    with patch.object(installers.subprocess, "run") as commands:
        installers.macos(stage, root / "installer.pkg")
    assert commands.call_count == 6, "Expected four components, required notices, and one product package"
    work = root / "macos-work"
    assert (work / "Standalone/Applications/GRID-ARVx.app/Contents/Info.plist").exists()
    xml = ET.parse(work / "distribution.xml")
    assert {node.attrib["id"] for node in xml.findall("choice")} == {"Standalone", "VST3", "AU", "CLAP", "Notices"}
print("PASS: Linux installer component selection, desktop shortcut, bad selections, missing components")
print("PASS: macOS component payload layout and selectable package XML (native tools mocked)")
