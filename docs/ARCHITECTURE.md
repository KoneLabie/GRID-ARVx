# GRID-ARVx foundation architecture

C++20, CMake, and JUCE supply platform audio, host wrappers, controls, and file
loading. The same Processor and Editor sources compile into the app and plugins.
The standalone shell uses an input-only device callback; it does not monitor
microphones back to speakers.

`AudioAnalysis::push` is a single-producer bounded queue. The visualization timer
consumes samples, computes RMS/peak and approximate frequency-band energies,
and publishes a waveform/frame to the renderer. When the queue fills, analysis
samples are dropped; host audio continues untouched. Analysis results are for
visual modulation, not calibrated measurement. The first implementation uses
simple filters, not an FFT or a tempo/beat detector.

The Canvas owns an experimental JUCE OpenGL 3.2 context and three independent
shaders. GPU data is synchronized outside the audio callback. JUCE paints image
and font overlays after the shader. Only the renderer owns its GPU resources.
The context is detached before destruction.

bgfx remains a candidate for the advanced engine. This ZIP does not commit the
project to bgfx or claim to implement it. Evaluate Metal/D3D/Vulkan versus the
prototype using actual DAW-host tests before making a permanent backend choice.
OpenGL is deprecated on macOS and this prototype requires host/GPU validation.

Plugin IDs and the app settings root are unique to this project:
`com.konelabie.gridarvx`, manufacturer `KnLb`, plugin `GAvx`, and
`<user application data>/KoneLabie/GRID-ARVx`. Assets are copied on import but
state files contain machine-local paths; moving a project between computers
requires copying/reimporting assets. Asset bundling is future work.

Automation IDs (`mode`, `sensitivity`, `speed`, `texture`) should remain stable.
Append future modes instead of reordering these initial indices when supporting
existing saved projects. Parameter schema version is 1.
