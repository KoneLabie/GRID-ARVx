#pragma once
#include "Visual/Scenes.h"
#include "EngineSurface.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_cryptography/juce_cryptography.h>
#include <deque>
#include <mutex>

namespace kone::grid {
// Vel Engine backend with a version-pinned renderer interface.
// The installed application/plugin and activation files are never patched.
class VelEngine final : private juce::Thread, private juce::ComponentListener {
public:
  VelEngine();
  ~VelEngine() override;
  void shutdown();
  juce::Result open(const juce::File &bundle,juce::File isolatedProfile = {});
  static juce::File adjacentBundle();
  void update(const SceneState &);
  void updateGraphics(int width,int height,int frameCap,int filtering,int vsync);
  void setPaused(bool value) noexcept { paused.store(value); }
  void setFrameCapture(bool value) noexcept { captureFrames.store(value); }
  void showWorkspace(Workspace, juce::Component *, juce::Rectangle<int>,bool compactHeader=false);
  void startPerformance();
  void finishMultipleLayout();
  void openMultiplePresets();
  void stepMultiplePreset(int direction);
  void saveMultiplePreset();
  bool multipleLayoutVisible() const;
  void coverPanel(bool);
  juce::Image keyboardGraphImage(int width,int height);
  void applyMappedControl(const juce::String &, int, bool button);
  bool requestedHome() const;
  void setHomeAction(std::function<void()>);
  void setMappingAction(std::function<void()> action) { mappingAction=std::move(action); }
  void setPerformanceKeyAction(std::function<bool(const juce::KeyPress &)> action) { performanceKeyAction=std::move(action); }
  bool performancePlaying() const;
  void triggerPerformanceAction(size_t);
  void setPerformanceHeld(bool,bool,bool);
#ifdef GRID_ENGINE_PROBE
  void checkPanelMenuClick();
  void checkPanelMappingClick();
  void checkPerformancePlay();
  void checkMultipleDone();
  void checkSaveMultiple(const juce::String &);
  void checkCancelMultipleSave();
  float checkPerformanceHold(bool) const;
  juce::var viewportState() const;
  juce::Image panelPresentation() const {juce::ScopedLock lock(frameLock);return presentedPanel;}
#endif
  bool forwardKey(const juce::KeyPress &);
  int activeCategory() const;
  juce::var frameTimings() const;
  double frameMilliseconds() const noexcept { return frameInterval.load(); }
  std::pair<int,int> renderingSize() const noexcept {
    const auto bits=renderDimensions.load();return {(int)(bits>>32),(int)(bits&0xffffffffu)};
  }
  juce::Result setLyrics(const juce::StringArray &, int active);
  void clearTextImage();
  void clearTextFont();
  void pushAudio(const float *, const float *, int, double) noexcept;
  juce::Result importFile(const juce::File &);
  juce::Image latestFrame() const;
  juce::String diagnostic() const;
  juce::var observedState() const;
  SceneState requestedScene() const {
    juce::ScopedLock guard(controlLock);
    return controls;
  }
  juce::var defaultProfile(int category, int variant) const;
  void setExactValues(const juce::var &);
  juce::uint64 frames() const noexcept { return frameCount.load(); }
  juce::uint64 panelFrames() const noexcept { return panelPaintCount.load(); }
  bool ready() const noexcept { return renderer != nullptr; }

private:
#ifdef GRID_ENGINE_PROBE
  void checkPanelClick(juce::Point<float>);
#endif
  void run() override;
  void render(void *);
  static void renderThunk(void *);
  static void closeThunk(void *);
  static void editorMouseDownThunk(void *,void *);
  static bool editorKeyThunk(void *,const void *);
  static void editorPaintThunk(void *,void *);
  static void editorResizedThunk(void *);
  static void editorModifiersThunk(void *,const void *);
  void checkMappingRequest();
  void componentMovedOrResized(juce::Component &,bool,bool) override;
  void fitViewport();
  void clickPanelControl(juce::Point<float>);
  void returnHome();
  bool connectRenderer();
  void prepareReviewUi();
  void configureComponentPainting(bool);
  void applyNativeFields(void *, const SceneState &);
  void dispatchImport();
  juce::AudioPluginFormatManager formats;
  std::unique_ptr<juce::AudioPluginInstance> plugin;
  std::unique_ptr<juce::Component> renderWindow,panelViewport;
  void *panelViewportOwner=nullptr;
  std::unique_ptr<juce::AudioProcessorEditor> engineEditor;
  juce::Rectangle<int> viewportBounds{0,0,600,600};
  bool fittingViewport=false;
  int hiddenPanelHeader=0;
  mutable std::recursive_mutex geometryLock;
  juce::Image cachedKeyboardGraph;
  std::atomic<Workspace> workspace{Workspace::lab};
  std::atomic<bool> paused{false},captureFrames{false};
  std::atomic<bool> homePending{false};
  std::atomic<bool> presentationChanging{false},panelCovered{false};
  std::atomic<void *> panelWindow{nullptr},hostPeerWindow{nullptr};
  std::atomic<juce::uint64> revealAfterPaint{0};
  std::atomic<unsigned> contextRestarts{0};
  EngineSurface surface;
  int midiTile = 0;
  std::atomic<bool> componentPainting{true};
  std::array<juce::AudioProcessorParameter *, 31> parameters{};
  std::array<float, 31> previousParameters{};
  mutable juce::CriticalSection frameLock, controlLock;
  juce::CriticalSection homeLock;
  std::function<void()> homeAction;
  std::function<void()> mappingAction;
  std::function<bool(const juce::KeyPress &)> performanceKeyAction;
  juce::Image frame;
#ifdef GRID_ENGINE_PROBE
  juce::Image presentedPanel;
  bool pixelCopyVerified=true;
#endif
  std::vector<juce::uint8> readbackBytes;
  juce::var observed;
  juce::var exactValues;
  SceneState controls;
  juce::StringArray pendingLyrics;
  juce::String lyricsSignature, appliedLyrics;
  int activeLyric = 0;
  juce::String failure;
  juce::File loadedBundle;
  std::deque<std::pair<juce::File, int>> pendingImports;
  void *editorObject = nullptr, *renderer = nullptr;
  uintptr_t moduleBase = 0;
  void *moduleReference = nullptr;
  std::array<void *, 4> renderVtable{};
  void **originalVtable = nullptr;
  std::array<void *,57> editorVtable{};
  void **originalEditorVtable=nullptr;
  std::atomic<juce::uint64> frameCount{0}, audioFrames{0};
  std::atomic<juce::uint64> panelPaintCount{0};
  double lastFrameTime = 0;
  std::atomic<double> frameInterval{0},sceneCpuMs{0},readbackCpuMs{0},copyCpuMs{0};
  std::atomic<juce::uint64> renderDimensions{0};
  int lastSwapInterval = -1;
  juce::AbstractFifo audioFifo{131072};
  std::array<std::array<float, 131072>, 2> audioRing{};
  juce::SpinLock audioProducer;
  std::atomic<double> inputRate{48000};
  std::atomic<bool> acceptingAudio{false};
  juce::WaitableEvent audioAvailable;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VelEngine)
};
} // namespace kone::grid
