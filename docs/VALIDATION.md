> Current 0.4.18 regression update: removed an unnecessary render-thread lock
> from routine control updates when no asset import is pending. Check processes
> now run independently of an open review instance; recording checks keep their
> preview on monitor #2. Added a live transition soak, actual 480p-to-4K native
> render-target checks, native-engine FX image validation, and private staging
> version/hash/selection tests. The older independent-renderer pixel assertions
> are explicitly inapplicable to the native backend. See the workspace report
> `outputs/vel-0.4.18-comprehensive/REPORT.md` for results and coverage limits.
> Physical MIDI, OBS capture, clean-machine installs and cross-platform builds
> still require separate verification; this is not a guarantee of zero defects.

> Current 0.4.17 update: 239 UI checks passed (zero failures), including
> right-edge Done/Play, centered Multiple preset controls, migrated overlay default,
> Windows control-window capture affinity 0x11 in all three modes, output affinity
> 0x00, shape holes and shared engine/MIDI state. Native saved-layout navigation,
> compact toolbar, dialog, cross-window Multiple and Performance Play probes passed.
> The shaped overlay avoids the observed error 8 rejection of per-pixel transparency.
> Manual OBS recording verification remains pending.

> Current 0.4.16 update: replaced region-clipping of the VST3 wrapper with a
> dedicated native child viewport confined below GRID's toolbar. The engine
> retains its full editor dimensions, native control callbacks and scene resolution.
> Fixed native peer lifetime during pop-out docking and shutdown. All 225 UI
> checks passed. Four compact-header cases verified physical viewport bounds,
> zero toolbar pixels overpainted, native save/cancel/browser and playback.
> Native dialog, cross-window Multiple and Performance Play checks all exited 0.
> Automated desktop capture was unavailable; manual visible-window and OBS
> verification remain pending. Reports: outputs/vel-0.4.16-*-verified/.

> Current 0.4.15 update: 225 quiet-desktop UI checks passed, including actual
> hit testing across Text motion controls and consolidated native toolbars.
> Native compact-header, Multiple save/cancel/browser, Performance Play, dialog,
> cross-window Multiple and 23 Text/copy scenarios passed. Multiple saving wrote
> native MultiplePreset XML with four tile children in an isolated profile.
> Black-rectangle-on-entry work remains deferred to the next user work session.
> Desktop file choosers/Explorer and oversized windows remain skipped during
> monitor #1 use. Hardware/OBS and manual desktop review remain pending.

> Current 0.4.14 update: 222 quiet-desktop UI checks passed; 23 offscreen
> native Text/color scenarios passed, including all offered styles, line selection,
> edits, font import/removal and category returns. Fourteen audio test groups and
> core control tests passed. Native dialog, Performance Play and cross-window
> Multiple checks passed. Native desktop file choosers/Explorer and oversized
> windows were skipped during monitor #1 use. Manual desktop/OBS review remains.

> Current 0.4.13 update: 218 quiet-desktop UI checks passed, plus live native
> dialog coverage/restoration in both modes and cross-window Multiple rendering.
> Native file choosers/Explorer and out-of-monitor sizing were skipped while
> monitor #1 was in use. Manual desktop/OBS review remains pending.

> Current 0.4.12 update: 217 UI/menu checks passed, including fresh Home-to-
> Performance/Multiple entry without Lab or a resize. Host scene GL attachment
> now follows the actual context target. Earlier native-frame tests did not
> include the overlapping host GL child. Manual on-screen/OBS review remains.

> Current 0.4.11 update: 211 UI/menu checks passed, including the Performance
> setup/playback toolbar, pop-out/docking, graph-first Mapping and separate Bindings.
> The 0.4.10 native-renderer results below are historical checks of the unchanged
> renderer. Manual OBS review and creator packaging remain pending.

> Current 0.4.10 status: 199 component/menu checks, core audio/MIDI tests,
> keyboard/MIDI mapping checks, native panel framebuffer captures and focused
> graph/Performance playback checks passed. Bindings measured 0.06–0.10 ms.
> Manual visual/OBS review and creator ZIP remain pending. Historical results
> below refer to earlier builds; see [current integration](ORIGINAL-ENGINE.md).

# Native private preview validation â€” 0.3.0 (2026-10-13)

Windows MSVC Release standalone compiled with `GRID_USE_PRIVATE_ENGINE=ON`.
Native renderer frames are displayed beneath GRID's UI, using the adjacent engine
copy in Sandboxie. No installed engine or preferences were modified.

| Check | Observed result |
| --- | --- |
| Core audio / MIDI CTest | 2/2 passed |
| Runtime suite | 96 checks passed |
| UI / Settings suite | 177 checks passed, including disabled unsupported native settings |
| Native Default profiles | 87/87 generated from the original setup/state defaults |
| Full native GPU sweep before final import repair | 125 captures; failed the new Scope trace-state assertion |
| Final image/font transition regression | 4/4 GPU captures passed; Scope image entry 15 and native contour count 1 verified |
| Earlier native full sweep | 124/124 captures passed PNG and shader checks; import state assertion was not yet present |
| Source / Python / dependency checks | Passed |
| Installed EXE, VST3 and preferences | SHA-256 unchanged from pre-integration baseline |

The full sweep exposed a mode-transition bug: the native stock selector replaced
the reserved image entry with Octa. Imports now wait for the native category,
reimport when moving the same asset between modes, and retain reserved image
entries on the render thread. The regression includes simultaneous image/font
import. The supplied OTF is used only in this test; it is not bundled or a default.

Automatic approval review blocked launching the final full sweep with "blocked by
policy", without a detailed reason. It has not been reported as passed after the
last repair. No alternate launch route was used to bypass that rejection.

Actual native GPU recording produced 720-frame, 12-second 1920x1080/60 MP4 files
with the installed software encoder. The final heavy run recorded 28 queue drops,
257 preview ticks and a 456.8 ms maximum timer gap. Missing frames are repeated to
preserve file duration: these results are not a sustained 60 FPS performance pass.
The preview stages no recording encoder; testers select an existing FFmpeg binary.

Runtime/UI assertions include some retained independent-renderer CPU fixtures;
only the GPU sweeps and asynchronous recording run the copied scene renderer.
These checks do not establish exhaustive control/state fidelity, real MIDI hardware
compatibility, clean-machine activation, sustained performance, native DAW hosting
or installer correctness. Geometry, bloom kernel and tracking quality remain
unavailable in the copied engine and are disabled. See ORIGINAL-ENGINE.md and
PRIVATE-TESTING.md for the current scope.

The following records describe previous independent-renderer revisions and are
historical, not additional native-engine acceptance results.

---

# Validation record (0.2 development revision)

Windows MSVC Release builds of standalone, VST3 and CLAP have compiled locally.
Core audio/control tests and a CLAP host smoke test pass. Runtime checks exercise
strict MIDI types, binding/state persistence, preset saving/recall, contour extraction,
Windows playback capture and MP4/MKV recording using the installed encoder.

Internal renderer checks produced 109 PNGs: nine category samples, nineteen FX profiles,
all seventy-four offered templates and seven Text effects, with no reported shader error.
The same checks also passed with the optional local reference-font library loaded.
The expanded component audit passes 165 checks across Settings and Lab. It exercises
all 21 contextual Lab knobs/swatches/toggles, range clamping, fine adjustment, MIDI
selection, context menus, navigation, presets, lyrics, image removal, save validation,
state files, native file-chooser open/cancel, folder launch and recording configuration.
Every graphics tier changes the actual submitted scene size, detail and frame cap.
Advanced toggles write the correct on/off endpoint values. Settings Back is inside the
modal panel and can receive input; settings pages have no Done button. Standalone
checks switch the actual WASAPI/hardware callbacks, exercise failure rollback and
mount the device selector with working analysis mute. Audio device selection on all
hardware models is not established by these checks.

The synchronous recording-button checks inject a frame fixture while the native event
loop is paused. Separate asynchronous GPU recording checks validate real capture.
FFprobe confirms the UI-configured fixture output is 640x360, 24 FPS and 24 frames.
The user-supplied logo and OTF file import in optional local asset checks.
They are import examples only, not bundled default assets or font-design references.
Thirty-two dedicated FX captures pass sixteen image checks for zero-value bypass,
amount response, frame/translation correlation, Grain exposure response, RGB dispersion,
refraction, scene preservation outside Glass, distinct Glass shapes and animated Symbol glyphs. Run `--fx-check <folder>`, then
`python tests/fx_image_checks.py <folder>` with Pillow and NumPy installed.
These are behavior checks; they do not establish exact agreement with vendor effects.

Center-position checks cover all nine modes at three window sizes. Runtime checks
also verify visible glyphs in every Text style at 320x180 and exclusion of contours
from Text rendering.

Actual 12-second 1080p/60 GPU recordings with all four FX and bloom produce 720-frame
files using installed software and NVIDIA encoders. Queue drops remain under this
heavy setup; repeated frames preserve timing. This is not a sustained-performance
pass. See REVISION-STATUS.md for observed counts and remaining work.

These reports apply to the revisions actually tested. Later changes require new checks.
They do not establish exact visual fidelity, real hardware MIDI behavior, long recording
stability, actual DAW compatibility, sustained NVENC performance or native installer correctness.
The revised macOS/Linux/AU builds and source-built encoder workflow have not run.
See REVISION-STATUS.md for remaining gates and the local compiler-package policy rejection.

## Historical foundation results (0.1)

The following original record describes the smaller foundation revision. Its Linux,
sanitizer and packaging results do not validate the new 0.2 implementation.

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
