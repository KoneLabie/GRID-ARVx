# Original Defaults catalog

Catalog revision 4 replaces the earlier approximate preset lists with all 87
built-in Defaults from the isolated Visual Lab Pro executable. Arcade is excluded.
Factory remains a separate folder. The 90 recovered FACTORY XML files are not
mislabelled as Defaults or installed into it.

The exact menu inventory and order come from Binary Ninja function `0x140565330`.
Mode setup defaults come from `0x1404aa8a0`; Basic Colors has eleven 16-byte setup
records at `0x140a0e3d0`. Original Basic Colors/Plasma paired RGB colors come from
`0x140a0d180`, and Code's initial green from `0x140a0d940`. Original RGB values are
stored alongside their MIDI CC positions; moving a color control resumes the CC
palette. Saving a User preset preserves these color values.

| Mode | Defaults |
| --- | --- |
| Oscilloscope | Wave, Lisaj, Radial, Tunnel, Aurora, Mirror, Helix, Strings, Wavefrm, Cube, Earth, Tetra, Dodec, Icosa, Octa |
| Lava Lamp | Lava Flow, Solar Flare, Tide, Strobe Cell, Liq Crystal, Iris, Fracture, Kaleidoscope, Tendril |
| Interstellar | Hyper Jump, Wormhole, Nebula Drift, Neon Bloom, Constellation, Solar Wind, Deep Dive, Prism Burst |
| Code | Rain, Grid, Skull, Three Tone, Pulse, Warp, Cipher, Glitch |
| Basic Colors | Swirl, Bullseye, Shatter, Warp, Checker, Ripple, Pinwheel, Diamonds, Tunnel, Strobe, Zebra |
| TV | TV God, Chroma, Plasma, Signal, Vortex, Strobe, Lissajous, Crystal, Static |
| Pixel | Bricks, Rain, Plasma, Life, Waveform, Cityscape, Starfield, Crystal, Balloon |
| Text | Simple, Pixel, Flor, System, Hand, Tarophis, Picnic, Myrtille, Noskale, VHS, Ransom |
| Plasma | Mirror, Helix, Figure 8, Pulse, Lattice, Ribbon, Knot |

On the first revision-4 startup, old Defaults are copied into each mode's
`Previous Defaults/Before catalog 4` folder before replacement. User and Factory
files are preserved. Subsequent edits to the new Defaults remain editable and
are not overwritten on startup. Legacy User preset/state indices are mapped to
the corresponding replacement families; removed invented families map to their
closest supported family. Text retains its saved-state indices and separate mode.

The catalog, named families and recovered setup/color data are original references.
Scene drawing remains a reconstruction; the changes do not establish pixel-for-pixel
equivalence with the compiled Visual Lab Pro renderers. Text continues to use basic
system fonts and the recorded styles, not the supplied decorative example font.

Revision 4 corrects the Basic Colors cycle/two-palette flags from the native setup
records. Revision-3 state and User indices already use the new inventory and
are retained unchanged; only older inventories need index remapping.

The current non-UI transfer audit is `outputs/preset-native/RENDERER-TRANSFER-AUDIT.md`
in the task workspace. It separates recovered routines from implemented and
visually verified behavior. Exact-engine integration remains a user decision.
