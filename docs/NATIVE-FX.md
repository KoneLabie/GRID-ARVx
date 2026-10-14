> Current 0.4.0 status: Vel Engine/Multiple/Performance integration compiled. New
> runtime behavior awaits manual review; automated regressions and the creator ZIP
> are deferred until then. The notes below describe earlier development results.
> See [current integration](ORIGINAL-ENGINE.md) and [manual review](PRIVATE-TESTING.md).

# Native FX status â€” 0.3.0 private preview

The private Windows standalone now runs the copied original Visual Lab Pro 1.2.0
engine behind GRID's UI. Its native FX replace the reconstructed renderer in this
build. [Engine integration](ORIGINAL-ENGINE.md) describes the implemented bridge,
isolation and remaining limits. [Private testing](PRIVATE-TESTING.md) covers launch
requirements. No public upload, repository push or distribution has occurred.

The following record describes earlier independent-renderer revisions. It does
not describe the active native preview or establish native equivalence.

---

# Native FX reconstruction: 0.2.2

The executable now runs `Source/Visual/ReferenceFx.cpp`. The previous recovery
phase only wrote analysis files; it did not change the application. This revision
connects reconstructed image operations to the standalone and plugin renderers.
The standalone window title identifies the new build as **0.2.2**.

The renderer builds the scene and bloom, processes active effects in a bounded
600 Ã— 500 RGBA buffer, adds Track, applies Glass, and presents that result.
With no FX active, the scene retains its configured rendering resolution.
The pass order is **Dither â†’ Texture â†’ Track â†’ Glass**.

The original Texture UI remaps choices to host ordinals `[2,1,0,3]` at
`0x140a0ca30`. Reconstruction uses that mapping:

| UI choice | Implementation |
| --- | --- |
| Noise | Scalar random noise, then nearest colour in the current mode's palette |
| Grain | Palette quantization with weighted Floyd-Steinberg error diffusion |
| CRT | Radial warp, per-row RGB displacement, alternate scanline darkening, temporal persistence, block averaging/posterization and audio-triggered row glitches |
| Thresh | Nearest-palette quantization |

Symbol uses the recovered 23/27/6-entry brightness tables, with repeats retained.
Its cell-size curve is `4 + 76 * amount^1.8`. Unicode code points are constructed
explicitly; system fonts provide the glyphs. No extracted font files are bundled.
Star, Flower and Hex use geometric paths and the recovered shape cell-size curve.

Glass uses reconstructed native radius/placement, radial magnification, RGB
offsets, rim lighting, moving caustics and the 24-point Burst polygon. The cached
Burst mask is 100 Ã— 100, and the output uses the native 2 Ã— 2 pixel blocks.
Display aspect correction keeps the lenses from stretching with the window.
Ball, 2x2, Full and Cross masks remain stationary. Only Flower and Burst rotate;
caustics and reflections can animate inside every lens. Glass's animation clocks
advance only while the effect renders, rather than jumping to total app uptime.

Reactive amounts now use the recovered input gate, a +9 dB center gain, .02 mean
absolute signal threshold and .45 pulse duty cycle. The pulse holds a randomized
variation of the manual amount; quiet audio does not multiply the selected effect
into a nearly invisible amount. Silence clears the gate. Timing/random generators
remain reconstructed, not bit-identical to the native binary.
As requested, pixels outside the lenses retain their source; the native effect's
global darkening outside the lenses is intentionally omitted.

This is not a claim of complete pixel equivalence. Track retains the existing
implementation the user approved. Bloom remains the existing GPU implementation.
Some CRT transient behavior and audio-envelope timing are reconstructed rather
than exact native state-machine translations. System-font glyphs and random seeds
can differ from Visual Lab Pro. The GRID-ARVx renderer does not load the original executable. Reference inspection runs the separate copied program in the VisualLabProLab sandbox.

Validation includes exact zero-value bypass, palette-only outputs, independent
Noise frames, distinct Dither/Glass profiles, opaque alpha and maximum-strength
Glass corner preservation, plus real OpenGL captures and UI checks. Integrity
checks on extracted files alone are not rendering validation.
