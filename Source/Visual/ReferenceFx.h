#pragma once
#include "Scenes.h"
#include <array>
#include <vector>

namespace kone::grid {
// Reconstructed image operations from the recovered native routines. Buffers
// use OpenGL RGBA ordering/bottom-up rows; JUCE glyph drawing is converted at
// the boundary. No original executable code is loaded or called.
class ReferenceFx {
public:
  static constexpr int width = 600, height = 500;
  using Buffer = std::vector<juce::uint8>;
  void ditherAndTexture(Buffer &, const SceneState &, float dither, float texture);
  void glass(Buffer &, const SceneState &, float amount);
  std::array<float,4> effectiveAmounts(const SceneState &);
  void reset();

private:
  void dither(Buffer &, const SceneState &, float);
  void texture(Buffer &, const SceneState &, float);
  void noise(Buffer &, float, float);
  Buffer scratch, history;
  std::vector<float> symbolSelection;
  int symbolColumns = 0, symbolRows = 0;
  double lastSymbols = -1, lastGlass = -1;
  float lastOnset = 0, glassFlash = 0, lastGlassOnset = 0;
  float glassPhase=0,glassReflectionPhase=0;
  struct ReactiveState { float phase=0, heldAmount=0; bool wasOn=false; };
  std::array<ReactiveState,4> reactive{};
  double amountTime=-1;
  juce::Random random{0x475249444658};
  std::array<float, 100 * 100> burstDistance{};
  float burstDepth = .1f;
  bool burstReady = false;
};
} // namespace kone::grid
