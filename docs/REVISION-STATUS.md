> Windows packaging update: the private 0.4.18 source package now includes the
> required engine files and a Windows-only native standalone Actions workflow.
> Inno Setup install, installed-app UI checks (241 passes) and uninstall passed
> in a separate local folder. Linux/macOS and plugins are deferred. The optional
> encoder source-build and GitHub workflow have not been run remotely. Historical
> sections below retain their original status; use WINDOWS-RELEASE.md for delivery.

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

# Development status â€” 0.3.0 private preview

The private Windows standalone now runs the copied original Visual Lab Pro 1.2.0
engine behind GRID's UI. Its native FX replace the reconstructed renderer in this
build. [Engine integration](ORIGINAL-ENGINE.md) describes the implemented bridge,
isolation and remaining limits. [Private testing](PRIVATE-TESTING.md) covers launch
requirements. No public upload, repository push or distribution has occurred.

The following record describes earlier independent-renderer revisions. It does
not describe the active native preview or establish native equivalence.

---

# GRID-ARVx 0.2 development status

This is an implementation in progress, not the final delivery ZIP. Source remains local; no repository push or workflow dispatch has been performed for this revision.

## Agreed scope

Nine aesthetic families: Oscilloscope, Lava Lamp, Interstellar, Code, Basic Colors, TV, Pixel, Text, Plasma. Arcade excluded. The custom home screen has Lab, Multiple, Performance, Settings; Multiple and Performance are disabled for later development. Company: Kone Labie.

Four FX groups: Texture (Noise, Grain, CRT, Thresh), Dither (Symbol, Star, Flower, Hex), Track (Basic, System, Bracket, Glow, Terminal), Glass (Ball, 2x2, Full, Flower, Cross, Burst). Each has amount, profile selection, and a reactive toggle. IN is analysis gain; audio output remains unchanged. INT scales visual response. Speed/Glow and colors are contextual. Color/preset Cycle intervals use beats; lyrics have a separate beat/seconds cycle.

All host parameters expose integers 0â€“127. MIDI Learn selects knobs/toggles with left click; right-click FX menus remain available, and choosing a profile arms that profile as a button action. After receiving a MIDI control, explicitly identify hardware as Knob or Button. Wrong types are rejected. CC button mappings accept endpoint values 0/127 and trigger once per press. Note messages can bind button actions, never knobs. Bindings survive complete state recall and are suspended by the processor's Performance flag; the deferred Performance UI does not yet activate it.

Per-category Defaults, Factory, User folders use JSON. Factory begins empty. User save requires a valid name and explicit overwrite confirmation. Preset assets are copied into a relative Assets folder. Complete host/standalone state includes MIDI mappings. Graphics preferences are global and are not replaced by a visual preset.

Text includes editable numbered lyric rows, scrollable lists, per-row deletion, add/clear, font import and image backgrounds with IMG scale/clear. TXT import accepts nonempty lines, rejects over 120 characters on any line or over 128 lines, and does not silently truncate an imported file. This 120-character limit is an implementation default open to adjustment. Text effects: Warp, Expand, Spin, Glitch, Wave, Shake, Strobe. GIF import uses a still frame; animated GIFs are not enabled.

Latest user clarification: toolbar order is MENU â†’ MIDI â†’ Record â†’ mode. Text remains its own mode, renderer and preset folders; selecting it does not compose another visual mode underneath. The supplied blaster-display.otf is only an import example. Eleven recorded styles are offered: Simple, Hand, VHS, System, Tarophis, Pixel, Ransom, Noskale, Picnic, Flor, Myrtille. The earlier inferred Normal default is hidden while its numeric slot remains compatible. Basic system fonts provide distributable approximations; the user-supplied external extraction supplies actual style glyphs for the local preview. Those fonts are not bundled; see TEXT-FONTS.md. Edited defaults and User presets remain preserved when untouched autogenerated Text defaults are upgraded.

Centering clarification: the top mode and bottom preset/style selectors stay at the true client-area center. Text's effect selector and preset Cycle use the same center line. Side controls are placed around those anchors; adding controls or changing mode does not recenter the selectors by the group's total width. Checks cover all nine modes at three requested window sizes, using the actual client area when native borders constrain the maximum width.

Images in supported visual families use alpha/luminance contour extraction with internal holes. Lava Lamp and Basic Colors do not use dropped traces; Text draws an image background. Traces rotate and respond to audio. The drag overlay distinguishes tracing from image/font/text import.

## Current technical implementation

JUCE/C++20 host shell; software-rendered paths/glyphs/particles uploaded to OpenGL; procedural GPU scenes, separable bloom, Texture/Dither/Glass and luminance-region Track overlays. Current FX order is bloom â†’ Texture â†’ Dither â†’ Glass â†’ Track. Bloom/Texture/Dither are cached before refracted sampling. Noise and Grain regenerate per frame rather than translating a fixed field; Grain is monochrome and responds to scene exposure. Glass has aspect-correct lens shapes, Snell refraction, differing RGB refractive indices, surface reflections, with the original scene preserved outside the lenses at maximum strength. Symbol now dithers with randomized system-font letter/number glyphs instead of cross marks. This is our implementation, not a verified claim about Greyland's internals.

Standalone accepts hardware input/MIDI devices. Windows has a WASAPI default-playback loopback adapter. macOS/Linux currently use a user-selected monitor/loopback device for system sound; native adapters are not implemented. Plugin audio/MIDI comes from the DAW.

Recording uses a bounded worker queue and a separate FFmpeg process. MP4 and MKV software MPEG-4 are implemented. NVENC H.264 is checked on a background thread and falls back to software when unavailable. Filename, folder chooser, resolution, FPS, bitrate and duration are configurable. Capture preserves aspect ratio with black padding and locks the window size. Stop requests finalize on the worker; the automatic duration limit works even after preview frames stop. Missing preview frames duplicate the last frame to maintain output timing. Recording is visuals-only.

Installer staging now requires a source-built recording encoder, its exact source archives, build recipe and notices. `scripts/encoder.py` pins FFmpeg 7.1.5 and NVIDIA headers, verifies SHA-256, disables GPL/nonfree and external media dependencies, and tests raw RGB recording in both containers. Windows/macOS/Linux workflow integration is written but has not been run for this revision. Automatic approval review rejected downloading and running a portable MSYS2 compiler locally, citing policy without a detailed reason; this route was stopped. Local recording checks use the installed encoder, not the proposed packaged build.

Settings now apply graphics changes immediately and display native units. Back is mounted inside each settings panel and subpages return to Settings. Audio/MIDI devices are embedded in that navigation. The explicit audio-source selector reports current source and signal activity. Graphics tiers and overrides control frame cap, render scale, geometry/particle detail, bloom kernel quality, filtering, tracking rate, VSync, adaptive preview quality and frame statistics. These tiers still need benchmarks on multiple GPU classes. The antialias control currently selects filtered versus nearest sampling; it is not MSAA or DLSS. No DLSS toggle is implemented.

## Reference sources

- https://greylandaudio.com/products/visual-lab-pro â€” category and FX descriptions, public feature inventory.
- https://getvisuallab.com/ â€” category descriptions and FX profiles.
- https://www.youtube.com/watch?v=snVfZkvh6Uo â€” official walkthrough reviewed earlier.
- User screenshots and `2026-10-11 20-44-19.mp4`, `2026-10-11 20-55-38.mp4` â€” layouts, presets, asset behavior and effects. Entire 5:35â€“7:45 FX section of the first video authorized as reference.

Specific searches on KVR, Gearspace and Reddit did not locate useful discussions of this exact product. With the user's authorization, readable installed preset XML and their supplied resource extraction were inspected. The five product GLSL files concern glow, blur and lines; they do not supply Noise, Grain or Glass algorithms. One line-fragment extract appears incomplete. The user-authorized Sandboxie VisualLabProLab folder inspection found settings and driver GPU caches, with no additional readable FX shader sources. No recovered shader or native effect-engine code was copied into GRID-ARVx. Our rendering remains a new implementation; the optional local font library supplies external glyph data. Some templates and algorithms remain approximations requiring comparison before fidelity sign-off.

## Validation already observed on Windows

- MSVC Release standalone, VST3, CLAP compile.
- Audio core tests, including the fixed Windows heap fixtures.
- CC ranges/choice round trips, strict MIDI types and concurrent event queue tests.
- CLAP scan, parameter ranges, automation, state restore, activation, bit-exact passthrough.
- Runtime checks for binding persistence, note/button restrictions, Performance suspension, preset save/overwrite/path validation, locked recall, contour extraction, scene construction and an encoded MP4.
- 109 internal OpenGL captures: nine category samples, nineteen FX profiles, all seventy-four offered templates and seven Text effects, with no reported shader error. Both basic-font and optional reference-font passes succeeded. Windows captures read our app's actual presented framebuffer including the UI; recording reads visuals before UI composition.
- Thirty-two dedicated GPU FX captures passed sixteen image assertions. Maximum temporal/one-pixel translation correlation was 0.0084 for Noise and 0.0260 for Grain, rejecting the old scrolling-pattern behavior. Checks also cover exposure response, bypass at CC zero, amount response, refraction, RGB dispersion, preservation of the exterior scene, all six Glass shapes and animated character dithering.
- Runtime checks verify visible lyric glyphs in every Text style at 320x180, including corrected Pixel drawing and scaled System-style padding. Text ignores visual-mode contours. Text bloom preserves the selected glyph color.
- Actual GPU recordings at 1920x1080 / 60 FPS / 12 seconds with four FX and bloom finalize 720-frame files using software MPEG-4 and NVIDIA H.264 on the local RTX 3060. These establish encoding and timeline integrity, not sustained 60 unique frames per second. Earlier optimized software/NVIDIA runs dropped 183/212 queued preview frames. The latest revised-pipeline NVIDIA run dropped 354, with 622 UI timer ticks and a maximum timer gap of 578.8 ms. Missing frames repeat to retain duration. A subsequent Settings/FX revision run dropped 305, with 603 UI ticks and a maximum 578.9 ms gap. These variable results do not establish a performance improvement. Source-built packaged encoder and long-session stability remain unvalidated.
- Direct UI checks for empty/invalid save names, per-mode User saving, overwrite confirmation, lyric import rejection/edit/add/remove/clear, image removal, MIDI profile targeting, reset and dialog exports. The expanded 165-check audit covers every Settings control and all 21 contextual Lab controls, tier-to-renderer propagation, native-unit preferences, endpoint toggles, modal Back routing, actual audio-source switches, failure rollback, state file I/O, encoder selection, folder launch and recording start/stop. It also verifies the eleven offered Text styles, safe legacy-default upgrades and preservation of edited defaults. App CLI checks use an isolated state/preset folder inside their output directory. These exercise our app's components and do not use native desktop screenshots.

These observations refer to the checked development revisions, not to untested later edits or every possible user workflow. Mac/Linux/AU, real DAW sessions, hardware MIDI controllers, installer installs/uninstalls, and long recordings are not yet validated. Linux installer integration cannot run on this Windows host because Bash is unavailable; fixture staging succeeded before that limitation.

## Remaining delivery gates

Complete preset-by-preset visual comparison (including distinct template behavior and imported contours), refine Text styles/effects, test every interactive menu/dialog, stabilize recording and graphics under load, finish encoder packaging, run macOS/Linux/AU build checks, validate installers and actual DAW compatibility. Final source ZIP comes after these gates; no partial build is described as the finished application.
