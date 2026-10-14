# Audio setup

In a DAW, insert GRID-ARVx as an audio effect on an instrument, bus, or master
track. Play audio and open its editor. The processor analyzes its input and
leaves the audio buffer untouched. It does not provide sidechain routing yet.

In the standalone app, open **Settings → Audio / MIDI Devices** and select a microphone or
audio interface. The app analyzes input without monitoring it back to speakers.
If the input is mono, the analyzer treats it as mono on both sides.

On Windows, capture music directly from speakers or headphones:

1. Open **Settings → Audio / MIDI Devices**.
2. Set **Analyze audio from** to **Speakers / system audio**.
3. Choose **Windows default speakers / headphones**, or select a specific playback
   device by its friendly name. Play music through that device.

Use **Microphone / interface** to switch back to an input device. The source mode
and selected speaker are saved with complete app state. Refresh updates the
speaker list after connecting devices. MIDI inputs apply to either source mode.
The live reading identifies the actual captured endpoint and input level. Windows
shared capture converts the playback mix into stereo floating-point samples, including
surround device downmixing. Default-device changes reconnect the capture stream.
The standalone app does not open an unused playback output or monitor audio back.
Reactive FX gate the selected strength rather than shrinking it with quiet playback.
GRID-ARVx analyzes speaker playback without rerouting or replaying it; VoiceMeeter
and virtual microphones are not required. This uses Windows
[WASAPI loopback capture](https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording).

For other operating systems, use an input source exposed by your system:

- macOS: route audio through a user-installed loopback/virtual device and select
  it as input. No audio driver is bundled here.
- Linux: expose/select a monitor input through your PulseAudio/PipeWire routing;
  the exact device list depends on the installed audio setup.

macOS/Linux playback support currently depends on that routing. ScreenCaptureKit
and native PulseAudio monitor adapters are not implemented. Device availability
and permissions still need testing on each target OS. MIDI mappings are restored
with complete app/host state; selecting MIDI devices remains a standalone setting.
