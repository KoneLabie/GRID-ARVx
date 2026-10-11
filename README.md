# GRID-ARVx — Kone Labie

An independent audio-reactive visualizer foundation for a standalone desktop app
and DAW plugins. This repository is a new project with its own implementation,
identifiers, settings directory, and installer identity.

## Upload and build in GitHub

1. Extract this ZIP. Open the **GRID-ARVx** folder.
2. Create your empty GitHub repository, then upload the **contents** of that folder
   to its root. `CMakeLists.txt` must be at the repository root.
3. Include `.github/workflows/build.yml` and `.gitignore`. If the web uploader
   omits `.github`, use **Add file → Create new file**, enter
   `.github/workflows/build.yml`, and paste the included workflow file.
4. Commit to `main` or `master`. Open **Actions → Build GRID-ARVx installers**.
   You can also use **Run workflow** on any branch after the workflow is present
   on the repository's default branch.
5. When all jobs pass, download the Windows, macOS, and Linux artifacts.
   Extract each artifact once to get its native installer.

The workflow fetches pinned public dependencies, compiles, runs audio tests, and
generates installers. No GitHub token, repository name, paid signing service, or
private SDK needs to be entered for the default build. The workflow uploads build
artifacts to your repository; it does not create a public release.

| Build | Installer | Components |
| --- | --- | --- |
| Windows x64 | `GRID-ARVx-Installer-Windows.exe` | Standalone, VST3, CLAP |
| macOS 11+ universal (Apple silicon / Intel) | `GRID-ARVx-Installer-macOS.pkg` | Standalone, VST3, AU, CLAP |
| Linux x64 (Ubuntu 22.04 build baseline) | `GRID-ARVx-Installer-Linux.run` | Standalone, VST3, CLAP |

Windows and macOS installers have component selection. On Linux, run
`bash GRID-ARVx-Installer-Linux.run` for the menu, or pass `--all`, `--standalone`,
`--vst3`, or `--clap`. Linux installs into your user account; Windows/macOS install
plugins in their standard system locations. The installers are unsigned in this
foundation. Signing and macOS notarization are later steps, not prerequisites for
producing test installers; your OS may require explicit permission to open them.

## What this foundation actually does

- Three original live visuals: Oscilloscope, Basic Colors, and Plasma.
- Audio level, peak, low/mid/high analysis and a waveform; bounded queue between
  the audio callback and visualization. No audio-thread rendering or file I/O.
- Mono/stereo DAW audio passes through unchanged. Visual mode, sensitivity,
  motion speed, and noise texture can be automated by the host.
- Drop PNG/JPG images for a pulsing overlay, TXT text, and TTF/OTF fonts.
  Imported image/font files are copied into the application's asset directory.
- Text overlays pulse with audio; text, asset references, and parameters restore
  with a DAW project. Standalone state and audio-device choice persist locally.
- Shared standalone/plugin UI and experimental OpenGL 3.2 renderer.

This is the agreed **foundation** scope, not a complete recreation of Visual Lab
Pro. Image silhouettes are displayed using their existing transparency; advanced
edge extraction, text warping, timed lyric files, setlists, multiple tiles, and
the full FX families are still planned. See [docs/FEATURES.md](docs/FEATURES.md).

Standalone input supports microphones, interfaces, and audio input devices your
OS exposes. For system playback, select a loopback/monitor/virtual input in
**Audio input settings**. Native automatic system-output capture and bundled
virtual drivers are not implemented. See [docs/AUDIO.md](docs/AUDIO.md).

VST2 and AAX are reserved, disabled options and have no working wrappers in this
foundation. Pro Tools support is therefore not included. AU is macOS-only.
Plugin compatibility is a build target, not a promise that every DAW has passed
testing. See [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

## Local developer build

Install CMake 3.22+, Git, and a C++20 compiler (Visual Studio 2022, Xcode, or GCC
11+). For Linux, install the packages shown in the workflow. Then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
python scripts/stage.py Linux
python scripts/notices.py
python scripts/installers.py Linux
```

Replace `Linux` with `Windows` or `macOS` when running on those systems.
Windows additionally requires Inno Setup 6; macOS uses Xcode's `pkgbuild` and
`productbuild`. Downloading dependencies requires internet access.

Core-only tests do not fetch JUCE or require an audio device:

```sh
cmake -S . -B build-core -DGRID_CORE_TESTS_ONLY=ON
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

## Repository contents

| Folder/file | Purpose |
| --- | --- |
| `.github/workflows` | Three-platform GitHub Actions build |
| `Source/Core` | Dependency-free audio analysis |
| `Source/Plugin` | DAW processor, automation, state, UI |
| `Source/Visual` | Original shader renderer and overlays |
| `Source/App` | Standalone audio input and window |
| `installer` | Native installer templates |
| `scripts` | Staging, notices, installer generation |
| `tests` | Audio and packaging checks |
| `docs` | Architecture, research, features, compatibility, validation |
| `CMakeLists.txt` | Build targets and pinned dependencies |

Read [docs/VALIDATION.md](docs/VALIDATION.md) for checks actually performed.
Dependency licensing still applies to private and free builds. A public
repository does not itself select a source-code license. No project-wide
open-source license has been chosen for you. See [LICENSE-NOTICE.md](LICENSE-NOTICE.md).
