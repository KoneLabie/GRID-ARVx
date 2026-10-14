# GRID-ARVx 0.4.18 manual review

Launch `GRID-ARVx.exe` from the Release folder with its adjacent `Engine/Vel Engine.vst3`
bundle. Sandboxie is no longer required. Packaged folders can use `Launch-GRID-ARVx.cmd`.
The previous 0.3.0 ZIP is obsolete. The 0.4.18 private source ZIP and Windows
installer are described in [Windows release instructions](WINDOWS-RELEASE.md).

Keep sharing within the permission reported by the owner: private, no resale,
reasonable friends testing, with the creator receiving the project. No original
license or personal configuration files are included. The private creator-review integration omits the native activation card and input
gate in its bundled engine instance under the owner permission reported by the user.
Clean-machine testing remains pending.

## Capture controls (0.4.18)

The capture button now defaults to controls aligned over the main output, without
another title bar or draggable controls frame. Existing profiles migrate once to
this default; a separate controls window remains an explicit right-click option.
Lab, Performance playback and Multiple reuse the same controls, engine and MIDI
banks. Capture the main GRID-ARVx output window in OBS; final OBS verification
is manual. The control layer's WDA_EXCLUDEFROMCAPTURE flag and the output's
WDA_NONE flag are verified independently in the native Windows UI checks.

The in-place layer uses a normal opaque redirected window shaped around the
controls, with holes revealing the underlying visuals. This avoids the measured
Windows error 8 rejection of JUCE's UpdateLayeredWindow transparency path.
Multiple's native editor has its own clipped child viewport within that owned
control layer. Its clean visual frames remain presented in the main output.

## First-entry regression

In 0.4.15, Performance setup has Menu, Mapping and Play on the centered title's
toolbar. Multiple has Done/Layout and a Save icon between the centered title and
tile selector. Use the centered PRESET row below the header to browse saved Multiple layouts
and tile visuals; its arrows navigate the original engine preset list. Done and
Performance Play sit at the far right. Check
the native preset save/cancel flow and body controls in docked and detached layouts.
Text motion selection should accept clicks across its entire button face.

The 0.4.16 viewport confines native windows below the toolbar in both modes;
check fresh entry, resizing, Mapping and pop-out/dock transitions. The saved
frame cap was approximately 30 FPS; Advanced
Graphics allows raising it without lowering resolution. The renderer was verified
to use the NVIDIA RTX 3060. No DLSS or other experimental renderer was enabled.

From a fresh launch, enter Performance before visiting Lab. Check the complete
setup menu without dragging a window edge or maximizing. Return Home and enter
Multiple; repeat after visiting Lab and returning from Performance playback.
The scene GL layer should never cover either setup menu.

## Manual review sequence

1. Settings: quality presets at the top and centered submenu buttons. Advanced
   Graphics: 480p–4K resolution, frame cap, filtering, VSync, menu pause, FPS/dimension
   display. Audio / MIDI Devices: source/device selection and mute. The four unsupported
   geometry/bloom/tracking controls should be absent.
2. Lab: each mode's Defaults, FX amounts/profiles/reactivity, lock/reset, saved
   presets, image tracing, Text image scale and bounded TXT lyrics. Fullscreen
   cycles borderless/green, fullscreen/red, windowed/grey and restores original
   window bounds. Escape restores windowed mode; Hide UI/click restores controls.
3. Creator-review UI: Multiple and Performance must enter their actual panels
   without a native activation card or stray license input field. The original
   installed program and its personal/activation files must remain unchanged.
4. Navigation and theme: in both native modes click their inner MENU/Back
   controls and confirm GRID's home appears without any provider-menu flash.
   Repeat from their subpages and with the separate/capture-excluded controls
   presentation. Compare the MENU button, dark surfaces, rounded button fill,
   border and selected state with Lab. Confirm native preset previews and FX
   colours remain unchanged.
5. Multiple: enter through the main menu, select a layout and populate its tiles;
   save/reload its native setup. Test graphics settings at 480p and 4K.
6. Performance: before Play, the top header shows only the centered PERFORMANCE
   bar; its native Menu/Mapping/Play and setlist controls remain below. During Play,
   the toolbar shows Menu, Mapping, Fullscreen, Pop-out and Hide UI, with no recording
   dot. Pop-out sits between Fullscreen and Hide UI and uses the same live engine.
   Returning from playback docks the controls. Both Mapping entry points open the
   keyboard diagram first; Bindings is a separate page and Back returns to the diagram.
   Create/open a setlist, add native presets, enter its live panel,
   advance presets and test native performance actions. The live visualizer should
   fill the window like Lab, without a 600px square, extra preview or resize handle.
   Escape returns to the setlist without resizing GRID.
7. MIDI: bind the same channel/CC differently in Lab and Performance. Switch both
   ways, confirm independence and restart to confirm persistence. Learn Knob/Button
   type matching; right-click FX then left-click a profile to bind it. Native panels
   show a binding strip while learning. Multiple's toolbar selects the affected tile.
   Right-click MIDI to clear the current bank only. In Performance use MAPPING,
   to see the original visual keyboard diagram. BINDINGS opens a separate
   editing frame; BACK returns to the diagram. In Bindings, scroll the
   Action / Keyboard / MIDI table, click a Keyboard cell
   and press its new shortcut (Backspace clears, Esc cancels), or click a MIDI
   cell and press a hardware button, then confirm Button. Remapping must stop the
   previous shortcut, preserve Lab/Multiple assignments and survive restart.
   Strobes and Blackout should stop when their bound button/key is released.
   Switch Mapping → Bindings → Back repeatedly: the graph should stay stable,
   and window Close / Alt+F4 should work while either frame is open. Also test
   window Close with Save, Record, Cycle and Settings frames open.
8. Capture controls: in Lab click the pop-out icon between fullscreen and Hide UI.
   Normal click opens a separate titled controls window on the same monitor and
   operates the same engine. Right-click selects the optional capture-excluded
   overlay; that choice persists. In OBS select the
   main GRID-ARVx window with Windows Graphics Capture and confirm the preview
   excludes controls, menus and dialogs. Move/resize/minimize the app. Click the icon again to dock the existing controls. Repeat in Multiple and
   Performance playback. Native mode dialogs must show above the embedded editor,
   with its original controls restored when the dialog closes.
   Hide UI must hide every control; click anywhere on visuals to restore them.
   Verify Text lyrics, dropped assets, FX menus, Settings and MIDI in both layouts.
9. Recording: choose an FFmpeg executable in Settings, then test start, stop and
   finalization in each workspace. Scene-only recordings should exclude controls.

GRID Lab presets use per-mode JSON folders. Native Multiple/Performance use their
own library formats under `%APPDATA%/Cenaceae/GRID-ARVx/VelEngine/Library/Presets`.
They do not automatically read Lab JSON files. Native image/font drops follow the
original panel's import behavior; import lyric TXT files through Lab Text.

Report the workspace, preset, settings and steps for defects. Focused panel
regressions are run for the 0.4.5 crash; broader automated tests and creator
packaging remain pending your manual review; compile success alone is not a
claim that every new transition or control has passed runtime testing.


## Text and cycle follow-up (0.4.14)

In Lab Text, enter two different lines and click each numbered selector. Confirm
both appear. Enable lyric cycling at 1, then 2 beats while audio is playing; repeat
with seconds timing and no audio. Check background/text color cycling and automatic
preset cycling together. Cycle selections should remain enabled during automatic
preset changes. Try a basic font and clear it again. Every offered Text style should
show its own native typeface. Audio cycles follow detected transients; songs without
clear beats may not produce an exact musical grid. Report the track and chosen
interval if the perceived timing differs.

## Regression checks (0.4.18)

The Windows standalone supports isolated `--self-test`, `--ui-check`,
`--render-check`, `--fx-check`, `--record-check` and `--transition-check` runs,
each followed by an output-folder argument. They use temporary test profiles and
can run alongside the review application. Keep GPU checks sequential for useful
performance measurements. `GRID_TEST_QUIET_DESKTOP=1` limits desktop interference;
hardware choosers and Explorer are omitted in that mode.

`tests/native_fx_image_checks.py` validates native-backend captures. The legacy
`tests/fx_image_checks.py` checks the independent renderer and deliberately rejects
native-engine captures rather than imposing different effect algorithms.
The probe's `--graphics` mode verifies real framebuffer dimensions through 4K;
`--profile` records both render timings and routine control-update duration.

Recording verification checks encoded dimensions, cadence, frame count and full
decode. A 60 FPS file can repeat rendered frames: successful encoding does not
establish that a heavy scene rendered 60 distinct frames per second.
