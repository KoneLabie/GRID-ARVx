#pragma once
#include "Scenes.h"
#include "ReferenceFx.h"
#include <juce_opengl/juce_opengl.h>
namespace kone::grid {
class Canvas final : public juce::Component, private juce::OpenGLRenderer {
public:
  Canvas();
  ~Canvas() override;
  void present(juce::Image, const SceneState &);
  void setPaused(bool value) noexcept { paused.store(value); }
  void presentNative(juce::Image nativeImage, const SceneState &state) {
    auto display = state;
    display.nativeFrame = true;
    display.values[(size_t)indexOf("category")] = 0;
    display.tracing = true;
    for (auto id : {"texture", "dither", "track", "glass", "glow"})
      display.values[(size_t)indexOf(id)] = 0;
    present(std::move(nativeImage), display);
  }
  void attachUi(juce::Component &);
  void attachScene();
  void detachUi() { context.detach(); }
  juce::Component *presentationTarget() const { return context.getTargetComponent(); }
  void capturePresented(std::function<void(juce::Image)>);
  void paint(juce::Graphics &) override;
  juce::String error() const;
  SceneState lastPresentedScene() const {
    juce::ScopedLock guard(lock);
    return scene;
  }
  std::function<void(juce::Image, int)> onCapture;
  std::atomic<bool> capture{false};
  std::atomic<double> frameMilliseconds{0};

private:
  void newOpenGLContextCreated() override;
  void renderOpenGL() override;
  void openGLContextClosing() override;
  juce::OpenGLContext context;
  std::atomic<bool> paused{false};
  std::unique_ptr<juce::OpenGLShaderProgram> shader;
  std::unique_ptr<juce::OpenGLShaderProgram> blurShader;
  std::array<unsigned int, 5> stageFbos{}, stageTextures{};
  ReferenceFx referenceFx;
  ReferenceFx::Buffer referencePixels;
  unsigned int vbo = 0, vao = 0, sourceTexture = 0, fbo = 0, smallTexture = 0;
  int uploadW = 0, uploadH = 0, frames = 0;
  int effectW = 0, effectH = 0;
  mutable juce::CriticalSection lock;
  juce::Image image,uploadedImage;
  int sourceWidth=0,sourceHeight=0;
  SceneState scene;
  juce::String shaderError;
  std::array<float, 64> boxes{};
  int boxCount = 0;
  double lastFrame = 0;
  int lastVsync = -1;
  int trackWidth = 64, trackHeight = 36;
  std::array<float,160> voronoiSeeds{};
  std::array<int,40> voronoiLabels{};
};
} // namespace kone::grid
