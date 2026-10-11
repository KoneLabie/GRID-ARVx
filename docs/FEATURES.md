# Foundation feature map

The program uses its own visual code. Vendor category names below identify
research goals, not identical output or presets.

| Category / capability | This ZIP |
| --- | --- |
| Oscilloscope | Working original waveform prototype |
| Basic Colors | Working original audio-driven color wash |
| Plasma | Working original interference shader |
| Custom Text | Working text/font overlay; advanced transforms planned |
| Lava Lamp | Planned metaball renderer |
| Interstellar | Planned starfield and image-outline renderer |
| Code | Planned glyph stream renderer |
| TV | Planned analog distortion renderer |
| Pixel | Planned quantized image/visual renderer |
| Arcade | Deferred, low priority |
| Multiple Mode | Deferred, low priority |
| Performance Mode | Planned cue/setlist engine |
| Texture | Working basic noise amount; other styles planned |
| Dither / Track / Glass | Planned post-processing modules |
| Images | PNG/JPG overlay with audio pulse; automatic contour detection planned |
| Fonts | TTF/OTF import; per-machine asset references saved |
| Lyrics | Editable/TXT text overlay; SRT/LRC timing and per-line cues planned |
| MIDI, video export, preset browser | Planned; not enabled in this foundation |

No unavailable category is presented as a working UI control. New visual modules
should share the same audio frame and parameter system, and expensive analysis
should stay outside the realtime audio callback.
