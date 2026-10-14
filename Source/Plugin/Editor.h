#pragma once
#include "Core/Cycles.h"
#include "Core/Graphics.h"
#include "Core/Presets.h"
#include "Output/Recorder.h"
#include "Processor.h"
#include "Visual/Canvas.h"
#if GRID_USE_VEL_ENGINE
#include "Engine/VelEngine.h"
#endif
#include <juce_audio_utils/juce_audio_utils.h>
namespace kone::grid {
class CCControl final : public juce::Component {
public:
  CCControl(Processor &, juce::String, juce::String, bool button = false,
            bool swatch = false);
  void paint(juce::Graphics &) override;
  void refresh();
  void mouseDown(const juce::MouseEvent &) override;
  void mouseDrag(const juce::MouseEvent &) override;
  juce::String id, label;
  bool button = false, swatch = false, selected = false;
  std::function<bool()> select;
  std::function<void()> context;

private:
  Processor &processor;
  int start = 0;
  float anchor = 0;
  int shownValue=-1;
  bool shownSelected=false;
  juce::String shownLabel;
  juce::uint32 shownColour=0;
};
class ToolButton final : public juce::TextButton {
public:
  ToolButton(const juce::String &text, const juce::String &icon = {});
  void paintButton(juce::Graphics &, bool, bool) override;
  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu()) {
      if (context)
        context();
      return;
    }
    juce::TextButton::mouseDown(e);
  }
  std::function<void()> context;
  juce::String icon;
  juce::Colour iconColour;
};
class ControlsSurface;
class Editor final : public juce::AudioProcessorEditor,
                     private juce::Timer,
                     public juce::FileDragAndDropTarget {
public:
  explicit Editor(Processor &, juce::File presetRoot = {});
  ~Editor() override;
  void paint(juce::Graphics &) override;
  void paintOverChildren(juce::Graphics &) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent &) override;
  bool keyPressed(const juce::KeyPress &) override;
  bool isInterestedInFileDrag(const juce::StringArray &) override;
  void filesDropped(const juce::StringArray &, int, int) override;
  void fileDragEnter(const juce::StringArray &, int, int) override;
  void fileDragMove(const juce::StringArray &, int, int) override;
  void fileDragExit(const juce::StringArray &) override;
  void
  setAudioSettingsAction(std::function<std::unique_ptr<juce::Component>()> f) {
    audioSettings = std::move(f);
  }
  void setSystemAudioAction(std::function<juce::Result(bool)> f) {
    systemAudio = std::move(f);
  }
  void runRenderChecks(const juce::File &, std::function<void()>,
                       bool effectsOnly = false);
  void runTransitionChecks(const juce::File &,std::function<void()>);
  void runRecordingChecks(const juce::File &, std::function<void()>);
  juce::String runUiChecks(const juce::File &);

private:
  friend class ControlsSurface;
  void timerCallback() override;
  void advanceCycles(unsigned beats,double elapsedSeconds);
  juce::Component &uiHost();
  void paintUi(juce::Graphics &);
  void moveUiTo(juce::Component &);
  void togglePopOut();
  void popOutMenu();
  void syncControlsWindow();
  void setUiHidden(bool);
  void updatePresentation();
  void selectWorkspace(Workspace, bool home = false);
  void categoryMenu();
  void effectMenu(int);
  void intervalMenu(const juce::String &, bool seconds = false);
  void intervalDialog(const juce::String &, bool lyric = false);
  bool learn(const juce::String &, int fixed = -1);
  void nextPreset(int = 1, bool automatic = false);
  void refreshAssets();
  void importAsset(const juce::File &);
  void saveDialog();
  void recordDialog();
  juce::Result startRecording(const RecordSettings &);
  void settingsPage();
  void advancedGraphics();
  void audioPage();
  void performanceMapping();
  bool performanceKeyPressed(const juce::KeyPress &);
  void updatePerformanceKeys();
  void navigateBack();
  void mountSettingsBack();
  void applyGraphicsTier(int);
  int currentGraphicsTier() const;
  void closePanel();
  void rebuildLyrics();
  void setLyrics();
  void reset();
  void sync();
  void saveReviewCapture(juce::Image, bool overlay);
  void showMessage(juce::String);
  void stateFile(bool saving);
  juce::Result writeStateFile(const juce::File &);
  juce::Result readStateFile(const juce::File &);
  void selectEncoderFile(const juce::File &);
  void toggleFullScreen();
  void setWindowMode(int);
  Processor &processor;
  Presets presets;
  Recorder recorder;
  Canvas canvas;
#if GRID_USE_VEL_ENGINE
  std::shared_ptr<VelEngine> nativeEngine;
  juce::String nativeEngineError;
  juce::uint64 lastNativeFrame = 0;
  juce::String nativeImagePath, nativeFontPath;
  int nativeImageCategory = -1;
#endif
  std::unique_ptr<juce::Timer> transitionCheckTimer;
  std::unique_ptr<juce::DocumentWindow> controlsWindow;
  std::unique_ptr<ControlsSurface> controlsSurface;
  bool dragOnControls=false;
  bool performanceLive=false;
  bool midiStrobes=false,midiBlackout=false,autoKeyDown=false,autoKeyChord=false;
  double autoKeyStart=0;
  bool captureOverlay=true;
  bool quietDesktopChecks=false;
  bool captureExcluded=false;
  int captureExclusionError=0;
  juce::String controlsShapeSignature;
  int windowMode=0,windowedTitleHeight=28;
  bool windowedNativeTitle=true,windowedResizable=true;
  juce::Rectangle<int> windowedBounds;
  std::unique_ptr<juce::DocumentWindow> outputWindow;
  Canvas *outputCanvas = nullptr;
  Assets assets;
  NativeDynamics nativeDynamics;
  juce::Viewport lyricViewport;
  juce::Component lyricBody;
  std::vector<std::unique_ptr<CCControl>> knobs;
  std::vector<std::unique_ptr<juce::TextEditor>> lyricEditors;
  std::vector<std::unique_ptr<juce::TextButton>> lyricNumbers, lyricRemove;
  ToolButton home{"MENU"}, record{"", "record"}, category{"OSCILLOSCOPE"},
      previous{"", "up"}, next{"", "down"}, save{"", "save"}, midi{"MIDI"},
      workspaceAction{"PLAY"}, full{"", "full"}, popOut{"", "popout"}, hide{"", "hide"};
  ToolButton lockButton{"", "lock"}, resetButton{"", "reset"}, preset{"WAVE"},
      textFx{"WARP"}, lyricCycle{"", "cycle"}, addLyric{"+"}, clearLyrics{"x"},
      clearImage{"x"};
  ToolButton lab{"LAB"}, multiple{"MULTIPLE"},
      performanceButton{"PERFORMANCE"}, settings{"SETTINGS"},
      back{"BACK"};
  juce::Label status;
  juce::ComboBox midiTile;
  std::unique_ptr<juce::Component> panel;
  std::vector<std::unique_ptr<juce::Component>> panelWidgets;
  std::unique_ptr<juce::FileChooser> chooser;
  std::function<std::unique_ptr<juce::Component>()> audioSettings;
  std::function<juce::Result(bool)> systemAudio;
  enum class SettingsPage { none, general, graphics, audio };
  SettingsPage settingsScreen = SettingsPage::none;
  juce::String loadedImage, loadedFont, loadedLyrics, loadedStyleFonts,
      selectedControl;
  int selectedFixed = -1, lastCategory = -1;
  bool bindingPrompt = false, dragging = false, isHome = true, uiHidden = false,
       lyricsDirty = false;
  double time = 0, lastTick = 0;
  std::array<BeatCycle, 3> colorClocks;
  BeatCycle presetClock, lyricClock;
  double hostClockTime = 0, lastHostBpm = 0;
  double lyricSeconds = 0;
  bool lyricSecondsActive = false;
  int lastLyricInterval = -1;
  int fps = 60, quality = 2;
  bool showFps = false;
  juce::File reviewFolder;
  int reviewIndex = 0;
  int reviewApplied = -1;
  struct ReviewCase {
    int category, variant, fx = -1, profile = 0, textEffect = 0;
    juce::String name;
    juce::String imagePath, fontPath;
    int fixture = 0, amount = 90;
  };
  std::vector<ReviewCase> reviewCases;
  int reviewFailures = 0;
  int reviewPresented = 0;
  std::atomic<bool> reviewEnabled{false}, reviewWaiting{false};
  std::atomic<int> reviewHold{0}, reviewTarget{0};
  bool overlaySnapshot = false;
  bool imageTraceReady = false;
  std::function<void()> checksFinished;
  bool recordingCheck = false;
  std::function<void()> recordingCheckTick;
  std::array<int, 12> actionValues{};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};
} // namespace kone::grid
