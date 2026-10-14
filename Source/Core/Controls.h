#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <string_view>
namespace kone::grid {
enum class Workspace { lab = 0, multiple = 1, performance = 2 };
inline const char *workspaceName(Workspace workspace) {
  return workspace == Workspace::multiple ? "Multiple"
       : workspace == Workspace::performance ? "Performance" : "Lab";
}
enum class Kind { knob, button, choice };
struct Control {
  const char *id;
  const char *name;
  int initial;
  Kind kind;
  int choices = 0;
};
inline constexpr std::array controls{
    Control{"category", "Category", 0, Kind::choice, 9},
    Control{"variant", "Template", 0, Kind::choice, 16},
    Control{"in", "Input", 64, Kind::knob},
    Control{"int", "Intensity", 64, Kind::knob},
    Control{"speed", "Speed", 64, Kind::knob},
    Control{"glow", "Glow", 24, Kind::knob},
    Control{"texture", "Texture", 0, Kind::knob},
    Control{"dither", "Dither", 0, Kind::knob},
    Control{"track", "Track", 0, Kind::knob},
    Control{"glass", "Glass", 0, Kind::knob},
    Control{"textureType", "Texture profile", 0, Kind::choice, 4},
    Control{"ditherType", "Dither profile", 0, Kind::choice, 4},
    Control{"trackType", "Track profile", 0, Kind::choice, 5},
    Control{"glassType", "Glass profile", 0, Kind::choice, 6},
    Control{"textureReact", "Texture reactive", 0, Kind::button},
    Control{"ditherReact", "Dither reactive", 0, Kind::button},
    Control{"trackReact", "Track reactive", 0, Kind::button},
    Control{"glassReact", "Glass reactive", 0, Kind::button},
    Control{"glowReact", "Glow reactive", 0, Kind::button},
    Control{"color", "Primary color", 20, Kind::knob},
    Control{"color2", "Secondary color", 82, Kind::knob},
    Control{"textColor", "Text color", 127, Kind::knob},
    Control{"colorCycle", "Primary color cycle", 0, Kind::button},
    Control{"color2Cycle", "Secondary color cycle", 0, Kind::button},
    Control{"textColorCycle", "Text color cycle", 0, Kind::button},
    Control{"presetCycle", "Preset cycle", 0, Kind::button},
    Control{"colorInterval", "Primary cycle beats", 0, Kind::knob},
    Control{"color2Interval", "Secondary cycle beats", 0, Kind::knob},
    Control{"textColorInterval", "Text cycle beats", 0, Kind::knob},
    Control{"presetInterval", "Preset cycle beats", 0, Kind::knob},
    Control{"lock", "FX lock", 0, Kind::button},
    Control{"textEffect", "Text effect", 0, Kind::choice, 7},
    Control{"lyric", "Lyric line", 0, Kind::knob},
    Control{"lyricCycle", "Lyric cycle", 0, Kind::button},
    Control{"lyricTimeMode", "Lyric seconds", 0, Kind::button},
    Control{"lyricInterval", "Lyric interval", 0, Kind::knob},
    Control{"imageScale", "Image scale", 64, Kind::knob},
    Control{"muteInput", "Mute analysis input", 0, Kind::button},
    Control{"frameCap", "Frame cap", 56, Kind::knob},
    Control{"renderScale", "Render scale", 46, Kind::knob},
    Control{"renderDetail", "Detail", 42, Kind::knob},
    Control{"bloomQuality", "Bloom quality", 64, Kind::knob},
    Control{"antialias", "Sampling filter", 127, Kind::button},
    Control{"trackingRate", "Tracking rate", 32, Kind::knob},
    Control{"trackingResolution", "Tracking resolution", 42, Kind::knob},
    Control{"vsync", "VSync", 127, Kind::button},
    Control{"adaptive", "Adaptive quality", 0, Kind::button},
    Control{"stats", "FPS display", 0, Kind::button},
    Control{"nextPreset", "Next preset", 0, Kind::button},
    Control{"previousPreset", "Previous preset", 0, Kind::button},
    Control{"resetFx", "Reset FX", 0, Kind::button},
    Control{"savePreset", "Save preset", 0, Kind::button},
    Control{"recordVideo", "Record video", 0, Kind::button},
    Control{"hideUI", "Hide UI", 0, Kind::button},
    Control{"fullscreen", "Fullscreen", 0, Kind::button},
    Control{"home", "Home", 0, Kind::button},
    Control{"addLyric", "Add lyric", 0, Kind::button},
    Control{"clearLyrics", "Clear lyrics", 0, Kind::button},
    Control{"clearImage", "Clear image", 0, Kind::button},
    Control{"perfNext", "Next cue", 0, Kind::button},
    Control{"perfPrevious", "Previous cue", 0, Kind::button},
    Control{"perfStrobes", "Strobes", 0, Kind::button},
    Control{"perfBlackout", "Blackout", 0, Kind::button},
    Control{"perfAuto", "Auto", 0, Kind::button},
    Control{"perfBypass", "Bypass", 0, Kind::button},
    Control{"multiTile", "MIDI target tile", 0, Kind::knob},
    Control{"renderResolution", "Render resolution", 64, Kind::choice, 7},
    Control{"pauseMenuRendering", "Pause visuals in menus", 127, Kind::button},
    Control{"popOutUI", "Pop out controls", 0, Kind::button}};
inline int indexOf(std::string_view id) {
  for (int i = 0; i < (int)controls.size(); ++i)
    if (id == controls[(size_t)i].id)
      return i;
  return -1;
}
inline bool globalControl(std::string_view id) {
  return id == "lock" || id == "presetCycle" || id == "presetInterval" ||
         id == "muteInput" || id == "frameCap" || id == "renderScale" ||
         id == "renderDetail" || id == "bloomQuality" || id == "antialias" ||
         id == "trackingResolution" || id == "trackingRate" || id == "vsync" ||
         id == "adaptive" || id == "stats" || id == "renderResolution" || id == "pauseMenuRendering" || id == "popOutUI" ||
         (indexOf(id) >= indexOf("nextPreset") &&
          indexOf(id) <= indexOf("clearImage"));
}
inline int encodeChoice(int c, int n) {
  return n < 2 ? 0
               : (int)std::lround(std::clamp(c, 0, n - 1) * 127.0 / (n - 1));
}
inline int decodeChoice(int v, int n) {
  return n < 2 ? 0 : (int)std::lround(std::clamp(v, 0, 127) * (n - 1) / 127.0);
}
inline float inputGain(int v) {
  return std::pow(10.0f, (v - 64) * 24.0f / 64.0f / 20.0f);
}
struct MidiEvent {
  int channel = 1, cc = 0, value = 0;
  bool note = false;
};
class MidiEvents {
  std::array<MidiEvent, 256> data{};
  std::atomic<unsigned> r{0}, w{0};

public:
  bool push(MidiEvent e) noexcept {
    auto i = w.load(std::memory_order_relaxed), n = (i + 1) % 256;
    if (n == r.load(std::memory_order_acquire))
      return false;
    data[i] = e;
    w.store(n, std::memory_order_release);
    return true;
  }
  bool pop(MidiEvent &e) noexcept {
    auto i = r.load(std::memory_order_relaxed);
    if (i == w.load(std::memory_order_acquire))
      return false;
    e = data[i];
    r.store((i + 1) % 256, std::memory_order_release);
    return true;
  }
};
enum class Hardware { knob, button };
struct Binding {
  int target = -1, fixed = -1;
  Hardware hardware = Hardware::knob;
};
inline bool compatible(Binding b) {
  if (b.target < 0 || b.target >= (int)controls.size())
    return false;
  auto k = controls[(size_t)b.target].kind;
  if (b.fixed >= 0)
    return (k == Kind::choice ||
            std::string_view(controls[(size_t)b.target].id) == "lyric") &&
           b.hardware == Hardware::button && b.fixed <= 127;
  return k != Kind::choice &&
         ((k == Kind::button) == (b.hardware == Hardware::button));
}
inline int pack(Binding b) {
  return compatible(b) ? (b.target + 1) | ((b.fixed + 1) << 8) |
                             ((b.hardware == Hardware::button ? 1 : 0) << 16)
                       : 0;
}
inline Binding unpack(int p) {
  return {(p & 255) - 1, ((p >> 8) & 255) - 1,
          (p & (1 << 16)) ? Hardware::button : Hardware::knob};
}
} // namespace kone::grid
