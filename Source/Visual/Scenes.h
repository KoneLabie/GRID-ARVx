#pragma once
#include "Core/AudioAnalysis.h"
#include "Core/Controls.h"
#include <juce_graphics/juce_graphics.h>
namespace kone::grid {
inline const juce::StringArray categories{
    "Oscilloscope", "Lava Lamp", "Interstellar", "Code",  "Basic Colors",
    "TV",           "Pixel",     "Text",         "Plasma"};
inline juce::StringArray templates(int category) {
  switch (category) {
  case 0:
    return {"Wave",   "Lisaj", "Radial",  "Tunnel",  "Aurora",
            "Mirror", "Helix", "Strings", "Wavefrm", "Cube",
            "Earth",  "Tetra", "Dodec",   "Icosa",   "Octa"};
  case 1:
    return {"Lava Flow",   "Solar Flare",  "Tide",
            "Strobe Cell", "Liq Crystal",  "Iris",
            "Fracture",    "Kaleidoscope", "Tendril"};
  case 2:
    return {"Hyper Jump", "Wormhole", "Nebula Drift", "Neon Bloom",
            "Constellation", "Solar Wind", "Deep Dive", "Prism Burst"};
  case 3:
    return {"Rain", "Grid", "Skull", "Three Tone", "Pulse", "Warp", "Cipher", "Glitch"};
  case 4:
    return {"Swirl", "Bullseye", "Shatter", "Warp", "Checker", "Ripple",
            "Pinwheel", "Diamonds", "Tunnel", "Strobe", "Zebra"};
  case 5:
    return {"TV God", "Chroma", "Plasma", "Signal", "Vortex", "Strobe", "Lissajous", "Crystal", "Static"};
  case 6:
    return {"Bricks", "Rain", "Plasma", "Life", "Waveform", "Cityscape",
            "Starfield", "Crystal", "Balloon"};
  case 7:
    return {"Simple", "Normal", "Hand",    "VHS",    "System", "Tarophis",
            "Pixel",  "Ransom", "Noskale", "Picnic", "Flor",   "Myrtille"};
  default:
    return {"Mirror", "Helix", "Figure 8", "Pulse", "Lattice", "Ribbon", "Knot"};
  }
}
inline constexpr int templateCatalogRevision = 4;
// Text keeps its established saved-state indices; menu order follows the binary.
inline juce::StringArray defaultOrder(int category) {
  return category == 7 ? juce::StringArray{"Simple", "Pixel", "Flor", "System", "Hand",
       "Tarophis", "Picnic", "Myrtille", "Noskale", "VHS", "Ransom"} : templates(category);
}
inline int migrateLegacyVariant(int category, int old) {
  static constexpr int interstellar[]{0,2,3,5,6,4}, code[]{0,1,7,5,4},
      basic[]{0,1,4,7,9,10}, tv[]{0,2,3,8,6,7}, pixel[]{0,2,3,4,6,7,8,1,5,0},
      plasma[]{0,4,2,6,5,1};
  const int* map = nullptr; int count = 0;
  switch(category) {
    case 2: map=interstellar; count=6; break;
    case 3: map=code; count=5; break;
    case 4: map=basic; count=6; break;
    case 5: map=tv; count=6; break;
    case 6: map=pixel; count=10; break;
    case 8: map=plasma; count=6; break;
    default: return old;
  }
  return map[juce::jlimit(0,count-1,old)];
}
inline const juce::StringArray textEffects{"Warp", "Expand", "Spin",  "Glitch",
                                           "Wave", "Shake",  "Strobe"};
// Keep the early Normal slot readable in saved states, but do not offer an
// invented default alongside the eleven styles visible in the recording.
inline bool offeredTemplate(int category, int variant) {
  return category != 7 || variant != 1;
}
inline int defaultColor(int category) {
  const int colors[] = {20, 82, 70, 43, 82, 127, 0, 84, 127};
  return colors[juce::jlimit(0, 8, category)];
}
inline int defaultColor2(int category) { return category == 8 ? 126 : 127; }
inline int defaultTextColor(int category, int variant) {
  return category == 7 && (variant == 4 || variant == 7) ? 126 : 127;
}
inline std::array<juce::Colour,2> nativeDefaultColors(int category,int variant) {
  // Original Basic Colors/Plasma 20 paired palettes, table 0x140a0d180.
  static constexpr juce::uint32 primary[]{0xffe8112d,0xffda291c,0xffff6900,0xffe0a526,
    0xfffce300,0xffc5d63d,0xff7ab648,0xff00a94f,0xff4a7729,0xff9bc4e2,
    0xff3065ab,0xff2e17b0,0xff6558b1,0xff59118e,0xfff0548f,0xfff59bbb,
    0xff6a3906,0xffbbbcbc,0xffffffff,0xff000000};
  static constexpr juce::uint32 secondary[]{0xfffce300,0xffbbbcbc,0xffc5d63d,0xfffce300,
    0xfff59bbb,0xffff6900,0xff00a94f,0xff9bc4e2,0xffe8112d,0xffff6900,
    0xfff59bbb,0xfffce300,0xfff0548f,0xffe8112d,0xff00a94f,0xff2e17b0,
    0xff00a94f,0xff2e17b0,0xff000000,0xffffffff};
  static constexpr int basic[]{0,11,3,15,8,17,1,11,16,10,12};
  if(category==4||category==8) {
    const auto index=category==4?basic[juce::jlimit(0,10,variant)]:9;
    return {juce::Colour(primary[index]),juce::Colour(secondary[index])};
  }
  if(category==3)return {juce::Colour(0xff44d62c),juce::Colours::white};
  return {};
}
inline int colorCC(juce::Colour color) {
  if(color==juce::Colours::white)return 127;
  if(color==juce::Colours::black)return 126;
  return juce::roundToInt(color.getHue()*125);
}
inline int defaultTemplateColor(int category, int variant) {
  if (category==3||category==4||category==8) return colorCC(nativeDefaultColors(category,variant)[0]);
  if (category != 7)
    return defaultColor(category);
  if (variant == 6)
    return 126;
  if (variant == 7)
    return 127;
  if (variant == 9 || variant == 11)
    return 20;
  if (variant == 10)
    return 42;
  return defaultColor(category);
}
inline juce::StringArray profiles(int fx) {
  switch (fx) {
  case 0:
    return {"Noise", "Grain", "CRT", "Thresh"};
  case 1:
    return {"Symbol", "Star", "Flower", "Hex"};
  case 2:
    return {"Basic", "System", "Bracket", "Glow", "Terminal"};
  default:
    return {"Ball", "2x2", "Full", "Flower", "Cross", "Burst"};
  }
}
inline juce::Colour palette(int v, bool textBackground = false) {
  if (v == 127)
    return juce::Colours::white;
  if (v == 126)
    return juce::Colours::black;
  return juce::Colour::fromHSV(v / 126.0f, textBackground ? .78f : .88f,
                               textBackground ? .68f : 1.0f, 1.0f);
}
struct SceneState {
  int captureTag = -1;
  bool tracing = false, hasImage = false;
  bool secondaryPalette = false;
  bool nativeDefaultColour = false;
  bool nativeFrame = false;
  std::array<float,5> nativeSignal{}; // low, middle, high, RMS, onset
  std::array<int, controls.size()> values{};
  AudioFrame audio;
  std::array<juce::Colour,3> presetColors{};
  double time = 0;
  int width = 960, height = 540, detail = 2;
  int get(const char *id) const { return values[(size_t)indexOf(id)]; }
  juce::Colour colour(const char* id,bool background=false) const {
    const int channel=juce::String(id)=="color2"?1:juce::String(id)=="textColor"?2:0;
    return presetColors[(size_t)channel].getAlpha()?presetColors[(size_t)channel]:palette(get(id),background);
  }
  int category() const { return decodeChoice(get("category"), 9); }
  int variant() const {
    return juce::jlimit(0, templates(category()).size() - 1,
                        decodeChoice(get("variant"), 16));
  }
  float amplitude() const {
    return juce::jlimit(0.0f, 2.0f,
                        audio.rms * inputGain(get("in")) * get("int") / 64.0f *
                            3.0f);
  }
};
struct NativeDynamics {
  double lastTime = -1;
  std::array<float,5> envelope{};
  float previousLow = 0;
  void update(SceneState& s) {
    const float frames=lastTime<0?1:juce::jlimit(0.0f,6.0f,(float)(s.time-lastTime)*60);
    if(lastTime>s.time){envelope={};previousLow=0;}
    lastTime=s.time;
    const float gain=std::pow(10.0f,((s.get("in")/127.0f-.5f)*40+12.2f)/20);
    std::array<float,4> target{};
    for(int i=0;i<128;i+=16)target[0]+=std::abs(s.audio.scope[(size_t)i])*.125f*gain;
    for(int i=128;i<640;i+=16)target[1]+=std::abs(s.audio.scope[(size_t)i])/32*gain;
    for(int i=640;i<1920;i+=16)target[2]+=std::abs(s.audio.scope[(size_t)i])/80*gain;
    target[3]=s.audio.rms;
    constexpr float rates[]{.15f,.1f,.08f,.08f,.25f};
    for(int i=0;i<4;++i)envelope[(size_t)i]+=(target[(size_t)i]-envelope[(size_t)i])*(1-std::pow(1-rates[i],frames));
    envelope[4]+=(juce::jmax(0.0f,target[0]-previousLow)*8-envelope[4])*(1-std::pow(1-rates[4],frames));
    previousLow=target[0];s.nativeSignal=envelope;
  }
};
struct Assets {
  juce::Image image;
  juce::Path contour;
  juce::Typeface::Ptr font;
  std::array<juce::Typeface::Ptr, 12> styleFonts;
  juce::StringArray lyrics{"GRID-ARVx"};
};
juce::Image drawScene(const SceneState &, const Assets &);
juce::Image drawTextScene(const SceneState &, const Assets &);
juce::Path traceImage(const juce::Image &);
} // namespace kone::grid
