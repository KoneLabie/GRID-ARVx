# Vel Engine integration — 0.4.18

Current delivery: native Windows x64 standalone with its adjacent engine, built
with the Windows-only workflow in `.github/workflows/build.yml`. The private
source archive includes the four required engine files through a Git allowlist.
Linux/macOS are deferred. The workflow requires a private repository. Sections
describing earlier revisions below are historical; see WINDOWS-RELEASE.md for
the current packaging and installation procedure.

Vel Engine is GRID's integration name for the authorized Greyland Audio Visual Lab
Pro 1.2.0 Windows VST3 runtime. It retains the compiled renderer, FX, defaults,
contour extraction, Multiple panels and Performance setlist/live panels. This is
binary integration, not recovered original C++ source. No source or SDK was supplied.
Provider metadata and copyright attribution remain intact.

## Runtime and storage

Configure Windows with `-DGRID_USE_VEL_ENGINE=ON` and build `GridARVxApp`.
Supply the authorized bundle at `vendor/VelEngine/Windows/x64/Vel Engine.vst3`;
the private source package includes only the four required bundle files, allowed
by the repository ignore rules. The build places it beside the EXE at
`Engine/Vel Engine.vst3`. The bridge loads this bundle only, checks its loaded
location, SHA-256 and version-specific interfaces, and rejects the installed
Visual Lab Pro locations. The original installed EXE/VST3 are never patched.

Module SHA-256: `110757113effef4c55d033904b9819a856a6c57f0749dc7b4809740e6c0a6b59`.

Named preferences and library file operations in this loaded module are redirected
from the provider's two AppData roots to `%APPDATA%/Cenaceae/GRID-ARVx/VelEngine`.
Native presets and setlists live under `VelEngine/Library/Presets`. This is separate
from GRID Lab's per-mode JSON preset folders. File imports outside those roots pass
through. In-memory import-table hooks affect this module only. The temporary
Sandboxie launch requirement and workflow visibility guard have been removed.
No license/personal preference files are imported. The 0.4.4 creator-review UI omits
the native activation card and input gate only in its private bundled instance,
under the owner permission reported by the user; installed files stay unchanged.
Activation on a clean testing machine remains unverified.

## UI and MIDI

Lab presents the native scene framebuffer underneath GRID's centered controls.
Multiple and Performance embed the native panels beneath GRID's toolbar. Native
preset selection, grid layout and setlist controls remain owned by their panels.
MIDI banks are independently stored for Lab, Multiple and Performance; old mappings
migrate into Lab. Switching workspaces preserves the other banks. MIDI learning
confirms physical Knob/Button type. Right-clicking FX still opens profile choices;
left-clicking a profile during learning selects its binding target.

Native workspaces show a binding strip when MIDI learning is active. Performance
includes previous/next, strobes, blackout, automatic sequencing and bypass. Multiple
has a toolbar tile selector for its current MIDI target. Right-click MIDI to clear
only the current bank. Native keyboard handling is forwarded to the active panel.

## Graphics and audio

Visible preview settings are fixed render resolution (480p through 4K), frame cap,
filtered sampling, VSync, menu rendering pause and FPS/dimension display. Four unsupported internal quality controls are
removed: geometry detail, bloom kernel, tracking interval and tracking resolution.
Overall quality selects resolution, frame cap and sampling settings. Native panels
render their scenes through a scaled framebuffer, then compose their UI at full
resolution. Original fixed internal geometry/FX quality remains unchanged.

Microphone/interface and WASAPI speaker capture feed the same native audio engine.
GRID keeps audio device selection, recording, lyric interval and custom color controls.
Recording resolution remains independent of preview scaling; FFmpeg is selected
separately. TXT import is available in Lab Text; native panels use their original
text editors. Lab JSON presets and native panel preset/setlist formats remain distinct.

## Review status

The 0.4.6 MSVC Release build passed. Focused native panel regression checks follow
the reported 0.4.5 crash; broader manual review is pending. New native-panel integration,
profile routing and MIDI bank transitions await the user's manual review. Automated
regression tests and the creator ZIP follow that review, as requested. Older 0.3.0
results are historical and do not validate these changes. No upload/email occurred.

## CRT/input recovery (0.4.2)

Lab's host is a fixed-size offscreen Component with a temporary, noninteractive
native peer; it no longer creates a taskbar DocumentWindow. Native component
painting is disabled while this hidden view renders Lab scenes, avoiding the
message-manager lock used for UI composition. The scene uses its own scaled FBO
and temporary render-only geometry; native component bounds are restored after
every frame. Multiple/Performance retain their visible native UI painting.

The earlier relative/adaptive scaling controls are retained only for state compatibility;
0.4.2 uses explicit scene dimensions instead. Accepted incoming MIDI changes
are shown in GRID's status line to diagnose unexpected control movement.
Build passed; CRT-at-maximum input responsiveness awaits manual retesting.

## Fixed resolution and performance (0.4.2)

Resolution choices: 854x480, 1280x720, 1600x900, 1920x1080, 2560x1440,
3200x1800 and 3840x2160. The selection configures the scene framebuffer before
native rendering. Lab's render-only geometry matches these dimensions; native
panels use the selected target with their existing layout coordinates. Presentation
then fits this scene into the window, with UI drawn at its normal window resolution.
This is not high-resolution scene rendering followed by a lower-resolution capture.
Fixed internal vendor FX buffers and native geometry detail remain unchanged.
Recording output size is independently selected and can scale the chosen source.

Legacy adaptive/relative-scale values cannot alter the explicit resolution.
Graphics quality presets select one of these resolutions plus frame cap/filtering.
All resolution/menu-pause preferences are global and excluded from visual presets.
General Settings only has the quality selector and centered submenu buttons;
audio source selection remains in Audio / MIDI Devices.

Performance changes: optional rendering pause in menus, no CPU frame readback for
visible native panels unless recording, reusable BGRA readback storage, reusable
GPU display textures, no upload for unchanged native frames, and repainting dial
widgets only when their value or appearance changes. The FPS display reports
native frame timing and actual framebuffer dimensions. These changes do not claim
a measured FPS improvement yet: manual review and automated regression are pending.

## Capture controls and mode entry (0.4.3)

The pop-out icon between fullscreen and Hide UI activates a transparent owned
control window aligned to the main editor. Windows display affinity excludes
that top-level control layer from supported capture methods; the main output
window stays capturable. Existing widgets are reparented and continue controlling
the same Processor, native engine, MIDI banks, presets and recorder. Right-click
chooses a separate movable controls window. Performance retains its native UI.

Hide UI hides all controls, including its own button. Clicking the output or
pressing Escape restores them. The overlay follows main-window movement/size
and minimizes with it. OBS should capture the main GRID-ARVx window, not the
controls window. Verify with OBS's Windows Graphics Capture method; capture
compatibility and FPS impact remain unmeasured. This is a visually integrated
layer, not a second visual engine or a promise of one operating-system window.
See https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity.

Multiple/Performance were connected, but mode entry dereferenced embedded native
TextEditor objects as pointers. Correct entry uses the embedded Component address
and its native virtual visibility method, matching the original entry routine.
This addresses the identified access-violation cause; runtime retesting is pending.
Automated UI regressions include same-processor control movement and hide/restore.
They are prepared but not yet run, following the requested manual-review order.

## Native activation and window modes (0.4.4)

The saved Binary Ninja constructor export (crt-ui-0x18053ae70.txt, lines 301-334)
and native mouse-handler disassembly (vel-mode-mouse-down.asm, lines 12-22)
confirm that the activation card/input gate is controlled by editor +0x3190.
For the owner-authorized private creator-review integration reported by the user,
the host clears that in-memory UI state and hides the embedded code field via its
native visibility method. It does this after verifying the adjacent module's
fingerprint and native editor layout, before presenting any workspace.

This change affects only this process's bundled Vel Engine instance. No installed
Visual Lab Pro binary, license text, activation file or personal preferences are
read/imported/modified. Engine bundle bytes remain unchanged. The creator receives
the explicit integration source and can replace this review behavior with their
own approved distribution/activation system. No public release is prepared.

The fullscreen button cycles windowed -> borderless window (green) -> fullscreen
(red) -> original windowed bounds (grey). Borderless retains the window's size;
fullscreen fills its current monitor. Escape returns to windowed unless restoring
hidden controls first. The capture-excluded control overlay retains ownership after
native window peer recreation. Manual capture/fullscreen verification is pending.

The standalone --monitor=2 launch option selects the Windows DISPLAY2 work area
before showing the main window. It fails without displaying a window if that
monitor is unavailable; it never falls back to monitor #1 for that request.

## GRID navigation and panel theme (0.4.5)

The native editor's per-instance virtual table retains its original RTTI and
all unaffected methods. Verified slot 4 (mouse down), slot 32 (key handling)
and slot 30 (UI paint) wrap the original handlers. A native return-to-menu
request immediately calls the host's GRID menu action on the message thread.
A paint guard prevents the provider menu from being drawn before any deferred
return reaches the host. This removes the old timer-only navigation path.

PanelTheme scopes a temporary context table to native UI painting only. The
verified setFill / drawRoundedRectangle / fillRoundedRectangle slots are
17 / 34 / 35. Solid UI colours use GRID's charcoal backgrounds, muted labels,
white titles and green selection accent. Gradient/image fills are preserved.
Button outlines gain Lab's charcoal fill, 10px corners and 1.2px border. Native
menu headers redraw MENU with matching Lab dimensions and a basic Arial Bold
font; native constructors/destructors maintain resource ownership. The engine's
scene shader passes, FX, preset previews and MIDI routing are untouched.

Binary Ninja analysis for this change is saved under outputs/binary-ninja-fx:
ui-restyle-vtable.json, ui-theme-0x1802a6e10.txt, ui-theme-0x1802a3d70.txt,
ui-theme-rounded-fill.txt and ui-theme-text-draw.json. The source-compatible
context wrapper copies the native table and restores it before returning;
no native executable code bytes or installed files are modified.

Runtime navigation, panel appearance, recording navigation and OBS review are
pending manual verification. No automated UI/GPU sweep or creator ZIP ran here.

## Native menu font crash (0.4.6)

0.4.5's drawMenu incorrectly called native FontOptions constructor 0x22fff0 with
storage for an 8-byte Font pointer. Binary Ninja confirms this constructor writes
104 bytes, corrupting adjacent stack storage. The corrected code creates aligned
104-byte FontOptions storage and constructs the pointer-sized Font using 0x22faf0.
That by-value native constructor consumes/destroys the options; the host does not
run a second options destructor. Native Font and String lifetimes remain native.

The development probe has a --panels check: a temporary offscreen host and isolated
EngineProfile open and paint Multiple/Performance twice each, then invoke the
actual native MENU handler and verify synchronous host navigation. No OS mouse
input, user's live controls, full FX sweep or original installation is involved.
UI painting is counted only after native painting and the styled MENU return.
The exception dump from 0.4.5 recorded fatal-app-exit 0xc0000409 / parameter 7.
No dump or personal state is included in a delivery package.

## Performance viewport and mappings (0.4.7)

Native Play routine 0x5256d0 enters screen 5, then 0x47fe70 requests a 600px
square editor. The host previously accepted that size beneath its full-width
GRID toolbar. The per-instance resized interface slot 39 (0x411680) and adapter
ComponentListener now preserve the host viewport before VST3 resize negotiation.
Play and Back are checked at 960x620 and 1280x720.

During live Performance, the editor moves to the stable offscreen host and its
component painting is disabled. The native scene-only layout renders directly
into the selected resolution framebuffer; GRID presents it through the same
Canvas used by Lab. Native scene/preset state is retained. No native control
preview or resize handle is composited into this display. Escape/Back restores
the native setlist panel to the current host viewport.

The setlist's Key Mapping button is drawn as MAPPING at its original hit region
(104,14,128,34). Its screen-4 request opens GRID's modal Mapping panel, while the
live Performance toolbar replaces MIDI with MAPPING. Bindings opens a scrollable
Action / Keyboard / MIDI table containing the 32 native shortcut actions.
Keyboard remapping redirects to each original engine action and unbinds any
previous owner of a duplicate shortcut. MIDI action buttons require confirmed
Button hardware and values 0/127; held Strobes/Blackout receive releases.

Performance keyboard and MIDI action bindings persist in GRID state. Its MIDI
action table is independent of both Lab and Multiple CC banks. Existing continuous
control MIDI assignments remain in their workspace banks. Only the copied
module's GetAsyncKeyState import is adapted for the two held Performance actions;
physical keyboard state and the installed module are unchanged. Keyboard holds
are sampled only while GRID is foreground; MIDI holds may operate in background.

Focused mapping tests cover persistence, duplicate assignment, press/release,
knob rejection, and workspace isolation. The --play development probe uses the
actual native Play and Escape handlers, scene-only rendering, and held-action
parameter observation. The --panels probe checks native paint, MENU, and MAPPING
callbacks without OS input. Broad regressions, OBS capture and creator packaging
remain pending manual review.

## Keyboard graph restored (0.4.8)

Mapping opens the original visual keyboard diagram, drawn by the pinned native
472d50 routine into its own native UI surface inside the GRID Mapping frame.
The graph remains a read-only legend. Host Strings/fonts/Graphics are not passed
into the native drawing code, and opening the diagram does not change the native
setlist or live-screen state. The scene framebuffer is captured before native UI
composition, so recording continues to contain only the visuals.

BINDINGS opens a separate editing frame with the existing scrollable three-column
table. BACK returns from Bindings to the graph; BACK on the graph closes Mapping.
Native editor ownership switches between the graph region and the offscreen host
when navigating these frames, preventing its child HWND from covering the list.
The --graph focused probe paints the original diagram twice and checks return to
the native panel and GRID home. Play/Back and held-action checks remain focused;
broad regressions and creator packaging remain deferred until manual review.

## Stable frames and window Close (0.4.9)

Mapping now caches the original diagram as a host-owned ARGB bitmap. Native
SoftwareImageType/Image (29f980/29f660), software renderer (2a2e10) and graph
472d50 preserve native font/resource ownership. Only pixels cross into GRID;
native destructors release native renderer, bitmap access and image references.
The borrowed Graphics view uses the verified 24-byte context layout.

The graph is generated once per size under a geometry lock and then displayed
by a normal JUCE component. Mapping/Bindings navigation no longer reparents the
engine or restarts its context. Stable showWorkspace requests return immediately;
partial workspace transitions do not render until the final layout is published.
Hidden renderers skip unnecessary scene blits and briefly idle between frames.
They never sleep in the frame-cap path while native UI painting holds its message
lock. Rendering resolution, presets and FX algorithms remain unchanged.

EmbeddedFrame allows modal events to its owning top-level window and the app's other DocumentWindows. JUCE's
Windows WM_CLOSE handler previously discarded Close while an embedded frame
was modal. Underlying controls remain blocked; native Close/Alt+F4 can now reach
normal shutdown. The existing 0.4.8 instance was closed through its normal quit
handler without terminating its process.

Focused checks cover the modal gate, underlying-control blocking, cached native
graph pixels, unchanged viewport/context count, MIDI persistence/isolation and
Play/Back. The diagram's Shift+number shortcuts for tracks 11–20 are now included
in Bindings. Broad GPU/OBS regressions and creator packaging remain pending review.


## Native menu composition and dialog latency (0.4.10)

Native menu frames now bypass the scene-resolution framebuffer. Scaling a menu
through that FBO could leave the right side of Performance covered by a black
rectangle. Lab, Performance playback and Multiple scenes retain scene-quality
rendering; control menus use the physical client viewport. EngineSurface restores
the original read/draw framebuffer bindings and viewport after presenting.

A native child surface stays hidden during layout/context changes until its new
control buffer has been painted. GRID paints a complete dark background while
that surface changes. Mapping hides the native GL child without reparenting its
editor or toggling component painting, preserving the live graphics context.

Bindings obtains all keyboard/MIDI labels in one snapshot instead of parsing
JSON and scanning every MIDI slot once per row. The 199-check component audit
measured Bindings at 0.06–0.10 ms over three final visits, and Mapping at 11.89 ms cold,
0.58–0.63 ms cached. A separate live-loop test covers hiding/restoring the native
surface without changing its context or viewport.

The native panel probe reads the actual front framebuffer after component UI
composition. It checks that right-side controls are present at 1120x658 and
960x620, saves those images, and exercises native Menu/Mapping callbacks.
Performance Play/Escape checks retain requested 640x360 scene rendering with
960x620 and 1280x720 host viewports, plus held Strobes/Blackout release behavior.

These checks establish the tested paths, not every populated setlist/tile workflow,
physical MIDI controller, long session, transient desktop-compositor frame or OBS
capture behavior. Those remain manual review items. The installed program,
plugin and preferences still match their original SHA-256 baseline.

Windows Application logs also recorded a 0.4.9 access violation in
UIAutomationCore.dll during the desktop accessibility/capture attempt. The
origin of that older-build automation crash is not established. The 0.4.10
component/native-frame tests produced no new Windows crash/hang reports; they
use app-owned callbacks and framebuffer reads rather than desktop input.


## Performance toolbar and shared control layer (0.4.11)

The pre-play header displays only the centered PERFORMANCE bar. Native setup
controls remain below it. During playback, Menu/Mapping and Fullscreen/Pop-out/
Hide UI are available; the recording dot is absent throughout Performance.

Pop-out uses GRID's existing capture-excluded overlay or separate controls window,
with the same processor, native engine and Performance binding bank. Returning
from playback automatically docks that layer. Home also docks it and switches the
native engine out of playback before the next Performance entry.

The setup Mapping callback and playback Mapping button both create the same
fresh graph-first frame. Bindings is opened only through that frame's BINDINGS
button; Back restores the graph. The visual diagram is retained.

The updated component audit passed 211 checks. Playback presentation tests invoke
its real toolbar callbacks with playback presentation active, both docked and in
the control layer; the native Play/Escape engine path was checked separately in
0.4.10. OBS capture and live performance sessions remain manual review items.


## First-entry host GL attachment (0.4.12)

A remaining black rectangle could disappear after resizing the main window.
The host Canvas could be invisible while its OpenGL context was still attached
to the whole Editor. Startup explicitly attached that context even on Home,
and updatePresentation compared requested rendering against Canvas visibility.
Entering Performance/Multiple directly from Home could therefore leave the
extra host GL child over the native editor.

Startup now calls updatePresentation instead of unconditionally attaching UI
rendering. Presentation compares the actual OpenGL target component against the
requested target (none, scene Canvas, or Editor). Home and native setup menus
have no host scene context; Lab and Performance playback attach it as needed.
No window-size nudge or native shader/engine change is involved.

The earlier native front-buffer captures did not include the host's overlapping
GL child, and the component suite visited Lab before the native menus. The new
regression enters Performance and Multiple immediately after Home, without Lab
or a resize, verifies that the host context is detached and bounds stay fixed,
and then verifies that Lab attaches it normally.

The updated 217-check UI audit passed, including the direct initial menu paths,
normal Lab attachment, Performance pop-out and graph navigation. Asset-drop and
render/record diagnostic entry paths also explicitly refresh scene presentation.
The automated attachment checks do not replace on-screen confirmation of the
reported compositor artifact.


## Native dialog occlusion and real detached controls (0.4.13)

Lab is host-drawn; Multiple and Performance embed a native child-window tree.
Hiding only its GL leaf left the enclosing plugin window above GRID's software
Mapping frame. That window could paint solid black even while the frame, graph
and Bindings controls were logically visible.

The bridge now identifies the native child container underneath its known host
peer. Mapping/modal coverage hides that whole child tree, never the host peer,
and restores it on exit. Parent traversal is bounded by the host HWND and the
render window's process identity. The renderer instance, editor object, native
callbacks and parameter/MIDI routes remain intact.

Live-loop native checks assert that the complete container is hidden while a
GRID frame is shown, the host remains visible, and the native editor is restored
afterwards. These checks cover both Multiple and Performance. A separate Multiple
check moves the same renderer between two native host windows while retaining
its requested 640x360 scene dimensions and native control container.

Normal Pop-out now opens a separate titled controls window. The capture-excluded
in-window overlay remains in the right-click menu, and its choice is persisted
as controlsPresentation. Detached controls open on the main window's monitor,
with bounds constrained to that display. They retain the same Processor, Vel
Engine instance, workspace and bindings; no second visualizer is created.

The quiet-desktop diagnostic option keeps sizing tests within the target monitor,
skips Explorer/native file-chooser launches and suppresses explicit focus grabs.
It is enabled only for these local UI checks, not normal app behavior.

The quiet-desktop pass completed 218 checks, including separate/docked controls,
Multiple MIDI Learn toggling and binding preservation, graph-first Mapping and
Bindings navigation. It produced internal Mapping/Bindings renders. Live native
checks passed for dialog container visibility/restoration in both modes, native
Menu/Mapping callbacks, Play/Escape, and Multiple's renderer moving between two
host peers. These are targeted integration checks, not an exhaustive populated
setlist, hardware-MIDI, Windows compositor or OBS session test.


## Text raster preparation and audio cycles (0.4.14)

The Text renderer is hybrid. Native editor paint (`+0x5137d0`) builds the styled
lyric image at editor `+0x1fa8`; the native GL Text scene then uploads it and applies
its effects. Disabling component painting for GRID Lab also disabled that producer,
so most Text styles displayed only their background. The bridge now runs the native
producer against a clipped 1×1 software target before the native GL scene. No original
UI is composited. Native fonts/images retain native ownership. Import and clear paths
serialize with rendering to avoid freeing an image or typeface during text preparation.

Beat onsets now use 10 ms windows of incoming stereo audio with an adaptive energy
threshold and retrigger holdoff. Counts use sample time and survive multiple onsets
between UI refreshes. Playing DAW transport BPM remains authoritative when supplied;
standalone audio uses detected onsets. Beat intervals remain 1–128 beats and lyric
seconds timing remains separate. This detects transients, rather than promising an
exact musical grid for every possible song. Automatic preset recalls retain enabled
cycles/intervals and run before color/lyric advancement, so they cannot disable or
overwrite a color change due on the same beat. Manual preset recall keeps saved states.

Targeted offscreen checks cover all native Text styles, active and edited lyrics,
text/background colors, a basic test font import/removal, FX, category returns and the
paired secondary palette. Audio unit fixtures cover quiet/loud kicks at 80/120/180 BPM,
44.1/48/96 kHz and delayed UI cadence. UI checks exercise PCM analysis through the same
cycle advancement used by the timer. See the per-build reports for measured results.

The active lyric is also sent through native parameter 20 (`TEXT BOX`, 0–74)
and its cached index `+0x1d34`. Previously setting that cache to −1 let the native
timer restore its old text-box parameter and force the display back to line 0.
The final 23-scenario report verifies both actual glyph output and selected index;
font import checks repeat the normal update loop so deferred imports are dispatched.

Verified 0.4.14 outputs: `outputs/vel-0.4.14-ui-final/ui-checks.txt` (222 PASS, 0 FAIL),
`outputs/vel-0.4.14-text-final/report.json` (23 PASS), and the native `dialogs-final`,
`play-final`, `multiple-final` reports. Installed EXE, VST3 and settings hashes still
match the baseline. These are focused regressions, not an exhaustive music corpus
or hardware/OBS/clean-machine certification.


## Native viewport and toolbar occlusion (0.4.16)

A transparent region on an enlarged VST3 wrapper was insufficient: its JUCE
ViewComponent paints black and the Windows peer excludes the entire child HWND
rectangle when painting the host. Both could obscure GRID's toolbar even when
native region checks passed. The native viewport is now a separate WS_CHILD peer
whose actual bounds start below the 82-pixel toolbar. The complete VST3 wrapper
is offset inside that viewport to crop its original 60-pixel header. Parent-client
clipping protects both host painting and input without changing native callbacks
or framebuffer resolution. The same path serves Multiple and Performance.

The plugin editor is detached before a pop-out controls host is destroyed, and
its GL thread is stopped before either parent peer is destroyed at shutdown.
Viewport peers are removed after reparenting the editor into its hidden render
host. This avoids Windows destroying plugin child HWNDs during a live render.

Validation: 225 UI checks; four native compact-toolbar cases at two sizes with
actual HWND/client geometry and zero host toolbar pixels overwritten; native
save/cancel/browser, Mapping coverage/restoration, cross-window Multiple and
Performance Play all passed with normal process exits. Desktop WGC capture
was unavailable in this session, so final visible-window/OBS review remains manual.


## Restored preset row and capture overlay (0.4.18)

Multiple has a centered host PRESET row in the original position below the main
header, and its native content viewport starts at y=142 while editing the layout.
The original native header stays cropped. Preset selection opens the native browser;
Previous/Next invoke the original header rectangle handlers, which call the native
Multiple preset loader. Native probes save two layouts, navigate both directions,
and verify selection and grid restoration. Done and Performance Play are anchored
to the right edge independently of the centered mode title.

Existing profiles migrate once to the in-place capture overlay. It is an owned,
frameless top-level control layer aligned with the main client area, sharing every
existing control and processor. A normal redirected window with a cached region
around controls replaces per-pixel alpha. On this machine the latter rejected
SetWindowDisplayAffinity with error 8; the shaped layer accepts exclusion and
GetWindowDisplayAffinity reads 0x11 in all three modes. The main output retains
0x00. Region holes, native parent ownership and bounds are checked separately.

Primary Windows references:
- https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity
- https://github.com/microsoft/Windows.UI.Composition-Win32-Samples/issues/56
- https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#layered-windows

No OBS session or end-to-end recording certification is implied by these native
API checks. No original installed module or personal activation settings changed.
