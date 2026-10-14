#pragma once
#include "Controls.h"
namespace kone::grid {
inline int frameRate(int value) {
  return (int)std::lround(12 + value * 108.0 / 127);
}
inline int frameRateValue(double fps) {
  return std::clamp((int)std::lround((fps - 12) * 127 / 108), 0, 127);
}
inline int scalePercent(int value) {
  return (int)std::lround(25 + value * 125.0 / 127);
}
inline int scaleValue(double percent) {
  return std::clamp((int)std::lround((percent - 25) * 127 / 125), 0, 127);
}
struct RenderResolution { int width,height; const char *name; };
inline constexpr std::array renderResolutions{
    RenderResolution{854,480,"480p (854 x 480)"},
    RenderResolution{1280,720,"720p (1280 x 720)"},
    RenderResolution{1600,900,"900p (1600 x 900)"},
    RenderResolution{1920,1080,"1080p (1920 x 1080)"},
    RenderResolution{2560,1440,"1440p (2560 x 1440)"},
    RenderResolution{3200,1800,"1800p (3200 x 1800)"},
    RenderResolution{3840,2160,"4K (3840 x 2160)"}};
inline RenderResolution renderResolution(int cc) {
  return renderResolutions[(size_t)decodeChoice(cc,(int)renderResolutions.size())];
}
struct GraphicsTier {
  int fps, scale, detail, bloom, tracking, trackResolution;
  bool filter;
};
inline constexpr std::array graphicsTiers{
    GraphicsTier{24, 35, 0, 0, 0, 0, false},
    GraphicsTier{30, 50, 1, 25, 12, 0, true},
    GraphicsTier{60, 70, 2, 64, 32, 1, true},
    GraphicsTier{60, 85, 3, 76, 48, 1, true},
    GraphicsTier{60, 100, 4, 89, 64, 2, true},
    GraphicsTier{90, 115, 5, 102, 96, 3, true},
    GraphicsTier{120, 150, 6, 127, 127, 3, true}};
} // namespace kone::grid
