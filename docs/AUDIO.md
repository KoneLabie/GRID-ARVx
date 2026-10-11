# Audio setup

In a DAW, insert GRID-ARVx as an audio effect on an instrument, bus, or master
track. Play audio and open its editor. The processor analyzes its input and
leaves the audio buffer untouched. It does not provide sidechain routing yet.

In the standalone app, open **Audio input settings** and select a microphone or
audio interface. The app opens input channels only and does not play sound back.
If the input is mono, the analyzer treats it as mono on both sides.

For computer playback, use an input source already exposed by your system:

- Windows: a device's loopback or Stereo Mix recording input, if available;
  otherwise route output through a virtual audio input.
- macOS: route audio through a user-installed loopback/virtual device and select
  it as input. No audio driver is bundled here.
- Linux: expose/select a monitor input through your PulseAudio/PipeWire routing;
  the exact device list depends on the installed audio setup.

System playback support currently depends on that routing. This is not an
automatic WASAPI loopback or ScreenCaptureKit capture implementation. Device
availability and permissions need testing on each target OS.
