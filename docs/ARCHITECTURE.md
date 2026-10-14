# GRID-ARVx 0.2 architecture

C++20, CMake, and JUCE supply platform audio, host wrappers, controls, and file
loading. The same Processor and Editor sources compile into the app and plugins.
The standalone shell uses an input-only device callback; it does not monitor
microphones back to speakers.

`AudioAnalysis::push` is a single-producer bounded queue. The visualization timer
consumes samples, computes RMS/peak and approximate frequency-band energies,
and publishes separate left/right waveforms plus their mono average to the renderer.
Stereo channel energies are averaged so opposite phase does not cancel visual intensity.
When the queue fills, analysis
samples are dropped; host audio continues untouched. Analysis results are for
visual modulation, not calibrated measurement. The first implementation uses
simple filters rather than an FFT. The editor detects bass transients for beat cycles,
or uses a host-supplied BPM clock. This clock is not aligned to DAW PPQ/song position.

The Canvas owns a JUCE OpenGL 3.2 context. Software paths, glyphs and particles
are uploaded to the GPU; procedural fields, separable bloom and FX execute in
framebuffer stages. Track reads a bounded low-resolution luminance image for region
detection and draws its overlays. GPU data is synchronized outside the audio callback.
Only the renderer owns its GPU resources.
The context is detached before destruction.

The top mode selector, bottom preset/style selector, Text effect selector and
preset Cycle share the window's center line. Their positions depend on client
width, not on how many side controls the active mode exposes. MIDI sits between
MENU and Record. Text dispatches to its own renderer before any other scene logic;
its glyphs, backgrounds, images and effects never use another mode's contour.
Default Text styles use basic system fonts, with optional user font imports and an
external style-font directory. Typeface data loads once when that directory changes;
it is not read on every rendered frame. Imported fonts override style-library fonts.
The unoffered legacy Normal variant keeps its numeric slot for saved-state compatibility.

Bloom, Texture and Dither are cached into one GPU image before Glass samples it.
Noise uses a coordinate/frame avalanche hash; Grain uses clustered monochrome samples
with an exposure-dependent amplitude. Neither translates a fixed texture with time.
Glass uses sphere entry/exit refraction for round lenses and height-field normals for
the shaped lenses. RGB rays have different refractive indices. The material blends
with the original image over the existing 0–127 control range.

Recording reads rendered visuals into a four-frame bounded queue. A worker converts
scanlines, fits the image without stretching, and pipes native RGB/BGR rows to a
separate FFmpeg process without per-pixel Colour conversions.
It duplicates frames to preserve timing, handles duration limits and finalizes outside
the UI timer. NVIDIA preflight also runs outside the message thread.

Plugin IDs and the app settings root are unique to this project:
`com.cenaceae.gridarvx`, manufacturer `Cenaceae`, plugin `GAvx`, and
`<user application data>/Cenaceae/GRID-ARVx`. User preset saves copy image/font
assets with relative paths. Unsaved complete state can still refer to machine-local
files; those files must accompany projects moved between machines.

Parameter schema is 2. All live parameters are integer CC values 0–127. MIDI mappings
serialize stable target IDs plus hardware type and optional fixed profile actions.
The processor uses atomic lookup entries and a bounded learn-event queue; rendering,
recording and filesystem work stay outside the DAW audio callback. Foundation binary
state aliases are migrated, but old DAW automation-lane identifiers were replaced.
