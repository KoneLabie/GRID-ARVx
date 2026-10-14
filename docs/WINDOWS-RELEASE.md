# Windows release — 0.4.18

This release packages the **Windows x64 standalone with Vel Engine** using the
project's existing Inno Setup 6 recipe. Linux and macOS installers are deferred
until a compatible engine is available. DAW plugins are excluded from this
installer; no independent-renderer substitution is made.

The installer targets Windows 10 version 2004 or newer and Windows 11. It installs
per user into `%LOCALAPPDATA%\Programs\GRID-ARVx`, with Start menu and optional
desktop shortcuts, an uninstaller, the adjacent engine bundle and dependency
notices. It does not require Visual Lab Pro to be installed. GPU/OpenGL and audio
driver compatibility still require testing on each tester's machine.

## Upload the source ZIP to GitHub

1. Create or use the authorized repository; public repository distribution is allowed.
2. Extract the ZIP and upload the contents of its `GRID-ARVx-0.4.18` folder into
   the repository root. Uploading only the ZIP file will not run the workflow.
3. Include `.github`, `Source`, `scripts`, `tests`, `installer`, `docs`, CMake files
   and the supplied `vendor/VelEngine` files. The engine allowlist in `.gitignore`
   permits exactly the four supplied bundle files, excluding other vendor state.
4. In **Actions → Build Windows installer**, run the workflow or push to `main`.
   Download the `GRID-ARVx-Windows-x64-Installer` artifact when it completes.

The workflow verifies the public-distribution setting, builds the native Windows
standalone, runs source/packaging/core checks, and generates the Inno installer.
It does not publish a release. GPU checks and OBS verification require real
hardware; compiling on GitHub is not equivalent to those tests.

`SOURCE-ARCHIVE.json` records every archived source/engine file with its SHA-256.
The archive excludes Git history, build caches, runtime user state, recordings,
license keys and local configuration. Engine metadata and attribution remain.
Public source and installer distribution is authorized under the creator's clearance.

## Build locally

Requirements: Visual Studio 2022 C++ desktop tools, CMake 3.22+, Git, Python 3.12+
and [Inno Setup 6](https://jrsoftware.org/isdl.php). Run from the extracted root:

```powershell
cmake -S . -B build -A x64 -DGRID_USE_VEL_ENGINE=ON -DGRID_BUILD_PLUGINS=OFF -DGRID_ENABLE_CLAP=OFF
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
python tests/check_project.py
python tests/private_preview_tests.py
python tests/windows_release_tests.py
python scripts/windows_release.py --iscc "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
```

The output is `dist/GRID-ARVx-0.4.18-Windows-x64-Setup.exe`. Staging rejects stale
app versions, unexpected engine hashes and existing output paths. To rebuild an
installer, use a fresh `--stage` and `--output` path rather than overwriting a
prior result. `--release-dir` selects an explicit compiled application folder.

## Recording encoder

The standard installer does not bundle FFmpeg. Select an existing executable in
Settings → Choose FFmpeg encoder to record. Visuals and audio input run without
it. The Actions manual-run option **bundle_encoder** builds the pinned encoder
and stages its corresponding source/notices; that optional source-build path
still needs a successful GitHub runner build. Locally, run `scripts/encoder.py`
with the documented MSYS2 prerequisites, then pass `--encoder build/encoder` to
`windows_release.py`. An arbitrary external encoder is not silently repackaged.

## Validation boundaries

The application passed the 0.4.18 local regression pass described in VALIDATION.md.
Installer fixture tests check engine/version validation, Inno arguments, file
hashes, personal-state exclusion and source archive integrity. Installer install,
installed-app smoke, uninstall and archive verification results accompany this
delivery in its packaging report. GitHub Actions has not been run by this agent.
The installer is unsigned; no signing credentials are included. A clean-machine
test remains necessary before wider distribution.
