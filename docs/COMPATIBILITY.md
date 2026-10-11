# Build targets and compatibility boundaries

| Format | Platforms | State |
| --- | --- | --- |
| Standalone | Windows x64, macOS universal, Linux x64 | Included |
| VST3 | Windows/macOS/Linux | Included; host testing needed |
| AU | macOS | Included; `auval` and DAW testing needed |
| CLAP | Windows/macOS/Linux | Included via pinned adapter; host testing needed |
| VST2 | Optional future | Disabled; no authorized SDK or wrapper included |
| AAX | Optional future | Disabled; SDK agreement, signing/testing, wrapper work pending |

Enabling the reserved VST2/AAX options intentionally produces a clear CMake
error. It never silently creates a mislabeled plugin. Pro Tools cannot load the
default formats as native AAX. There is no licensing-free native AAX/VST2
compatibility layer included.

All default builds use JUCE 8.0.4 pinned to commit
`51d11a2be6d5c97ccf12b4e5e827006e19f0555a`. CLAP's adapter is pinned to
`7adee3a1bd4684d4caa5601100e364abccff4b4d`, with its own pinned submodules.
These are deliberate prototype pins, not claims to use the newest versions.

Suggested host checks: mono/stereo buses, save/reload parameters, plugin scan,
opening/closing the editor during playback, and multiple plugin instances.
Keep live GPU, audio-device, installer, and DAW results separate from compile
success. Signed distribution and older-OS support require additional work.
