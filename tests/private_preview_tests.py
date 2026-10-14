"""Private delivery fixtures: current release, provenance and stale-build rejection."""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("private_preview", ROOT / "scripts/private_preview.py")
preview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preview)

with tempfile.TemporaryDirectory() as temporary:
    base = Path(temporary)
    source, build = base / "source", base / "build"
    source.mkdir(); build.mkdir()
    (source / "docs").mkdir()
    (source / "CMakeLists.txt").write_text("project(GRID_ARVx VERSION 9.8.7 LANGUAGES C CXX)", encoding="utf-8")
    for name in ("LICENSE-NOTICE.md", "Launch-GRID-ARVx.cmd"):
        (source / name).write_text("fixture", encoding="utf-8")
    for name in ("PRIVATE-TESTING.md", "ORIGINAL-ENGINE.md", "VALIDATION.md", "AUDIO.md"):
        (source / "docs" / name).write_text("fixture", encoding="utf-8")
    (build / "CMakeCache.txt").write_text("GRID_USE_VEL_ENGINE:BOOL=ON", encoding="utf-8")

    def release(name, marker):
        directory = build / "GridARVxApp_artefacts" / name
        directory.mkdir(parents=True)
        (directory / "GRID-ARVx.exe").write_bytes(marker)
        bundle = directory / "Engine/Vel Engine.vst3"
        for relative in ("Contents/x86_64-win/Vel Engine.vst3", "Contents/Resources/moduleinfo.json", "Plugin.ico", "desktop.ini"):
            file = bundle / relative; file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(b"engine fixture")
        (bundle / "activation.settings").write_bytes(b"PRIVATE FIXTURE MUST NOT SHIP")
        (directory / "state.bin").write_bytes(b"USER FIXTURE MUST NOT SHIP")
        return directory

    current = release("Release-9.8.7", b"current")
    stale = release("Release", b"stale")
    override = release("Custom", b"override")
    version = lambda file: "0.0.1" if file.read_bytes() == b"stale" else "9.8.7"
    notices = SimpleNamespace(collect=lambda build, destination: None)
    with patch.object(preview, "ROOT", source), patch.object(preview, "ENGINE_HASH", hashlib.sha256(b"engine fixture").hexdigest()), patch.object(preview, "executable_version", version), patch.dict(sys.modules, {"notices": notices}):
        destination = base / "current-package"
        preview.stage(build, destination)
        assert (destination / "GRID-ARVx.exe").read_bytes() == b"current"
        manifest = json.loads((destination / "package-manifest.json").read_text(encoding="utf-8"))
        assert manifest["version"] == "9.8.7"
        assert manifest["files"]["GRID-ARVx.exe"]["sha256"] == hashlib.sha256(b"current").hexdigest()
        with zipfile.ZipFile(base / "current-package.zip") as archive:
            assert not any("activation.settings" in name or "state.bin" in name for name in archive.namelist())
        preview.stage(build, base / "explicit-package", override)
        assert (base / "explicit-package/GRID-ARVx.exe").read_bytes() == b"override"
        try:
            preview.stage(build, base / "stale-package", stale)
        except RuntimeError as error:
            assert "version" in str(error)
        else:
            raise AssertionError("Stale executable accepted")
        assert not (base / "stale-package").exists()
        try:
            preview.stage(build, destination)
        except RuntimeError:
            pass
        else:
            raise AssertionError("Existing package overwritten")
        (current / "Engine/Vel Engine.vst3/Contents/x86_64-win/Vel Engine.vst3").write_bytes(b"wrong engine")
        try:
            preview.stage(build, base / "bad-engine")
        except RuntimeError as error:
            assert "fingerprint" in str(error)
        else:
            raise AssertionError("Unverified engine accepted")
        assert not (base / "bad-engine").exists()

print("PASS: current/explicit release selection, source version, file hashes, stale-build and engine rejection, no overwrite, personal-state exclusion")
