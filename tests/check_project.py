#!/usr/bin/env python3
"""Dependency-free validation before a native compiler is invoked."""
import ast
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
for path in list((ROOT / "scripts").glob("*.py")) + list((ROOT / "tests").glob("*.py")):
    ast.parse(path.read_text(), filename=str(path))
required = ["CMakeLists.txt", "README.md", "LICENSE-NOTICE.md", ".github/workflows/build.yml",
            "Source/Plugin/Processor.cpp", "Source/Plugin/Editor.cpp", "Source/Visual/Canvas.cpp",
            "Source/App/Main.cpp", "Source/Core/AudioAnalysis.h", "tests/audio_tests.cpp",
            "installer/windows/GRID-ARVx.iss", "installer/linux/header.sh"]
for name in required:
    assert (ROOT / name).is_file(), f"Missing {name}"
cmake = (ROOT / "CMakeLists.txt").read_text()
for source in re.findall(r"(?:Source|tests)/[A-Za-z0-9_/]+\.cpp", cmake):
    assert (ROOT / source).is_file(), f"Invalid CMake source: {source}"
assert 'GRID_ENABLE_VST2 "Reserved for an authorized legacy implementation" OFF' in cmake
assert 'GRID_ENABLE_AAX "Reserved for an authorized AAX implementation" OFF' in cmake
assert len(re.findall(r"GIT_TAG [0-9a-f]{40}", cmake)) == 2, "Dependencies must be pinned"
for path in (ROOT / "Source").rglob("*.cpp"):
    content = path.read_text()
    assert content.count('R"glsl(') == content.count(')glsl";'), f"Unclosed shader literal: {path}"
print("PASS: source paths, Python syntax, dependency pins, and reserved SDK options")
