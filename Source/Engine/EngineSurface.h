#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
namespace kone::grid {
class EngineSurface {
public:
  bool begin(int width,int height,int displayWidth,int displayHeight,int virtualWidth=0,int virtualHeight=0);
  void present(bool filtered);
  void capture();
  void close();
  static juce::Image frontImage();
  int width() const { return renderWidth; }
  int height() const { return renderHeight; }
private:
  unsigned framebuffer=0,texture=0,depth=0;
  int destinationRead=0,destinationDraw=0;
  std::array<int,4> destinationViewport{};
  int renderWidth=0,renderHeight=0,displayWidth=0,displayHeight=0;
};
}
