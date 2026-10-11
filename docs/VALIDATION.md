# Validation record — 2026-10-11

This package contains freshly written GRID-ARVx source. The reference screenshot
was used only to guide repository organization.

| Check | Result |
| --- | --- |
| Required files, Python syntax, pinned dependencies | Passed |
| Workflow YAML parsing | Passed |
| CMake core-only configuration / CTest | Passed |
| Ten audio-core checks | Passed |
| Address / undefined-behavior sanitizer checks | Passed with leak detection disabled; environment blocks LeakSanitizer |
| ThreadSanitizer producer/consumer stress | Passed |
| Linux native standalone, VST3, CLAP compilation | Passed on Ubuntu 24.04, GCC 13.3, JUCE 8.0.4 |
| CLAP minimal-host scan, automation, state round-trip | Passed |
| CLAP stereo pass-through | Passed, byte-for-byte unchanged samples |
| Linux component installer fixture integration | Passed |
| Actual Linux installer with compiled products | Passed; installed bytes match, notices present, installed CLAP loads |
| macOS package layout / selection XML | Passed with native packaging tools mocked |
| Windows native build / Inno installer | Pending GitHub Actions |
| macOS universal build / native pkg generation / AU validation | Pending GitHub Actions |
| Real DAW compatibility matrix | Pending testing in actual DAWs |
| Standalone live GPU/audio-device behavior | Pending; virtual display cannot create sockets in this environment |

The ten core checks cover empty input, sine amplitude, mono/stereo consistency,
invalid callback input, queue overflow/recovery, nonfinite samples, silence
decay, frequency-band response, common sample rates, and concurrent capture.
These are audio-core checks, not ten completed cross-platform acceptance tests.

The native Linux build here uses the pinned dependency checkouts. The workflow
will fetch them on GitHub's runners. No GitHub run has been submitted on the
user's behalf; remote builds cannot be marked as passed yet. Linux CI uses an
Ubuntu 22.04 baseline, so that runner still needs its own build result.

Before relying on a live show, verify each installer on its actual OS, plugin
scanning in the intended DAWs, editor resize/open/close during playback, state
restore with images/fonts, device permissions, and GPU behavior. Advanced modes,
FX families, timed lyrics, and native system-output capture remain future work.
