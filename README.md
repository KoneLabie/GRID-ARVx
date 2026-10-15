# GRID-ARVx — Cenaceae

GRID-ARVx 0.4.18 is a Windows audio-reactive visualizer with a custom UI and an
adjacent Vel Engine runtime. This private testing build includes Lab, Multiple
and Performance, native visual defaults/FX, image tracing, Text, separate MIDI
mapping banks, speaker loopback input and capture controls.

**Current release: Windows x64 standalone.** Linux/macOS installers are deferred
until a compatible engine is available. DAW plugins are not in this installer.
Older independent-renderer targets remain in the source for future development.

## Install

Run `GRID-ARVx-0.4.18_Windows-Setup.exe`. The Inno Setup installer installs
the app and its engine per user, adds shortcuts and includes an uninstaller.
Visual Lab Pro does not need to be installed. Target: Windows 10 version 2004+
or Windows 11, with compatible GPU/OpenGL and audio drivers.

The Windows installer bundles the pinned FFmpeg recording encoder with its
corresponding source and license notices. Users can record immediately after
installation without downloading FFmpeg separately.

## GitHub source package

Extract the source ZIP and upload the **contents** of its `GRID-ARVx-0.4.18`
folder into the root of the repository. Include `.github` and `vendor`.
Uploading the ZIP itself does not install the source or trigger a build.

In **Actions → Build Windows installer**, run the workflow, or push to `main`
or `master`. It compiles the native Windows standalone, runs source/packaging/core
checks, and uploads the Windows installer artifact. It does not publish releases.
The adjacent engine's four required files are explicitly allowed by `.gitignore`;
personal state and other vendor files remain excluded. `SOURCE-ARCHIVE.json`
contains the source package's file hashes.

YOU ARE NOT ALLOWED TO RE-DISTRIBUTE THIS!

## Build the Windows standalone

Install Visual Studio 2022 C++ desktop tools, Git, CMake 3.22+, Python 3.12+ and
Inno Setup 6. From the source root:

```powershell
cmake -S . -B build -A x64 -DGRID_USE_VEL_ENGINE=ON -DGRID_BUILD_PLUGINS=OFF -DGRID_ENABLE_CLAP=OFF
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
python scripts/windows_release.py --iscc "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
```

## Validation and limitations

The 0.4.18 local regression pass covered 241 UI checks, 170 live transitions,
core audio/MIDI/mapping suites, native Text/FX captures, asset imports and native
render targets from 480p through 4K. Heavy recording can drop input frames under
load while preserving output duration. Physical MIDI, OBS capture methods,
clean-machine installs, different drivers and long sessions still need testing.
GitHub Actions has not been run by this agent. The installer is unsigned.

For playback analysis, use Settings → Audio / MIDI Devices → Speakers / system
audio, then select the output device playing music. Windows WASAPI loopback
does not require a virtual microphone.
