#include "Editor.h"
#if JUCE_WINDOWS
#include <windows.h>
#undef small
#endif
namespace kone::grid {
juce::AudioProcessorEditor *Processor::createEditor() {return new Editor(*this);}
static juce::Colour bg(0xff101011), accent(0xff00dd91), muted(0xff858589);
static const char *fxIDs[] = {"texture", "dither", "track", "glass"};
class ControlsSurface final : public juce::Component,public juce::FileDragAndDropTarget {
public:
  explicit ControlsSurface(Editor &editor):owner(editor){setOpaque(true);setWantsKeyboardFocus(true);}
  void paint(juce::Graphics &g) override {g.fillAll(bg);owner.paintUi(g);}
  bool hitTest(int x,int y) override {
    if(!owner.captureOverlay || owner.isHome || owner.dragging)return true;
    for(auto *child:getChildren())if(child->isVisible() && child->getBounds().contains(x,y))return true;
    return y<82 || (owner.processor.workspace()==Workspace::lab && y>=getHeight()-142);
  }
  void resized() override {owner.resized();}
  bool keyPressed(const juce::KeyPress &key) override {return owner.keyPressed(key);}
  void mouseDown(const juce::MouseEvent &event) override {owner.mouseDown(event);}
  bool isInterestedInFileDrag(const juce::StringArray &files) override {return owner.isInterestedInFileDrag(files);}
  void fileDragEnter(const juce::StringArray &files,int x,int y) override {owner.dragOnControls=true;owner.fileDragEnter(files,x,y);repaint();}
  void fileDragMove(const juce::StringArray &files,int x,int y) override {owner.fileDragMove(files,x,y);}
  void fileDragExit(const juce::StringArray &files) override {owner.fileDragExit(files);owner.dragOnControls=false;repaint();}
  void filesDropped(const juce::StringArray &files,int x,int y) override {owner.filesDropped(files,x,y);owner.dragOnControls=false;repaint();}
  void paintOverChildren(juce::Graphics &g) override {
    if(owner.dragging && owner.dragOnControls) {
      g.setColour(juce::Colours::black.withAlpha(.7f));g.fillAll();g.setColour(accent);
      g.setFont(juce::FontOptions(16,juce::Font::bold));
      g.drawText("DROP IMAGE, FONT, OR TEXT / LYRICS",getLocalBounds(),juce::Justification::centred);
    }
  }
private:Editor &owner;
};

CCControl::CCControl(Processor &p, juce::String i, juce::String l, bool b,
                     bool c)
    : id(std::move(i)), label(std::move(l)), button(b), swatch(c),
      processor(p) {
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
void CCControl::refresh() {
  const int value=processor.value(id.toRawUTF8());
  const auto colour=swatch?processor.visualColour(id.toRawUTF8(),
      id=="color" && decodeChoice(processor.value("category"),9)==7).getARGB():0;
  if(value!=shownValue || selected!=shownSelected || label!=shownLabel || colour!=shownColour) {
    shownValue=value;shownSelected=selected;shownLabel=label;shownColour=colour;repaint();
  }
}
void CCControl::paint(juce::Graphics &g) {
  int v = processor.value(id.toRawUTF8());
  g.setColour(selected ? accent : juce::Colours::white);
  g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
  if (!button)
    g.drawText(label, 0, 0, getWidth(), 18, juce::Justification::centred);
  auto r = getLocalBounds().toFloat().reduced(8);
  if (!button)
    r.removeFromTop(18);
  if (swatch) {
    g.setColour(processor.visualColour(id.toRawUTF8(),
        id == "color" && decodeChoice(processor.value("category"), 9) == 7));
    g.fillRoundedRectangle(r, 6);
    g.setColour(selected ? accent : muted);
    g.drawRoundedRectangle(r, 6, 1);
  } else if (button) {
    if (label == "o") {
      auto d = r.withSizeKeepingCentre(14, 14);
      if (v >= 64)
        g.fillEllipse(d);
      else
        g.drawEllipse(d, 1.5f);
    } else {
      g.setColour(v >= 64 ? accent : muted);
      if (v >= 64) {
        g.fillRoundedRectangle(r, 4);
        g.setColour(juce::Colours::black);
      }
      g.drawRoundedRectangle(r, 4, 1.2f);
      g.drawText(label, r, juce::Justification::centred);
    }
  } else {
    float size = juce::jmin(r.getWidth(), r.getHeight());
    auto circle = r.withSizeKeepingCentre(size, size);
    g.drawEllipse(circle, 1.1f);
    float angle = juce::MathConstants<float>::pi * (.75f + v / 127.0f * 1.5f);
    auto c = circle.getCentre();
    g.drawLine(c.x, c.y, c.x + std::cos(angle) * size * .42f,
               c.y + std::sin(angle) * size * .42f, 1.2f);
    if (isMouseOver()) {
      g.setColour(muted);
      g.setFont(juce::FontOptions(10));
      g.drawText(juce::String(v), circle, juce::Justification::centred);
    }
  }
}
void CCControl::mouseDown(const juce::MouseEvent &e) {
  if (e.mods.isPopupMenu()) {
    if (context)
      context();
    return;
  }
  if (select && select())
    return;
  if (swatch && context) {
    context();
    return;
  }
  if (button) {
    processor.set(id.toRawUTF8(),
                  processor.value(id.toRawUTF8()) >= 64 ? 0 : 127);
    repaint();
    return;
  }
  start = processor.value(id.toRawUTF8());
  anchor = e.position.y;
}
void CCControl::mouseDrag(const juce::MouseEvent &e) {
  if (button || swatch || processor.learning.load() || e.mods.isPopupMenu())
    return;
  processor.set(id.toRawUTF8(),
                start + juce::roundToInt((anchor - e.position.y) *
                                         (e.mods.isShiftDown() ? .2f : 1.0f)));
  repaint();
}
ToolButton::ToolButton(const juce::String &s, const juce::String &i)
    : TextButton(s), icon(i) {}
void ToolButton::paintButton(juce::Graphics &g, bool over, bool down) {
  auto r = getLocalBounds().toFloat().reduced(2);
  auto c = !isEnabled() ? muted.withAlpha(.45f)
           : getToggleState()
               ? accent
               : ((over || getHeight() >= 45) ? juce::Colours::white : muted);
  if(icon=="full")c=iconColour.getAlpha()!=0?iconColour:muted;
  auto fill = down ? juce::Colour(0xff444448) : juce::Colour(0xff2c2c30);
  if (icon.isEmpty() && isEnabled() && getButtonText() == "CANCEL") {
    fill = juce::Colour(0xff54181b);
    c = juce::Colour(0xffff5555);
  } else if (icon.isEmpty() && isEnabled() && getButtonText() == "RECORD") {
    fill = juce::Colour(0xff8b2428);
    c = juce::Colours::white;
  }
  g.setColour(fill);
  if (icon.isEmpty()) {
    g.fillRoundedRectangle(r, 10);
    g.setColour(c);
    g.drawRoundedRectangle(r, 10, 1.2f);
    g.setFont(
        juce::FontOptions(getHeight() >= 50 ? 18.0f : 12.0f, juce::Font::bold));
    g.drawText(getButtonText(), r, juce::Justification::centred);
    return;
  }
  g.setColour(c);
  auto b = r.withSizeKeepingCentre(23, 23);
  float x = b.getX(), y = b.getY();
  if (icon == "save") {
    g.drawRoundedRectangle(b, 2, 1.5f);
    g.drawRect(x + 5, y, 12.0f, 8.0f, 1.4f);
    g.drawRect(x + 5, y + 13, 13.0f, 10.0f, 1.4f);
  } else if (icon == "popout") {
    g.drawRoundedRectangle(x,y+3,23,17,1.5f,1.7f);
    g.fillRoundedRectangle(x+11,y+11,10,7,1.0f);
  } else if (icon == "record") {
    g.setColour(
        getToggleState()
            ? juce::Colours::red.withAlpha(
                  .45f +
                  .55f *
                      (float)(.5 +
                              .5 *
                                  std::sin(
                                      juce::Time::getMillisecondCounterHiRes() *
                                      .004)))
            : muted);
    g.fillEllipse(b);
  } else if (icon == "lock") {
    juce::Path shackle;
    if (getToggleState()) {
      shackle.startNewSubPath(x + 6, y + 12);
      shackle.lineTo(x + 6, y + 7);
      shackle.cubicTo(x + 6, y, x + 17, y, x + 17, y + 7);
      shackle.lineTo(x + 17, y + 12);
    } else {
      shackle.startNewSubPath(x + 10, y + 12);
      shackle.lineTo(x + 10, y + 7);
      shackle.cubicTo(x + 10, y, x + 21, y, x + 21, y + 7);
    }
    g.strokePath(shackle, juce::PathStrokeType(2.2f));
    g.fillRoundedRectangle(x + 3, y + 10, 17, 13, 2.5f);
    g.setColour(juce::Colour(0xff151518));
    g.fillEllipse(x + 10, y + 14, 3, 3);
    g.fillRect(x + 11, y + 16, 1.2f, 3.5f);
  } else if (icon == "reset" || icon == "cycle") {
    juce::Graphics::ScopedSaveState saved(g);
    if (icon == "cycle")
      g.addTransform(juce::AffineTransform::scale(-1.0f, 1.0f)
                         .translated((float)getWidth(), 0));
    juce::Path p;
    p.addCentredArc(b.getCentreX(), b.getCentreY(), 8, 8, 0, .4f, 5.5f, true);
    g.strokePath(p, juce::PathStrokeType(1.7f));
    g.drawLine(x + 16, y + 3, x + 20, y + 7, 1.7f);
  } else if (icon == "up" || icon == "down") {
    juce::Path p;
    bool up = icon == "up";
    p.addTriangle(x, y + (up ? 20 : 3), x + 23, y + (up ? 20 : 3), x + 12,
                  y + (up ? 3 : 20));
    g.fillPath(p);
  } else if (icon == "full") {
    g.drawLine(x, y + 7, x, y, 1.5f);
    g.drawLine(x, y, x + 7, y, 1.5f);
    g.drawLine(x + 23, y + 16, x + 23, y + 23, 1.5f);
    g.drawLine(x + 23, y + 23, x + 16, y + 23, 1.5f);
  } else {
    g.drawEllipse(b.withTrimmedTop(6).withTrimmedBottom(6), 1.3f);
    g.drawLine(x, y, x + 23, y + 23, 1.4f);
  }
}
Editor::Editor(Processor &p, juce::File presetRoot)
    : AudioProcessorEditor(p), processor(p), presets(p, presetRoot) {
  quietDesktopChecks=juce::SystemStats::getEnvironmentVariable("GRID_TEST_QUIET_DESKTOP","")=="1";
  // Restore capture-excluded, in-place controls for existing profiles too.
  // A separate controls window remains an explicit right-click alternative.
  if(processor.getAssetProperty("captureOverlayDefaultVersion")!="1") {
    processor.setAssetProperty("controlsPresentation","overlay");
    processor.setAssetProperty("captureOverlayDefaultVersion","1");
  }
  captureOverlay=processor.getAssetProperty("controlsPresentation")!="window";
  if(processor.getAssetProperty("reloadDefaultPreset")=="1") {
    const juce::File selected(processor.getAssetProperty("presetFile"));
    if(selected.existsAsFile()) presets.load(selected);
    processor.setAssetProperty("reloadDefaultPreset","0");
  }
  setSize(1120, 740);
  setResizable(true, true);
  setResizeLimits(960, 620, 2560, 1600);
  setWantsKeyboardFocus(true);
  const auto styleFonts =
      juce::SystemStats::getEnvironmentVariable("GRID_STYLE_FONTS", "");
  if (styleFonts.isNotEmpty())
    processor.setAssetProperty("styleFontRoot", styleFonts);
  canvas.setInterceptsMouseClicks(false, false);
  addAndMakeVisible(canvas);
  lyricViewport.setViewedComponent(&lyricBody, false);
  lyricViewport.setScrollBarsShown(true, false);
  addAndMakeVisible(lyricViewport);
  for (auto *b :
       {&home,        &record,     &category, &previous,   &next,
        &save,        &midi,       &workspaceAction, &full,     &popOut, &hide, &lockButton,
        &resetButton, &preset,     &textFx,   &lyricCycle, &addLyric,
        &clearLyrics, &clearImage, &lab,      &multiple,   &performanceButton,
        &settings,    &back})
    addAndMakeVisible(b);
  status.setInterceptsMouseClicks(false,false);
  status.setColour(juce::Label::textColourId, muted);
  status.setFont(juce::FontOptions(11));
  addAndMakeVisible(status);
  home.onClick = [this] {
    selectWorkspace(controlsWindow && processor.workspace()!=Workspace::performance?processor.workspace():Workspace::lab,true);
    closePanel();
    resized();
  };
  lab.onClick = [this] {
    selectWorkspace(Workspace::lab);
  };
  multiple.onClick=[this]{selectWorkspace(Workspace::multiple);};
  performanceButton.onClick=[this]{selectWorkspace(Workspace::performance);};
#if !GRID_USE_VEL_ENGINE || GRID_DISABLE_ADVANCED_WORKSPACES
  multiple.setEnabled(false);
  performanceButton.setEnabled(false);
#endif
  settings.onClick = [this] { settingsPage(); };
  back.setComponentID("settings-back");
  back.onClick = [this] { navigateBack(); };
  category.onClick = [this] { categoryMenu(); };
  previous.onClick = [this] { nextPreset(-1); };
  next.onClick = [this] { nextPreset(1); };
  preset.onClick = [this] { nextPreset(); };
  save.onClick = [this] {
#if GRID_USE_VEL_ENGINE
    if(processor.workspace()==Workspace::multiple && nativeEngine){nativeEngine->saveMultiplePreset();return;}
#endif
    saveDialog();
  };
  workspaceAction.onClick=[this] {
#if GRID_USE_VEL_ENGINE
    if(!nativeEngine)return;
    if(processor.workspace()==Workspace::performance)nativeEngine->startPerformance();
    else if(processor.workspace()==Workspace::multiple)nativeEngine->finishMultipleLayout();
    timerCallback();resized();
#endif
  };
  record.onClick = [this] {
    if (recorder.active()) {
      recorder.requestStop();
    } else
      recordDialog();
  };
  addChildComponent(midiTile);
  for(int tile=1;tile<=36;++tile)midiTile.addItem("Tile "+juce::String(tile),tile);
  midiTile.setTooltip("Multiple MIDI target: incoming controls change this tile.");
  midiTile.onChange=[this] {
    const int cc=encodeChoice(midiTile.getSelectedId()-1,36);
    processor.set("multiTile",cc);
#if GRID_USE_VEL_ENGINE
    if(nativeEngine)nativeEngine->applyMappedControl("multiTile",cc,false);
#endif
  };
  midi.context=[this] {
    juce::PopupMenu menu;
    menu.addItem(1,"Clear "+juce::String(workspaceName(processor.workspace()))+" MIDI bindings");
    juce::Component::SafePointer<Editor> safe(this);
    const auto context=processor.workspace();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&midi),[safe,context](int result) {
      if(safe && result==1 && safe->processor.workspace()==context) {
        safe->processor.clearBindings();safe->showMessage("Cleared this workspace's MIDI bindings.");
      }
    });
  };
  midi.onClick = [this] {
    if(processor.workspace()==Workspace::performance) {performanceMapping();return;}
    processor.learning = !processor.learning.load();
    selectedControl.clear();
    selectedFixed = -1;
    resized();
    status.setText(
        processor.learning
            ? "MIDI Learn: select a control, then move or press its MIDI CC."
            : "",
        juce::dontSendNotification);
  };
  hide.onClick = [this] {setUiHidden(!uiHidden);};
  hide.setTooltip("Hide all controls; click the visual output to restore them");
  popOut.setTooltip("Keep controls over this window, excluded from capture; right-click for a separate window");
  popOut.onClick=[this]{togglePopOut();};
  popOut.context=[this]{popOutMenu();};
  full.onClick = [this] { toggleFullScreen(); };
  full.setTooltip("Windowed: click for borderless (green), then fullscreen (red), then windowed (grey)");
  lockButton.onClick = [this] {
    if (learn("lock"))
      return;
    processor.set("lock", processor.value("lock") >= 64 ? 0 : 127);
  };
  resetButton.onClick = [this] { reset(); };
  textFx.onClick = [this] {
    juce::PopupMenu m;
    for (int i = 0; i < textEffects.size(); ++i)
      m.addItem(i + 1, textEffects[i]);
    juce::Component::SafePointer<Editor> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&textFx),
                    [safe](int i) {
                      if (safe && i > 0) {
                        int v = encodeChoice(i - 1, 7);
                        if (!safe->learn("textEffect", v))
                          safe->processor.set("textEffect", v);
                      }
                    });
  };
  lyricCycle.onClick = [this] {
    if (learn("lyricCycle"))
      return;
    processor.set("lyricCycle", processor.value("lyricCycle") >= 64 ? 0 : 127);
  };
  lyricCycle.context = [this] {
    juce::PopupMenu menu, beats, seconds;
    for (int v : {1, 2, 4, 8, 16, 32, 64, 128})
      beats.addItem(v, juce::String(v) + " beats");
    for (int v : {1, 2, 3, 5, 10, 15, 30, 60, 120})
      seconds.addItem(1000 + v, juce::String(v) + " seconds");
    menu.addSubMenu("Beats", beats);
    menu.addSubMenu("Seconds", seconds);
    menu.addItem(2000, "Custom interval / MIDI binding...");
    juce::Component::SafePointer<Editor> safe(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&lyricCycle),
        [safe](int id) {
          if (!safe || id <= 0)
            return;
          if (id == 2000) {
            safe->intervalDialog("lyricInterval", true);
            return;
          }
          bool seconds = id > 1000;
          if (seconds)
            id -= 1000;
          safe->processor.set("lyricTimeMode", seconds ? 127 : 0);
          safe->processor.set("lyricInterval", id - 1);
        });
  };
  addLyric.onClick = [this] {
    setLyrics();
    int selected = decodeChoice(processor.value("lyric"), assets.lyrics.size());
    if (assets.lyrics.size() <
#if GRID_USE_VEL_ENGINE
      75
#else
      128
#endif
    ) {
      assets.lyrics.add("");
      processor.set("lyric", encodeChoice(selected, assets.lyrics.size()));
      processor.setAssetProperty(
          "lyrics", juce::JSON::toString(juce::var(assets.lyrics)));
      rebuildLyrics();
    }
  };
  clearLyrics.onClick = [this] {
    assets.lyrics = {""};
    processor.set("lyric", 0);
    processor.setAssetProperty("lyrics", "[\"\"]");
    rebuildLyrics();
  };
  clearImage.onClick = [this] {
    processor.setAssetProperty("imagePath", "");
    refreshAssets();
  };
  const char *actions[] = {"nextPreset",  "previousPreset", "resetFx",
                           "savePreset",  "recordVideo",    "hideUI",
                           "fullscreen",  "home",           "addLyric",
                           "clearLyrics", "clearImage", "popOutUI"};
  ToolButton *buttons[] = {&next,     &previous,    &resetButton, &save,
                           &record,   &hide,        &full,        &home,
                           &addLyric, &clearLyrics, &clearImage, &popOut};
  for (int i = 0; i < 12; ++i) {
    auto id = juce::String(actions[i]);
    actionValues[(size_t)i] = processor.value(actions[i]);
    auto action = buttons[i]->onClick;
    buttons[i]->onClick = [this, id, action] {
      if (!learn(id) && action)
        action();
    };
  }
  preset.onClick = [this] {
    if(learn("nextPreset"))return;
#if GRID_USE_VEL_ENGINE
    if(processor.workspace()==Workspace::multiple && nativeEngine) {nativeEngine->openMultiplePresets();resized();return;}
#endif
    nextPreset();
  };
  textFx.context = textFx.onClick;
  for (auto control : controls) {
    juce::String id(control.id);
    if (control.kind == Kind::choice || id == "lock" || id == "lyricCycle" ||
        id == "muteInput" || id == "lyric" || id.contains("Interval") ||
        id == "lyricTimeMode")
      continue;
    bool small = id.endsWith("React");
    bool cycle = id.endsWith("Cycle");
    auto c = std::make_unique<CCControl>(p, id,
                                         small                ? "o"
                                         : cycle              ? "CYCLE"
                                         : id == "int"        ? "INT"
                                         : id == "in"         ? "IN"
                                         : id == "imageScale" ? "IMG"
                                         : id == "color2"     ? "COLOR II"
                                         : id == "textColor"  ? "TEXT"
                                         : id.startsWith("perf") ? juce::String(control.name).toUpperCase()
                                                             : id.toUpperCase(),
                                         control.kind == Kind::button,
                                         id == "color" || id == "color2" ||
                                             id == "textColor");
    auto *ptr = c.get();
    c->select = [this, id] { return learn(id); };
    c->context = [this, id] {
      for (int i = 0; i < 4; ++i)
        if (id == fxIDs[i]) {
          effectMenu(i);
          return;
        }
      if (id.endsWith("Cycle")) {
        intervalMenu(id);
        return;
      }
      if (id == "color" || id == "color2" || id == "textColor") {
        juce::PopupMenu m;
        for (int v = 0; v < 128; ++v)
          m.addColouredItem(
              v + 1, "Color " + juce::String(v),
              palette(v,
                      id == "color" &&
                          decodeChoice(processor.value("category"), 9) == 7));
        juce::Component::SafePointer<Editor> safe(this);
        m.showMenuAsync(juce::PopupMenu::Options(), [safe, id](int v) {
          if (safe && v > 0)
            safe->processor.set(id.toRawUTF8(), v - 1);
        });
      }
    };
    addAndMakeVisible(ptr);
    knobs.push_back(std::move(c));
  }
  if (processor.getAssetProperty("lyrics").isEmpty())
    processor.setAssetProperty("lyrics", "[\"GRID-ARVx\"]");
  fps = frameRate(processor.value("frameCap"));
  quality = decodeChoice(processor.value("renderDetail"), 7);
  canvas.onCapture = [this](juce::Image im, int tag) {
    if (!reviewEnabled) {
      recorder.submit(std::move(im));
      return;
    }
    if (tag != reviewTarget.load() || ++reviewHold < 16)
      return;
    if (reviewWaiting.exchange(true))
      return;
    juce::Component::SafePointer<Editor> safe(this);
    juce::MessageManager::callAsync([safe, im] {
      if (!safe)
        return;
#if JUCE_WINDOWS
      if (safe->reviewIndex == 0) {
        auto scene = recordingFrame(im, im.getWidth(), im.getHeight());
        if (auto stream = safe->reviewFolder.getChildFile("scene-only.png")
                              .createOutputStream())
          juce::PNGImageFormat().writeImageToStream(scene, *stream);
      }
      safe->canvas.capturePresented([safe, im](juce::Image presented) {
        juce::MessageManager::callAsync([safe, im, presented] {
          if (!safe)
            return;
          if (!presented.isValid()) {
            ++safe->reviewFailures;
            safe->saveReviewCapture(im, true);
          } else {
            ++safe->reviewPresented;
            safe->saveReviewCapture(presented, false);
          }
        });
      });
#else
      safe->saveReviewCapture(im, true);
#endif
    });
  };
  refreshAssets();
  rebuildLyrics();
#if GRID_USE_VEL_ENGINE
  nativeEngine = std::make_shared<VelEngine>();
  if (auto result = nativeEngine->open(VelEngine::adjacentBundle(),presetRoot==juce::File{}?juce::File{}:presetRoot.getParentDirectory().getChildFile("EngineProfile")); result.failed()) {
    nativeEngineError = result.getErrorMessage();
    showMessage(nativeEngineError);
  } else {
    nativeEngine->setHomeAction([safe=juce::Component::SafePointer<Editor>(this)] {
      if(!safe)return;
      safe->selectWorkspace(safe->controlsWindow?safe->processor.workspace():Workspace::lab,true);
      safe->closePanel();
    });
    nativeEngine->setMappingAction([safe=juce::Component::SafePointer<Editor>(this)] {if(safe)safe->performanceMapping();});
    nativeEngine->setPerformanceKeyAction([safe=juce::Component::SafePointer<Editor>(this)](const juce::KeyPress &key) {
      return safe?safe->performanceKeyPressed(key):false;
    });
    processor.velEngine.store(nativeEngine);
    if(auto updated=presets.refreshNativeProfiles(*nativeEngine);updated.failed())showMessage(updated.getErrorMessage());
    const juce::File selected(processor.getAssetProperty("presetFile"));
    if(selected.existsAsFile() && selected.getParentDirectory().getFileName()=="Defaults")presets.load(selected);
    else if(processor.getAssetProperty("freshStart")=="1") {
      auto defaults=presets.files(0,"Defaults");
      if(!defaults.isEmpty())presets.load(defaults[0]);
    }
    processor.setAssetProperty("freshStart", "0");
    showMessage("Vel Engine / Windows build");
  }
#endif
  lastTick = juce::Time::getMillisecondCounterHiRes() * .001;
  startTimerHz(fps);
  resized();
  updatePresentation();
}
void Editor::saveReviewCapture(juce::Image im, bool overlay) {
#if GRID_USE_VEL_ENGINE
  if(nativeEngine) {
    auto observed=nativeEngine->observedState();
    reviewFolder.getChildFile(juce::String(reviewIndex)+".native.json").replaceWithText(juce::JSON::toString(observed));
    if(reviewCases[(size_t)reviewIndex].fixture)
      if(auto stream=reviewFolder.getChildFile(juce::String(reviewIndex)+".native.png").createOutputStream())
        juce::PNGImageFormat().writeImageToStream(nativeEngine->latestFrame(),*stream);
    if(reviewCases[(size_t)reviewIndex].name=="Imported logo / traced" &&
       ((int)observed["scopeStyle"]!=15 || (juce::int64)observed["scopeTraceCount"]<=0))
      ++reviewFailures;
    if(reviewCases[(size_t)reviewIndex].name.contains("Swirl") || reviewCases[(size_t)reviewIndex].name.contains("Simple"))
      if(auto stream=reviewFolder.getChildFile(juce::String(reviewIndex)+"-native-raw.png").createOutputStream())
        juce::PNGImageFormat().writeImageToStream(nativeEngine->latestFrame(),*stream);
  }
#endif
  auto image = recordingFrame(im, im.getWidth(), im.getHeight());
  if (overlay) {
    overlaySnapshot = true;
    juce::Image layer(juce::Image::ARGB, getWidth(), getHeight(), true,
                      juce::SoftwareImageType());
    {
      juce::Graphics g(layer);
      paintEntireComponent(g, true);
    }
    overlaySnapshot = false;
    juce::Graphics g(image);
    g.drawImage(layer, image.getBounds().toFloat());
  } else {
    const float scale = (float)image.getWidth() / getWidth();
    const auto pixel =
        image.getPixelAt(juce::roundToInt((category.getX() + 12) * scale),
                         juce::roundToInt(36 * scale));
    if (std::abs((int)pixel.getRed() - 44) > 10 ||
        std::abs((int)pixel.getGreen() - 44) > 10 ||
        std::abs((int)pixel.getBlue() - 48) > 10)
      ++reviewFailures; // Category pill must appear in the actual presented GPU
                        // frame.
  }
  auto f = reviewFolder.getChildFile(
      juce::String(reviewIndex).paddedLeft('0', 2) + ".png");
  bool written = false;
  if (auto stream = f.createOutputStream())
    written = juce::PNGImageFormat().writeImageToStream(image, *stream);
  if (!written)
    ++reviewFailures;
  ++reviewIndex;
  reviewTarget = reviewIndex;
  reviewHold = 0;
  reviewWaiting = false;
  if (reviewIndex >= (int)reviewCases.size()) {
    reviewEnabled = false;
    canvas.capture = false;
    reviewFolder.getChildFile("result.txt")
        .replaceWithText("Rendered " + juce::String(reviewIndex) +
                         " cases.\nPresented GPU UI captures: " +
                         juce::String(reviewPresented) + "\n" +
                         (reviewFailures || canvas.error().isNotEmpty()
                              ? "FAIL "
                              : "PASS ") +
                         "PNG captures / shader status: " + canvas.error());
    if (checksFinished)
      checksFinished();
  }
}
Editor::~Editor() {
  transitionCheckTimer.reset();
  stopTimer();
#if GRID_USE_VEL_ENGINE
  processor.velEngine.store(nullptr);
  if (nativeEngine) {nativeEngine->setHomeAction({});nativeEngine->setMappingAction({});nativeEngine->setPerformanceKeyAction({});nativeEngine->shutdown();}
  nativeEngine.reset();
#endif
  canvas.detachUi();
  controlsWindow.reset();controlsSurface.reset();
  outputWindow.reset();
  outputCanvas = nullptr;
  processor.learning = false;
  canvas.capture = false;
  recorder.stop();
  setLyrics();
}
void Editor::showMessage(juce::String s) {
  status.setText(s, juce::dontSendNotification);
}
juce::Component &Editor::uiHost(){return controlsSurface?static_cast<juce::Component &>(*controlsSurface):static_cast<juce::Component &>(*this);}
void Editor::moveUiTo(juce::Component &host) {
  for(auto *component:std::initializer_list<juce::Component *>{&home,&record,&category,&previous,&next,&save,&midi,&workspaceAction,&full,&popOut,&hide,
      &lockButton,&resetButton,&preset,&textFx,&lyricCycle,&addLyric,&clearLyrics,&clearImage,
      &lab,&multiple,&performanceButton,&settings,&status,&midiTile,&lyricViewport})host.addChildComponent(component);
  if(back.getParentComponent()!=panel.get())host.addChildComponent(&back);
  for(auto &control:knobs)host.addChildComponent(control.get());
  if(panel)host.addChildComponent(panel.get());
}
void Editor::togglePopOut() {
  if(controlsWindow) {
#if GRID_USE_VEL_ENGINE
    // Release native child peers before Windows destroys their controls host.
    // The same engine is reattached below by resized(), with all state intact.
    if(nativeEngine)nativeEngine->showWorkspace(processor.workspace(),nullptr,{});
#endif
    moveUiTo(*this);controlsWindow.reset();controlsSurface.reset();uiHidden=false;captureExcluded=false;
  } else {
    if(isHome || (processor.workspace()==Workspace::performance && !performanceLive))return;
    class ControlsWindow final : public juce::DocumentWindow {
    public:
      ControlsWindow(bool over,std::function<void()> close)
          :DocumentWindow("GRID-ARVx Controls",bg,over?0:closeButton,false),
           overlay(over),closed(std::move(close)) {
        // Use an ordinary redirected window, shaped around controls. JUCE's
        // per-pixel UpdateLayeredWindow path rejects capture affinity on some
        // supported Windows versions (ERROR_NOT_ENOUGH_MEMORY).
        setOpaque(true);setUsingNativeTitleBar(!over);setTitleBarHeight(over?0:28);
        setResizable(!over,false);setResizeLimits(over?1:960,over?1:620,8192,8192);
        addToDesktop(getDesktopWindowStyleFlags());
      }
      void closeButtonPressed() override {if(closed)closed();}
      juce::BorderSize<int> getBorderThickness() const override {return overlay?juce::BorderSize<int>{}:DocumentWindow::getBorderThickness();}
      int getDesktopWindowStyleFlags() const override {
        return overlay?juce::ComponentPeer::windowIsTemporary:DocumentWindow::getDesktopWindowStyleFlags();
      }
    private:bool overlay;std::function<void()> closed;
    };
    controlsSurface=std::make_unique<ControlsSurface>(*this);
    juce::Component::SafePointer<Editor> safe(this);
    controlsWindow=std::make_unique<ControlsWindow>(captureOverlay,[safe]{juce::MessageManager::callAsync([safe]{if(safe && safe->controlsWindow)safe->togglePopOut();});});
    moveUiTo(*controlsSurface);
    controlsWindow->setContentNonOwned(controlsSurface.get(),false);
    if(captureOverlay)controlsWindow->setBounds(getScreenBounds());
    else {
      auto bounds=juce::Rectangle<int>(juce::jmax(960,getWidth()),juce::jmax(700,getHeight()))
          .withPosition(getScreenPosition().translated(48,48));
      if(auto *display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
        bounds=bounds.constrainedWithin(display->userArea);
      controlsWindow->setBounds(bounds);
    }
#if JUCE_WINDOWS
    auto handle=static_cast<HWND>(controlsWindow->getPeer()->getNativeHandle());
    if(captureOverlay && getPeer())
      SetWindowLongPtrW(handle,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(getPeer()->getNativeHandle()));
    // Exclude only this owned, top-level control layer. Never exclude the
    // output HWND: OBS must continue capturing the engine's scene.
#endif
    uiHidden=false;syncControlsWindow();controlsWindow->setVisible(true);controlsWindow->toFront(!quietDesktopChecks);
#if JUCE_WINDOWS
    // Assign affinity after final window geometry and visibility are established.
    captureExcluded=SetWindowDisplayAffinity(handle,0x11)!=FALSE;
    captureExclusionError=captureExcluded?0:(int)GetLastError();
#endif
    showMessage(captureExcluded?"Controls excluded from Windows capture; capture the main GRID-ARVx window."
                                :"Capture exclusion unavailable; right-click pop-out for a separate controls window.");
  }
  popOut.setToggleState(controlsWindow!=nullptr,juce::dontSendNotification);
  updatePresentation();resized();repaint();
}
void Editor::popOutMenu() {
  juce::PopupMenu menu;
#if JUCE_WINDOWS
  menu.addItem(1,"Controls over visuals (capture excluded)",true,captureOverlay);
#endif
  menu.addItem(2,"Separate controls window",true,!captureOverlay);
  juce::Component::SafePointer<Editor> safe(this);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&popOut),[safe](int choice){
    if(!safe || choice==0)return;
    const bool overlay=choice==1;
    if(safe->controlsWindow)safe->togglePopOut();
    safe->captureOverlay=overlay;safe->processor.setAssetProperty("controlsPresentation",overlay?"overlay":"window");safe->togglePopOut();
  });
}
void Editor::syncControlsWindow() {
  if(!controlsWindow || !captureOverlay)return;
  auto *mainPeer=getPeer();
  const bool visible=!uiHidden && isShowing() && mainPeer && !mainPeer->isMinimised();
  if(visible && controlsWindow->getBounds()!=getScreenBounds())controlsWindow->setBounds(getScreenBounds());
  if(controlsWindow->isVisible()!=visible)controlsWindow->setVisible(visible);
#if JUCE_WINDOWS
  if(!controlsWindow->getPeer())return;
  auto window=static_cast<HWND>(controlsWindow->getPeer()->getNativeHandle());
  const auto scale=controlsWindow->getPeer()->getPlatformScaleFactor();
  juce::RectangleList<int> regions;
  const auto area=controlsSurface->getLocalBounds();
  if(isHome || dragging || processor.workspace()==Workspace::multiple)regions.add(area);
  else {
    regions.add(area.withHeight(82));
    if(processor.workspace()==Workspace::lab)regions.add(area.withTrimmedTop(juce::jmax(0,area.getHeight()-142)));
    for(auto *child:controlsSurface->getChildren())if(child->isVisible())
      regions.add(child->getBounds().getIntersection(area));
  }
  juce::String signature=juce::String::toHexString((juce::int64)(uintptr_t)window)+"/"+juce::String(scale);
  for(const auto &rectangle:regions)signature+="/"+rectangle.toString();
  if(signature!=controlsShapeSignature) {
    auto combined=CreateRectRgn(0,0,0,0);
    for(const auto &rectangle:regions) {
      auto physical=(rectangle.toFloat()*scale).getSmallestIntegerContainer();
      auto part=CreateRectRgn(physical.getX(),physical.getY(),physical.getRight(),physical.getBottom());
      CombineRgn(combined,combined,part,RGN_OR);DeleteObject(part);
    }
    if(SetWindowRgn(window,combined,TRUE))controlsShapeSignature=signature;
    else DeleteObject(combined);
  }
#endif
}
void Editor::setUiHidden(bool hidden) {
  if(hidden && panel)closePanel();
  uiHidden=hidden;
  if(controlsWindow) {
    controlsWindow->setVisible(!hidden);
    if(!hidden)controlsWindow->toFront(!quietDesktopChecks);
  }
  updatePresentation();resized();repaint();
}
void Editor::mouseDown(const juce::MouseEvent &event) {
  if(uiHidden && (!isHome || controlsWindow))setUiHidden(false);
  else if(controlsWindow && event.eventComponent==this)controlsWindow->toFront(!quietDesktopChecks);
}
void Editor::updatePresentation() {
  const bool sceneOnly=controlsWindow!=nullptr || uiHidden;
  const bool wantsCanvas=(!isHome || controlsWindow) && (processor.workspace()==Workspace::lab || performanceLive || sceneOnly);
  auto *target=wantsCanvas?(sceneOnly?static_cast<juce::Component *>(&canvas):this):nullptr;
  // Visibility describes the Canvas component, not its OpenGL child window:
  // attachUi targets the whole editor. Track that actual attachment so an
  // invisible Canvas cannot leave a stale GL surface above the native menus.
  if(canvas.presentationTarget()!=target) {
    canvas.detachUi();
    if(wantsCanvas){if(sceneOnly)canvas.attachScene();else canvas.attachUi(*this);}
  }
  canvas.setVisible(wantsCanvas);
}
void Editor::selectWorkspace(Workspace mode,bool homeScreen) {
#if GRID_DISABLE_ADVANCED_WORKSPACES
  if (mode != Workspace::lab) mode = Workspace::lab;
#endif
  if(controlsWindow && (mode==Workspace::performance || processor.workspace()==Workspace::performance))togglePopOut();
  if(panel)closePanel();
#if GRID_USE_VEL_ENGINE
  if(nativeEngine)nativeEngine->coverPanel(false);
#endif
  processor.selectWorkspace(mode);processor.learning.store(false);
  performanceLive=false;midiStrobes=midiBlackout=autoKeyDown=false;
  selectedControl.clear();selectedFixed=-1;isHome=homeScreen;
  updatePresentation();resized();showMessage(juce::String(workspaceName(mode))+" MIDI bank");
}
void Editor::closePanel() {
  if (back.getParentComponent() != &uiHost())
    uiHost().addChildComponent(back);
  settingsScreen = SettingsPage::none;
  panel.reset();
  panelWidgets.clear();
  resized();
}
void Editor::paint(juce::Graphics &g) {if(!controlsWindow)paintUi(g);}
void Editor::paintUi(juce::Graphics &g) {
  if(processor.workspace()!=Workspace::lab && !performanceLive)g.fillAll(bg);
  if (!overlaySnapshot && isHome)
    g.fillAll(bg);
  if (isHome) {
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(42, juce::Font::bold));
    g.drawText("GRID-ARVx", uiHost().getLocalBounds().withHeight(160),
               juce::Justification::centredBottom);
    g.setColour(muted);
    g.setFont(juce::FontOptions(14));
    g.drawText("CENACEAE / AUDIO REACTIVE VISUAL LAB", 0, 170, uiHost().getWidth(), 30,
               juce::Justification::centred);
  } else if (!uiHidden) {
    g.setColour(juce::Colour(0xe6101011));
    g.fillRect(0, 0, uiHost().getWidth(), 82);
    if(processor.workspace()==Workspace::lab || processor.learning.load())
      g.fillRect(0, uiHost().getHeight() - 142, uiHost().getWidth(), 142);
    g.setColour(juce::Colour(0xff333338));
    g.drawHorizontalLine(82, 0, (float)uiHost().getWidth());
    if(processor.workspace()==Workspace::lab || processor.learning.load())
      g.drawHorizontalLine(uiHost().getHeight() - 142, 0, (float)uiHost().getWidth());
  }
}
void Editor::paintOverChildren(juce::Graphics &g) {
  if (dragging && !dragOnControls) {
    g.setColour(juce::Colours::black.withAlpha(.7f));
    g.fillAll();
    g.setColour(accent);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4), 6, 1.5f);
    g.setFont(juce::FontOptions(16, juce::Font::bold));
    g.drawText(decodeChoice(processor.value("category"), 9) == 7
                   ? "DROP IMAGE, FONT, OR TEXT / LYRICS"
                   : "DROP IMAGE TO TRACE",
               getLocalBounds(), juce::Justification::centred);
  }
}
void Editor::resized() {
  canvas.setBounds(getLocalBounds());
  auto r=uiHost().getLocalBounds();
  const int uiWidth=r.getWidth(),uiHeight=r.getHeight();
  status.setVisible(isHome || !uiHidden);
  canvas.setVisible((!isHome || controlsWindow) && (processor.workspace()==Workspace::lab || performanceLive || controlsWindow || uiHidden));
  midi.setButtonText(processor.workspace()==Workspace::performance?"MAPPING":"MIDI");
  workspaceAction.setVisible(!isHome && !uiHidden && processor.workspace()!=Workspace::lab && !performanceLive);
  for (auto *b : {&home, &record, &category, &previous, &next, &save, &midi,
                  &full, &popOut, &lockButton, &resetButton, &preset, &textFx,
                  &lyricCycle, &addLyric, &clearLyrics, &clearImage})
    b->setVisible(!isHome && !uiHidden);
  hide.setVisible(!isHome && !uiHidden);
  popOut.setVisible(!isHome && !uiHidden && (processor.workspace()!=Workspace::performance || performanceLive));
  midiTile.setVisible(!isHome && !uiHidden && processor.workspace()==Workspace::multiple);
  for (auto *b : {&lab, &multiple, &performanceButton, &settings})
    b->setVisible(isHome && !panel);
  back.setVisible(panel && settingsScreen != SettingsPage::none);
  for (auto &c : knobs)
    c->setVisible(false);
  for (auto &e : lyricEditors)
    e->setVisible(false);
  for (auto &e : lyricNumbers)
    e->setVisible(false);
  for (auto &e : lyricRemove)
    e->setVisible(false);
  lyricViewport.setVisible(!isHome && !uiHidden &&
                           processor.workspace()==Workspace::lab &&
                           decodeChoice(processor.value("category"), 9) == 7);
  if (isHome) {
#if GRID_USE_VEL_ENGINE
    if(nativeEngine)nativeEngine->showWorkspace(processor.workspace(),nullptr,{});
#endif
    int width = 300, x = (uiWidth - width) / 2;
    lab.setBounds(x, 230, width, 64);
    multiple.setBounds(x, 312, width, 64);
    performanceButton.setBounds(x, 394, width, 64);
    settings.setBounds(x, 476, width, 64);
    status.setBounds(20, uiHeight - 36, uiWidth - 40, 28);
  } else {
    int middle = uiWidth / 2;
    // Anchor the selectors to the window center, independently of side
    // controls.
    category.setBounds(middle - 128, 14, 256, 54);
    home.setBounds(middle - 370, 22, 88, 38);
    midi.setBounds(middle - 264, 20, 64, 40);
    if(performanceLive) {
      home.setBounds(middle-384,22,88,38);midi.setBounds(middle-284,20,100,40);
    }
    record.setBounds(middle - 178, 20, 36, 38);
    previous.setBounds(middle + 138, 14, 24, 25);
    next.setBounds(middle + 138, 42, 24, 25);
    save.setBounds(middle + 180, 20, 42, 40);
    full.setBounds(middle + 268, 23, 30, 32);
    popOut.setBounds(middle+305,23,30,32);
    hide.setBounds(middle + 342, 23, 30, 32);
    if(processor.workspace()!=Workspace::lab) {
      category.setButtonText(juce::String(workspaceName(processor.workspace())).toUpperCase());
      category.onClick=nullptr;
      category.onClick=nullptr;category.setTooltip("");
      for(auto *button:{&previous,&next,&save,&lockButton,&resetButton,&preset,&textFx,
                       &lyricCycle,&addLyric,&clearLyrics,&clearImage})button->setVisible(false);
      if(processor.workspace()==Workspace::performance)record.setVisible(false);
      if(processor.workspace()==Workspace::performance) {
        home.setBounds(middle-370,22,88,38);midi.setBounds(middle-270,20,110,40);
        workspaceAction.setButtonText("PLAY");workspaceAction.setBounds(uiWidth-96,22,78,38);
        if(!performanceLive)for(auto *button:{&full,&hide})button->setVisible(false);
      } else {
        home.setBounds(middle-444,22,88,38);midi.setBounds(middle-346,20,64,40);
        record.setBounds(middle-272,20,36,38);workspaceAction.setBounds(uiWidth-96,22,78,38);
        save.setVisible(!uiHidden);save.setBounds(middle+138,20,42,40);
        save.setTooltip("Save the complete Multiple preset");
        midiTile.setBounds(middle+188,25,88,32);
        full.setBounds(middle+284,23,26,32);popOut.setBounds(middle+316,23,26,32);hide.setBounds(middle+348,23,26,32);
      }
      midiTile.setSelectedId(decodeChoice(processor.value("multiTile"),36)+1,juce::dontSendNotification);
      const int footer=processor.learning.load() && !uiHidden?142:0;
      bool multipleLayout=false;
#if GRID_USE_VEL_ENGINE
      multipleLayout=processor.workspace()==Workspace::multiple && nativeEngine && nativeEngine->multipleLayoutVisible();
#endif
      if(multipleLayout && !uiHidden) {
        preset.setVisible(true);preset.setButtonText("PRESET");preset.setTooltip("Choose a saved Multiple layout and its tile visuals");
        preset.setBounds(middle-100,96,200,32);
        previous.setVisible(true);next.setVisible(true);
        previous.setBounds(middle+105,94,22,17);next.setBounds(middle+105,112,22,17);
      }
      const int top=uiHidden?0:multipleLayout?142:82;
#if GRID_USE_VEL_ENGINE
      // The native editor owns a child HWND. Keep it offscreen while a GRID
      // dialog is shown, so it cannot cover the dialog or intercept its clicks.
      if(nativeEngine) {
        nativeEngine->coverPanel(panel!=nullptr && !performanceLive && !uiHidden);
        nativeEngine->showWorkspace(processor.workspace(),uiHidden || performanceLive?nullptr:&uiHost(),
          {0,top,uiWidth,uiHeight-top-footer},true);
        if(processor.workspace()==Workspace::multiple) {
          workspaceAction.setButtonText(nativeEngine->multipleLayoutVisible()?"DONE":"LAYOUT");
          if(nativeEngine->multipleLayoutVisible()!=multipleLayout) {resized();return;}
        }
      }
#endif
      if(processor.learning.load() && !uiHidden) {
        const char *ids[]{"in","int","speed","glow","texture","dither","track","glass","color","textureReact","ditherReact","trackReact","glassReact",
                          "perfPrevious","perfNext","perfStrobes","perfBlackout","perfAuto","perfBypass"};
        const int count=processor.workspace()==Workspace::performance?19:13;
        int position=0;
        for(const auto *id:ids) {
          if(processor.workspace()==Workspace::performance && juce::String(id)=="multiTile")continue;
          if(processor.workspace()==Workspace::multiple && juce::String(id).startsWith("perf"))continue;
          for(auto &control:knobs)if(control->id==id) {
            if(control->id.endsWith("React"))control->label=control->id.dropLastCharacters(5).toUpperCase()+" AR";
            const int row=position/10,column=position%10;
            const int rowCount=juce::jmin(10,count-row*10),width=juce::jmin(102,uiWidth/10);
            control->setBounds((uiWidth-rowCount*width)/2+column*width,uiHeight-footer+row*64,width,60);
            control->setVisible(true);++position;
          }
        }
      }
      status.setBounds(12,uiHeight-24,uiWidth-24,20);
      if(panel) {panel->setBounds(r.withSizeKeepingCentre(juce::jmin(760,uiWidth-40),juce::jmin(560,uiHeight-100)));panel->toFront(false);}
      syncControlsWindow();return;
    }
#if GRID_USE_VEL_ENGINE
    if(nativeEngine)nativeEngine->showWorkspace(Workspace::lab,nullptr,{});
#endif
    category.onClick=[this]{categoryMenu();};category.setTooltip("");save.setTooltip("Save this Lab preset");
    for(auto &control:knobs)if(control->id.endsWith("React"))control->label="o";
    const int pw = juce::jlimit(130, 220, uiWidth - 800);
    int y = uiHeight - 126,
        cat = decodeChoice(processor.value("category"), 9),
        x = middle - pw / 2 - 360;
    auto place = [&](const char *id, int xx, int yy, int ww = 65, int hh = 84) {
      for (auto &c : knobs)
        if (c->id == id) {
          c->setBounds(xx, yy, ww, hh);
          c->setVisible(!uiHidden);
        }
    };
    place("in", x, y, 50);
    x += 52;
    for (auto fx : fxIDs) {
      place(fx, x, y);
      place((juce::String(fx) + "React").toRawUTF8(), x, y + 75, 65, 37);
      x += 70;
    }
    lockButton.setBounds(x, y + 4, 26, 30);
    preset.setBounds(x + 28, y + 7, pw, 49);
    place("presetCycle", middle - 48, y + 55, 96, cat==7?36:42);
    resetButton.setBounds(x + pw + 28, y + 4, 26, 30);
    int right = x + pw + 62;
    place("int", right, y);
    right += 69;
    bool speed = cat == 1 || cat == 2 || cat == 3 || cat == 4 || cat == 5 ||
                 cat == 6 || cat == 7;
    bool glow = cat == 0 || cat == 3 || cat == 7 || cat == 8;
    if (speed) {
      place("speed", right, y);
      right += 69;
    }
    if (glow) {
      place("glow", right, y);
      if (cat == 8)
        place("glowReact", right, y + 75, 65, 37);
      right += 69;
    }
    place("color", right, y);
    place("colorCycle", right, y + 78, 65, 36);
    right += 69;
    if (cat == 4 || cat == 8) {
      place("color2", right, y);
      place("color2Cycle", right, y + 78, 65, 36);
      right += 69;
    }
    textFx.setVisible(cat == 7 && !uiHidden);
    if (cat == 7) {
      textFx.setBounds(x + 28, y + 91, pw, 34);
      place("textColor", right, y);
      place("textColorCycle", right, y + 78, 65, 36);
      if (assets.image.isValid()) {
        place("imageScale", uiWidth - 90, 90);
        clearImage.setBounds(uiWidth - 30, 88, 23, 23);
      } else
        clearImage.setVisible(false);
      int rows = juce::jmin(4, (int)lyricEditors.size()),
          top = uiHeight - 150 - rows * 31 - 32;
      lyricCycle.setBounds(8, top, 24, 24);
      addLyric.setBounds(uiWidth - 29, top, 23, 24);
      clearLyrics.setBounds(uiWidth - 57, top, 23, 24);
      for (int i = 0; i < (int)lyricEditors.size(); ++i) {
        bool show = !uiHidden;
        lyricEditors[(size_t)i]->setVisible(show);
        lyricNumbers[(size_t)i]->setVisible(show);
        lyricRemove[(size_t)i]->setVisible(show);
        lyricNumbers[(size_t)i]->setBounds(0, i * 31, 24, 26);
        lyricEditors[(size_t)i]->setBounds(30, i * 31, uiWidth - 90, 26);
        lyricRemove[(size_t)i]->setBounds(uiWidth - 55, i * 31, 23, 26);
      }
      lyricViewport.setBounds(8, top + 31, uiWidth - 16, rows * 31);
      lyricBody.setSize(uiWidth - 32, (int)lyricEditors.size() * 31);
    } else {
      lyricCycle.setVisible(false);
      addLyric.setVisible(false);
      clearLyrics.setVisible(false);
      clearImage.setVisible(false);
    }
    status.setBounds(12, uiHeight - 26, uiWidth - 24, 22);
  }
  if (panel) {
    panel->setBounds(r.withSizeKeepingCentre(
        juce::jmin(680, uiWidth - 40), juce::jmin(560, uiHeight - 80)));
    panel->toFront(false);
    if (settingsScreen != SettingsPage::none) {
      back.setBounds(16, 15, 76, 30);
      back.toFront(false);
    }
  }
  if(controlsSurface){syncControlsWindow();controlsSurface->repaint();}
  repaint();
}
bool Editor::keyPressed(const juce::KeyPress &k) {
  if(panel && processor.workspace()==Workspace::performance)return false;
  if(k==juce::KeyPress::escapeKey && uiHidden){setUiHidden(false);return true;}
  if(k==juce::KeyPress::escapeKey && !panel && windowMode!=0){setWindowMode(0);return true;}
#if GRID_USE_VEL_ENGINE
  if(processor.workspace()!=Workspace::lab && nativeEngine)return nativeEngine->forwardKey(k);
#endif
  if (k.getModifiers().isCtrlDown() && (k.getKeyCode()=='o' || k.getKeyCode()=='O')) {
    if(panel) return false;
    chooser=std::make_unique<juce::FileChooser>("Import image, font, or lyrics",juce::File(),
        "*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.ttf;*.otf;*.txt");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,
      [safe=juce::Component::SafePointer<Editor>(this)](const juce::FileChooser &file) {
        if(safe && file.getResult().existsAsFile())
          safe->filesDropped({file.getResult().getFullPathName()},0,0);
      });
    return true;
  }
  if (k == juce::KeyPress::escapeKey) {
    if (panel)
      navigateBack();
    else if (uiHidden) {
      setUiHidden(false);
    } else if(windowMode!=0)setWindowMode(0);
    return true;
  }
  return false;
}
void Editor::toggleFullScreen() {setWindowMode((windowMode+1)%3);}
void Editor::setWindowMode(int mode) {
  mode=juce::jlimit(0,2,mode);
  auto *window=dynamic_cast<juce::DocumentWindow *>(getTopLevelComponent());
  if(!window) {
    if(mode==0) {outputWindow.reset();outputCanvas=nullptr;}
    else if(!outputWindow) {
      class Output final : public juce::DocumentWindow {
      public:
        explicit Output(std::function<void()> close)
            :DocumentWindow("GRID-ARVx Output",juce::Colours::black,closeButton),closed(std::move(close)) {
          setUsingNativeTitleBar(true);setContentOwned(new Canvas,true);
          centreWithSize(1120,740);setVisible(true);
        }
        juce::BorderSize<int> getBorderThickness() const override {
          return isUsingNativeTitleBar()?DocumentWindow::getBorderThickness():juce::BorderSize<int>{};
        }
        void closeButtonPressed() override {if(closed)closed();}
        bool keyPressed(const juce::KeyPress &key) override {
          if(key==juce::KeyPress::escapeKey){closeButtonPressed();return true;}return false;
        }
      private:std::function<void()> closed;
      };
      juce::Component::SafePointer<Editor> safe(this);
      outputWindow=std::make_unique<Output>([safe]{juce::MessageManager::callAsync([safe]{if(safe)safe->setWindowMode(0);});});
      outputCanvas=dynamic_cast<Canvas *>(outputWindow->getContentComponent());
    }
    window=outputWindow.get();
  }
  if(window) {
    if(windowMode==0 && mode!=0) {
      windowedBounds=window->getBounds();windowedNativeTitle=window->isUsingNativeTitleBar();
      windowedTitleHeight=window->getTitleBarHeight();windowedResizable=window->isResizable();
    }
#if JUCE_WINDOWS
    // Title-bar changes can recreate the main HWND. Release and restore the
    // overlay's ownership so Windows does not destroy its owned control peer.
    if(controlsWindow && captureOverlay && controlsWindow->getPeer())
      SetWindowLongPtrW(static_cast<HWND>(controlsWindow->getPeer()->getNativeHandle()),GWLP_HWNDPARENT,0);
#endif
    window->setFullScreen(false);
    if(mode==0) {
      window->setUsingNativeTitleBar(windowedNativeTitle);window->setTitleBarHeight(windowedTitleHeight);
      window->setResizable(windowedResizable,false);if(!windowedBounds.isEmpty())window->setBounds(windowedBounds);
    } else {
      window->setUsingNativeTitleBar(false);window->setTitleBarHeight(0);window->setResizable(false,false);
      if(mode==1 && !windowedBounds.isEmpty())window->setBounds(windowedBounds);
      if(mode==2)window->setFullScreen(true);
    }
#if JUCE_WINDOWS
    if(controlsWindow && captureOverlay && controlsWindow->getPeer() && getPeer())
      SetWindowLongPtrW(static_cast<HWND>(controlsWindow->getPeer()->getNativeHandle()),GWLP_HWNDPARENT,
          reinterpret_cast<LONG_PTR>(getPeer()->getNativeHandle()));
#endif
  }
  windowMode=mode;
  full.iconColour=mode==1?accent:mode==2?juce::Colour(0xffff4d5a):muted;
  full.setToggleState(mode!=0,juce::dontSendNotification);full.repaint();syncControlsWindow();resized();
}
bool Editor::learn(const juce::String &id, int fixed) {
  if (!processor.learning)
    return false;
  MidiEvent pending;
  while (processor.midiEvents.pop(pending)) {
  }
  selectedControl = id;
  selectedFixed = fixed;
  showMessage("Learning " + id +
              juce::String(u8" — send MIDI CC, then confirm Knob or Button."));
  for (auto &c : knobs)
    c->selected = c->id == id;
  return true;
}
void Editor::effectMenu(int fx) {
  juce::PopupMenu m;
  auto names = profiles(fx);
  auto id = juce::String(fxIDs[fx]) + "Type";
  for (int i = 0; i < names.size(); ++i)
    m.addItem(i + 1, names[i], true,
              decodeChoice(processor.value(id.toRawUTF8()), names.size()) == i);
  juce::Component::SafePointer<Editor> safe(this);
  m.showMenuAsync(juce::PopupMenu::Options(), [safe, fx, id](int i) {
    if (safe && i > 0) {
      int v = encodeChoice(i - 1, profiles(fx).size());
      if (!safe->learn(id, v))
        safe->processor.set(id.toRawUTF8(), v);
    }
  });
}
void Editor::intervalMenu(const juce::String &toggle, bool seconds) {
  auto id = toggle.dropLastCharacters(5) + "Interval";
  juce::PopupMenu m;
  for (int n : {1, 2, 4, 8, 16, 32, 64, 128})
    m.addItem(n, juce::String(n) + (seconds ? " seconds" : " beats"));
  m.addItem(1001, "Custom interval / MIDI binding...");
  juce::Component::SafePointer<Editor> safe(this);
  m.showMenuAsync(juce::PopupMenu::Options(), [safe, id](int n) {
    if (safe && n == 1001)
      safe->intervalDialog(id);
    else if (safe && n > 0)
      safe->processor.set(id.toRawUTF8(), n - 1);
  });
}
void Editor::categoryMenu() {
  class UserRow final : public juce::PopupMenu::CustomComponent {
  public:
    UserRow(juce::String n, int i, std::shared_ptr<int> d)
        : CustomComponent(false), name(std::move(n)), id(i),
          deletion(std::move(d)) {}
    void getIdealSize(int &w, int &h) override {
      w = 280;
      h = 40;
    }
    void paint(juce::Graphics &g) override {
      g.fillAll(isItemHighlighted() ? juce::Colour(0xff363639)
                                    : juce::Colour(0xff161618));
      g.setColour(juce::Colours::white);
      g.setFont(juce::FontOptions(13, juce::Font::bold));
      g.drawText(name.toUpperCase(), 10, 0, getWidth() - 46, getHeight(),
                 juce::Justification::centredLeft);
      g.setColour(muted);
      g.drawText(u8"×", getWidth() - 34, 0, 30, getHeight(),
                 juce::Justification::centred);
    }
    void mouseUp(const juce::MouseEvent &e) override {
      *deletion = e.x > getWidth() - 38 ? id : 0;
      triggerMenuItem();
    }
    juce::String name;
    int id;
    std::shared_ptr<int> deletion;
  };
  juce::PopupMenu main;
  auto mapping = std::make_shared<std::vector<juce::File>>();
  auto deletion = std::make_shared<int>(0);
  for (int c = 0; c < 9; ++c) {
    juce::PopupMenu groups;
    for (auto group : {"Factory", "User", "Defaults"}) {
      auto files = presets.files(c, group);
      if (juce::String(group) == "User" && files.isEmpty())
        continue;
      juce::PopupMenu list;
      for (auto f : files) {
        mapping->push_back(f);
        int id = (int)mapping->size();
        if (juce::String(group) == "User") {
          list.addCustomItem(
              id, std::make_unique<UserRow>(f.getFileNameWithoutExtension(), id,
                                            deletion));
        } else
          list.addItem(id, f.getFileNameWithoutExtension());
      }
      if (files.isEmpty())
        list.addItem(9999, "No presets yet", false);
      groups.addSubMenu(group, list);
    }
    main.addSubMenu(categories[c], groups);
  }
  juce::Component::SafePointer<Editor> safe(this);
  main.showMenuAsync(
      juce::PopupMenu::Options().withTargetComponent(&category),
      [safe, mapping, deletion](int id) {
        if (!safe || id <= 0)
          return;
        bool remove = *deletion == id;
        if (id > (int)mapping->size())
          return;
        auto file = (*mapping)[(size_t)id - 1];
        if (remove) {
          juce::AlertWindow::showOkCancelBox(
              juce::MessageBoxIconType::QuestionIcon, "Delete preset",
              "Delete " + file.getFileNameWithoutExtension() + "?", "Delete",
              "Cancel", safe.getComponent(),
              juce::ModalCallbackFunction::create([safe, file](int yes) {
                if (safe && yes)
                  file.deleteFile();
              }));
        } else {
          auto result = safe->presets.load(file);
          if (result.failed())
            safe->showMessage(result.getErrorMessage());
          safe->refreshAssets();
          safe->sync();
          safe->resized();
        }
      });
}
void Editor::nextPreset(int direction, bool automatic) {
#if GRID_USE_VEL_ENGINE
  if(processor.workspace()==Workspace::multiple && nativeEngine) {nativeEngine->stepMultiplePreset(direction);resized();return;}
#endif
  int cat = decodeChoice(processor.value("category"), 9);
  auto list = presets.all(cat);
  if (list.isEmpty())
    return;
  auto name = processor.getAssetProperty("presetName");
  auto presetFile = processor.getAssetProperty("presetFile");
  if (name.isEmpty()) {
    auto names = templates(cat);
    name = names[juce::jlimit(0, names.size() - 1,
                              decodeChoice(processor.value("variant"), 16))];
  }
  int index = -1;
  for (int i = 0; i < list.size(); ++i)
    if (presetFile.isNotEmpty()
            ? list[i].getFullPathName() == presetFile
            : list[i].getFileNameWithoutExtension() == name) {
      index = i;
      break;
    }
  int target = (index + direction + list.size()) % list.size();
  if (index < 0 && direction < 0)
    target = list.size() - 1;
  const char *cycleIds[]{"colorCycle","color2Cycle","textColorCycle","lyricCycle",
    "colorInterval","color2Interval","textColorInterval","lyricInterval","lyricTimeMode"};
  std::array<int,9> cycleValues{};
  if(automatic)for(size_t i=0;i<cycleValues.size();++i)cycleValues[i]=processor.value(cycleIds[i]);
  auto result = presets.load(list[target]);
  if(automatic)for(size_t i=0;i<cycleValues.size();++i)processor.set(cycleIds[i],cycleValues[i]);
  if (result.failed())
    showMessage(result.getErrorMessage());
  refreshAssets();
  sync();
}
void Editor::reset() {
  for (auto c : controls) {
    juce::String id(c.id);
    if (id == "in" || id == "int" || id == "glow" || id.startsWith("texture") ||
        id.startsWith("dither") || id.startsWith("track") ||
        id.startsWith("glass") || id == "color" || id == "color2" ||
        id == "textColor" || id == "colorCycle" || id == "color2Cycle" ||
        id == "textColorCycle" || id == "glowReact")
      processor.set(c.id, c.initial);
  }
  const auto cat = decodeChoice(processor.value("category"), 9);
  processor.set(
      "color",
      defaultTemplateColor(cat, decodeChoice(processor.value("variant"), 16)));
  processor.set("color2", defaultColor2(cat));
  processor.set(
      "textColor",
      defaultTextColor(cat, decodeChoice(processor.value("variant"), 16)));
  sync();
}
void Editor::sync() {
  int c = decodeChoice(processor.value("category"), 9);
  category.setButtonText(categories[c].toUpperCase());
  auto name = processor.getAssetProperty("presetName");
  auto names = templates(c);
  preset.setButtonText(
      name.isEmpty()
          ? names[juce::jlimit(0, names.size() - 1,
                               decodeChoice(processor.value("variant"), 16))]
                .toUpperCase()
          : name.toUpperCase());
  textFx.setButtonText(
      textEffects[decodeChoice(processor.value("textEffect"), 7)]
          .toUpperCase());
  midi.setToggleState(processor.learning, juce::dontSendNotification);
  lockButton.setToggleState(processor.value("lock") >= 64,
                            juce::dontSendNotification);
  record.setToggleState(recorder.active(), juce::dontSendNotification);
  lyricCycle.setToggleState(processor.value("lyricCycle") >= 64,
                            juce::dontSendNotification);
  if (lastCategory != c) {
    lastCategory = c;
    resized();
  }
  for (auto &k : knobs) {
    if (k->id == "color")
      k->label = c == 7 ? "BG" : "COLOR";
    k->refresh();
  }
}
void Editor::setLyrics() {
  if (!lyricsDirty)
    return;
  juce::Array<juce::var> lines;
  assets.lyrics.clear();
  for (auto &e : lyricEditors) {
    auto value = e->getText().substring(0, 120);
    assets.lyrics.add(value);
    lines.add(value);
  }
  processor.setAssetProperty("lyrics", juce::JSON::toString(lines));
  loadedLyrics = processor.getAssetProperty("lyrics");
  lyricsDirty = false;
}
void Editor::rebuildLyrics() {
  lyricEditors.clear();
  lyricNumbers.clear();
  lyricRemove.clear();
  loadedLyrics = processor.getAssetProperty("lyrics");
  assets.lyrics.clear();
  auto a = juce::JSON::parse(loadedLyrics);
  if (auto *lines = a.getArray())
    for (auto l : *lines)
      assets.lyrics.add(l.toString());
  if (assets.lyrics.isEmpty())
    assets.lyrics.add("");
  for (int i = 0; i < assets.lyrics.size(); ++i) {
    auto edit = std::make_unique<juce::TextEditor>();
    edit->setInputRestrictions(120);
    edit->setText(assets.lyrics[i], false);
    edit->setColour(juce::TextEditor::backgroundColourId,
                    juce::Colour(0x40101010));
    edit->setColour(juce::TextEditor::outlineColourId, muted);
    edit->onTextChange = [this] { lyricsDirty = true; };
    lyricBody.addAndMakeVisible(*edit);
    lyricEditors.push_back(std::move(edit));
    auto n = std::make_unique<juce::TextButton>(juce::String(i + 1));
    n->setColour(juce::TextButton::buttonOnColourId, accent);
    n->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    n->onClick = [this, i] {
      auto value = encodeChoice(i, assets.lyrics.size());
      if (!learn("lyric", value))
        processor.set("lyric", value);
    };
    lyricBody.addAndMakeVisible(*n);
    lyricNumbers.push_back(std::move(n));
    auto rem = std::make_unique<juce::TextButton>(u8"−");
    rem->onClick = [this, i] {
      setLyrics();
      int selected =
          decodeChoice(processor.value("lyric"), assets.lyrics.size());
      assets.lyrics.remove(i);
      if (assets.lyrics.isEmpty())
        assets.lyrics.add("");
      processor.setAssetProperty(
          "lyrics", juce::JSON::toString(juce::var(assets.lyrics)));
      if (i < selected)
        --selected;
      processor.set("lyric",
                    encodeChoice(juce::jmin(selected, assets.lyrics.size() - 1),
                                 assets.lyrics.size()));
      rebuildLyrics();
    };
    lyricBody.addAndMakeVisible(*rem);
    lyricRemove.push_back(std::move(rem));
  }
  resized();
}
void Editor::refreshAssets() {
  const auto styleRoot = processor.getAssetProperty("styleFontRoot");
  if (styleRoot != loadedStyleFonts) {
    loadedStyleFonts = styleRoot;
    assets.styleFonts.fill(nullptr);
    const char *files[] = {"GenericG50.otf",
                           "Noskale.otf",
                           "ShadowHand.otf",
                           "VCRMono.ttf",
                           "DMMono-Regular.ttf",
                           "Tarophis.otf",
                           "PressStart2P-Regular.ttf",
                           "Ransom.ttf",
                           "Noskale.otf",
                           "PicNic.otf",
                           "FlorDeRuina.otf",
                           "Myrtille.ttf"};
    if (styleRoot.isNotEmpty() && juce::File(styleRoot).isDirectory())
      for (int style = 0; style < 12; ++style) {
        juce::MemoryBlock data;
        auto file = juce::File(styleRoot).getChildFile(files[style]);
        if (file.existsAsFile() && file.getSize() <= 32 * 1024 * 1024 &&
            file.loadFileAsData(data))
          assets.styleFonts[(size_t)style] =
              juce::Typeface::createSystemTypefaceFor(data.getData(),
                                                      data.getSize());
      }
  }
  auto imagePath = processor.getAssetProperty("imagePath"),
       fontPath = processor.getAssetProperty("fontPath");
  const bool imageChanged = loadedImage != imagePath;
  if (loadedImage != imagePath) {
    loadedImage = imagePath;
    auto file = juce::File(imagePath);
    assets.image = imagePath.isNotEmpty() && file.getSize() <= 32 * 1024 * 1024
                       ? juce::ImageFileFormat::loadFrom(file)
                       : juce::Image();
    if (assets.image.getWidth() > 8192 || assets.image.getHeight() > 8192)
      assets.image = {};
    assets.contour.clear();
    imageTraceReady = false;
    if (imagePath.isNotEmpty() && !assets.image.isValid())
      showMessage("Image missing or unsupported: " + imagePath);
  }
  const auto cat = decodeChoice(processor.value("category"), 9);
  if (!imageTraceReady && assets.image.isValid() && cat != 7) {
    assets.contour = traceImage(assets.image);
    imageTraceReady = true;
    showMessage(assets.contour.isEmpty() ? "No outline found. Use an image with contrasting shapes or transparency." : "Image traced: " + juce::File(loadedImage).getFileName());
  }
  if (loadedFont != fontPath) {
    loadedFont = fontPath;
    juce::MemoryBlock data;
    auto file = juce::File(fontPath);
    if (fontPath.isNotEmpty() && file.getSize() <= 32 * 1024 * 1024 &&
        file.loadFileAsData(data))
      assets.font = juce::Typeface::createSystemTypefaceFor(data.getData(),
                                                            data.getSize());
    else
      assets.font = nullptr;
  }
  if (loadedLyrics != processor.getAssetProperty("lyrics"))
    rebuildLyrics();
  if (imageChanged)
    resized();
}
bool Editor::isInterestedInFileDrag(const juce::StringArray &f) {
  for (auto name : f)
    if (juce::File(name).hasFileExtension("png;jpg;jpeg;bmp;gif;ttf;otf;txt"))
      return true;
  return false;
}
void Editor::fileDragEnter(const juce::StringArray &, int, int) {
  dragging = true;
  repaint();
}
void Editor::fileDragMove(const juce::StringArray &files, int, int) {
  if (!dragging && isInterestedInFileDrag(files)) { dragging = true; repaint(); }
}
void Editor::fileDragExit(const juce::StringArray &) {
  dragging = false;
  repaint();
}
void Editor::filesDropped(const juce::StringArray &files, int, int) {
  dragging = false;
  if (isHome && !files.isEmpty()) {
    const bool textFile = juce::File(files[0]).hasFileExtension("ttf;otf;txt");
    processor.set("category", encodeChoice(textFile?7:0,9));
    processor.set("variant",0);
    processor.setAssetProperty("presetName","");
    processor.setAssetProperty("presetFile","");
    isHome = false;
    closePanel();
  }
  for (auto f : files)
    importAsset(juce::File(f));
  updatePresentation();
  resized();
  repaint();
}
void Editor::importAsset(const juce::File &f) {
  if (!f.existsAsFile() || f.getSize() > 32 * 1024 * 1024) {
    showMessage("Import files must be smaller than 32 MB.");
    return;
  }
#if GRID_USE_VEL_ENGINE
  if(processor.workspace()!=Workspace::lab && nativeEngine) {
    if(f.hasFileExtension("txt")) {
      showMessage("Import lyric text in Lab. Native panels keep their own text editors.");
      return;
    }
    const auto result=nativeEngine->importFile(f);
    showMessage(result.failed()?result.getErrorMessage():"File imported into the active native panel.");
    return;
  }
#endif
  int cat = decodeChoice(processor.value("category"), 9);
  if (f.hasFileExtension("txt")) {
    if (cat != 7) {
      showMessage("Drop lyrics in Text.");
      return;
    }
    auto lines = juce::StringArray::fromLines(f.loadFileAsString());
    juce::Array<juce::var> values;
    for (int i = 0; i < lines.size(); ++i) {
      auto line = lines[i].trim();
      if (line.length() > 120) {
        showMessage("Import rejected: line " + juce::String(i + 1) +
                    " exceeds 120 characters.");
        return;
      }
      if (line.isNotEmpty())
        values.add(line);
    }
#if GRID_USE_VEL_ENGINE
    if (values.size() > 75 || values.isEmpty()) {
      showMessage("The native engine supports 1–75 non-empty lyric lines.");
#else
    if (values.size() > 128 || values.isEmpty()) {
      showMessage(u8"Lyrics must contain 1–128 non-empty lines.");
#endif
      return;
    }
    processor.setAssetProperty("lyrics", juce::JSON::toString(values));
    processor.set("lyric", 0);
    rebuildLyrics();
  } else if (f.hasFileExtension("ttf;otf")) {
    if (cat != 7) {
      showMessage("Drop fonts in Text.");
      return;
    }
    juce::MemoryBlock b;
    juce::Typeface::Ptr face;
    if (!f.loadFileAsData(b) ||
        !(face = juce::Typeface::createSystemTypefaceFor(b.getData(),
                                                         b.getSize()))) {
      showMessage("Unsupported or malformed font.");
      return;
    }
    processor.setAssetProperty("fontPath", f.getFullPathName());
    loadedFont = f.getFullPathName();
    assets.font = std::move(face);
  } else {
    auto im = juce::ImageFileFormat::loadFrom(f);
    if (!im.isValid() || im.getWidth() > 8192 || im.getHeight() > 8192) {
      showMessage("Unsupported image or dimensions exceed 8192 pixels.");
      return;
    }
    processor.setAssetProperty("imagePath", f.getFullPathName());
    loadedImage = f.getFullPathName();
    assets.image = std::move(im);
    assets.contour.clear();
    imageTraceReady = false;
  }
  // The timer forwards imports after synchronising the native category.
  // Forwarding here as well would import into the previous category and repeat
  // contour extraction on the following tick.
  refreshAssets();
}
void Editor::advanceCycles(unsigned beats,double clockDelta) {
  const auto presetDue =
      presetClock.tick(processor.value("presetCycle") >= 64,
                       processor.value("presetInterval"), beats);
  if (presetDue)
    nextPreset((int)presetDue,true);
  int clock = 0;
  for (auto c : {"color", "color2", "textColor"}) {
    auto id = juce::String(c);
    const auto due = colorClocks[(size_t)clock++].tick(
        processor.value((id + "Cycle").toRawUTF8()) >= 64,
        processor.value((id + "Interval").toRawUTF8()), beats);
    if (due)
      processor.set(c, (processor.value(c) + (int)due * 11) % 126);
  }
  const auto lyricDue =
      lyricClock.tick(processor.value("lyricCycle") >= 64 &&
                          processor.value("lyricTimeMode") < 64,
                      processor.value("lyricInterval"), beats);
  if (lyricDue)
    processor.set("lyric", encodeChoice((decodeChoice(processor.value("lyric"),
                                                      assets.lyrics.size()) +
                                         (int)lyricDue) %
                                            assets.lyrics.size(),
                                        assets.lyrics.size()));
  if (processor.value("lyricCycle") >= 64 &&
      processor.value("lyricTimeMode") >= 64) {
    const auto interval = processor.value("lyricInterval");
    if (!lyricSecondsActive || interval != lastLyricInterval)
      lyricSeconds = 0;
    lyricSecondsActive = true;
    lastLyricInterval = interval;
    lyricSeconds += clockDelta;
    if (lyricSeconds >= interval + 1) {
      const int due = (int)std::fmod(std::floor(lyricSeconds / (interval + 1)),
                                     (double)assets.lyrics.size());
      lyricSeconds = std::fmod(lyricSeconds, (double)interval + 1);
      processor.set("lyric",
                    encodeChoice((decodeChoice(processor.value("lyric"),
                                               assets.lyrics.size()) +
                                  due) %
                                     assets.lyrics.size(),
                                 assets.lyrics.size()));
    }
  } else {
    lyricSeconds = 0;
    lyricSecondsActive = false;
  }
}
void Editor::timerCallback() {
#if GRID_USE_VEL_ENGINE
  if(nativeEngine) {
    const bool playing=!isHome && nativeEngine->performancePlaying();
    if(performanceLive!=playing) {
      performanceLive=playing;
      if(!playing) {
        midiStrobes=midiBlackout=autoKeyDown=false;
        if(processor.workspace()==Workspace::performance && controlsWindow)togglePopOut();
      }
      updatePresentation();resized();
    }
    updatePerformanceKeys();
  }
#endif
  syncControlsWindow();
  double now = juce::Time::getMillisecondCounterHiRes() * .001,
         clockDelta = juce::jmax(0.0, now - lastTick),
         dt = juce::jlimit(.001, .25, clockDelta);
  lastTick = now;
  time += dt;
  const char *actionIds[] = {"nextPreset",  "previousPreset", "resetFx",
                             "savePreset",  "recordVideo",    "hideUI",
                             "fullscreen",  "home",           "addLyric",
                             "clearLyrics", "clearImage", "popOutUI"};
  ToolButton *actionButtons[] = {&next,     &previous,    &resetButton, &save,
                                 &record,   &hide,        &full,        &home,
                                 &addLyric, &clearLyrics, &clearImage, &popOut};
  for (int i = 0; i < 12; ++i) {
    auto v = processor.value(actionIds[i]);
    if (v != actionValues[(size_t)i]) {
      actionValues[(size_t)i] = v;
      if (!processor.learning)
        actionButtons[i]->triggerClick();
    }
  }
  int requestedFps = frameRate(processor.value("frameCap"));
  if (requestedFps != fps) {
    fps = requestedFps;
    startTimerHz(fps);
  }
  quality = decodeChoice(processor.value("renderDetail"), 7);
  canvas.setPaused(isHome && !controlsWindow && processor.value("pauseMenuRendering")>=64);
  // Resolution is explicit and fixed. Legacy adaptive/relative-scale state
  // never silently changes the user's selected rendering resolution.
#if GRID_USE_VEL_ENGINE
  if(nativeEngine) {
    const auto size=renderResolution(processor.value("renderResolution"));
    nativeEngine->updateGraphics(size.width,size.height,processor.value("frameCap"),processor.value("antialias"),processor.value("vsync"));
    nativeEngine->setPaused(isHome && !controlsWindow && processor.value("pauseMenuRendering")>=64);
    nativeEngine->setFrameCapture(recorder.active() || controlsWindow || uiHidden);
  }
#endif
  setLyrics();
  const int assetCategory = decodeChoice(processor.value("category"), 9);
  if (loadedLyrics != processor.getAssetProperty("lyrics") ||
      loadedStyleFonts != processor.getAssetProperty("styleFontRoot") ||
      loadedImage != processor.getAssetProperty("imagePath") ||
      loadedFont != processor.getAssetProperty("fontPath") ||
      (!imageTraceReady && assets.image.isValid() && assetCategory != 7))
    refreshAssets();
  MidiEvent event;
  if (!bindingPrompt && selectedControl.isNotEmpty() && processor.midiEvents.pop(event)) {
    bindingPrompt = true;
    auto id = selectedControl;
    int fixed = selectedFixed;
    const auto bindingWorkspace=processor.workspace();
    juce::Component::SafePointer<Editor> safe(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Confirm MIDI hardware")
            .withMessage("Channel " + juce::String(event.channel) +
                         (event.note ? ", Note " : ", CC ") +
                         juce::String(event.cc) + juce::String(u8" → ") + id +
                         "\nChoose the physical control type.")
            .withButton("Knob")
            .withButton("Button")
            .withButton("Cancel"),
        [safe, id, fixed, event,bindingWorkspace](int result) {
          if (!safe)
            return;
          safe->bindingPrompt = false;
          if(safe->processor.workspace()!=bindingWorkspace)return;
          if (result == 0)
            return;
          Binding b{indexOf(id.toStdString()), fixed,
                    result == 1 ? Hardware::knob : Hardware::button};
          if (!safe->processor.bind(event.channel, event.cc, b, event.note)) {
            safe->showMessage("Rejected: knobs bind only to knobs; buttons "
                              "bind to toggles or profile selections.");
            return;
          }
          safe->selectedControl.clear();
          safe->showMessage("Bound CC " + juce::String(event.cc) + " to " + id);
        });
  }
  MidiEvent applied;
  while(processor.appliedMidi.pop(applied))
    if(applied.cc>=0 && applied.cc<(int)controls.size())
      showMessage("MIDI channel "+juce::String(applied.channel)+" changed "+
                  juce::String(controls[(size_t)applied.cc].name)+" to "+juce::String(applied.value));
  auto audio = processor.analysis.consume((float)dt);
#if GRID_USE_VEL_ENGINE
  MidiEvent mapped;
  while(processor.mappedEvents.pop(mapped))
    if(nativeEngine && mapped.channel==(int)processor.workspace()+1 && mapped.cc>=0 && mapped.cc<(int)controls.size())
      nativeEngine->applyMappedControl(controls[(size_t)mapped.cc].id,mapped.value,mapped.note);
  if(!isHome && processor.workspace()!=Workspace::lab && nativeEngine) {
    SceneState display;
    for(size_t i=0;i<controls.size();++i)display.values[i]=processor.value(controls[i].id);
    const auto resolution=renderResolution(processor.value("renderResolution"));
    display.width=resolution.width;display.height=resolution.height;
    nativeEngine->update(display);
    if(controlsWindow || uiHidden || performanceLive) {auto image=nativeEngine->latestFrame();if(image.isValid())canvas.presentNative(image,display);}
    if(recorder.active()) {
      auto frame=nativeEngine->latestFrame();
      if(frame.isValid()) {
        juce::Image bottomUp(juce::Image::ARGB,frame.getWidth(),frame.getHeight(),false,juce::SoftwareImageType());
        juce::Graphics graphics(bottomUp);
        graphics.drawImageTransformed(frame,juce::AffineTransform::verticalFlip((float)frame.getHeight()));
        recorder.submit(bottomUp);
      }
    }
    if(recorder.active())showMessage("Recording "+juce::String(recorder.elapsed(),1)+"s / dropped frames "+juce::String(recorder.dropped.load()));
    else if(processor.value("stats")>=64) {
      const double ms=nativeEngine->frameMilliseconds();
      const auto [width,height]=nativeEngine->renderingSize();
      showMessage(juce::String(width)+" x "+juce::String(height)+" / "+juce::String(ms>0?1000/ms:0,1)+" FPS");
    }
    for(auto &control:knobs)if(control->isVisible())control->refresh();
    if(processor.workspace()==Workspace::multiple)workspaceAction.setButtonText(nativeEngine->multipleLayoutVisible()?"DONE":"LAYOUT");
    if(processor.workspace()==Workspace::multiple)midiTile.setSelectedId(decodeChoice(processor.value("multiTile"),36)+1,juce::dontSendNotification);
    record.setToggleState(recorder.active(),juce::dontSendNotification);
    midi.setToggleState(processor.learning,juce::dontSendNotification);
    if(recorder.active())record.repaint();home.setEnabled(!recorder.active());
    if(nativeEngine->requestedHome() && !recorder.active())selectWorkspace(Workspace::lab,true);
    return;
  }
#endif
#if GRID_USE_VEL_ENGINE
  if(isHome && controlsWindow && nativeEngine) {
    const auto image=nativeEngine->latestFrame();
    if(image.isValid())canvas.presentNative(image,nativeEngine->requestedScene());
  }
#endif
  if (panel && settingsScreen == SettingsPage::audio) {
    for (auto* child : panel->getChildren())
      if (auto* activity = dynamic_cast<juce::Label*>(child->findChildWithID("capture-activity"))) {
        const bool system = processor.getAssetProperty("audioSource") == "system";
        auto reading = processor.value("muteInput") >= 64 ? juce::String("Analysis muted")
          : audio.rms < .00001f ? juce::String("No signal - play music on this device")
          : juce::String(20*std::log10(audio.rms),1)+" dB input";
        activity->setText((system ? processor.getAssetProperty("audioCaptureDevice") : juce::String("Microphone"))+
          " / "+reading, juce::dontSendNotification);
      }
  }
  if (reviewEnabled) {
    const auto &test = reviewCases[(size_t)reviewIndex];
    processor.setAssetProperty("imagePath", test.imagePath);
    processor.setAssetProperty("fontPath", test.fontPath);
    const auto words = test.fixture?"[\"\"]":test.imagePath.isNotEmpty() || test.fontPath.isNotEmpty()
                           ? "[\"JAH\"]"
                           : "[\"GRID-ARVx\"]";
    if (processor.getAssetProperty("lyrics") != words)
      processor.setAssetProperty("lyrics", words);
    if (loadedImage != test.imagePath || loadedFont != test.fontPath)
      refreshAssets();
    if (reviewApplied != reviewIndex) {
      reviewApplied=reviewIndex;
      const auto name=templates(test.category)[test.variant];
      const auto file=presets.directory(test.category,"Defaults").getChildFile(name+".json");
      if(file.existsAsFile()) presets.load(file);
      else { processor.set("category",encodeChoice(test.category,9)); processor.set("variant",encodeChoice(test.variant,16)); }
      processor.set("textEffect",encodeChoice(test.textEffect,7));
      for(auto id:fxIDs) processor.set(id,0);
      if(test.fx>=0) {
        processor.set(fxIDs[test.fx],test.amount);
        processor.set((juce::String(fxIDs[test.fx])+"Type").toRawUTF8(),encodeChoice(test.profile,profiles(test.fx).size()));
      }
      processor.setAssetProperty("lyrics",words);
      processor.setAssetProperty("imagePath",test.imagePath);
      processor.setAssetProperty("fontPath",test.fontPath);
      if(test.fixture) {
        processor.set("speed",0);processor.set("glow",0);processor.set("textEffect",0);
        processor.set("imageScale",64);
        for(const auto *fx:fxIDs)processor.set((juce::String(fx)+"React").toRawUTF8(),0);
      }
    }
    audio.averageMagnitude = .25f;
    audio.rms = .3f;
    audio.bass = .25f;
    audio.mid = .2f;
    audio.high = .1f;
    for(int i=0;i<2048;++i)audio.scope[(size_t)i]=.5f*std::sin(i*.01f+(float)time);
    for (int i = 0; i < 256; ++i) {
      audio.waveform[(size_t)i] = .5f * std::sin(i * .08f + (float)time);
      audio.left[(size_t)i] = audio.waveform[(size_t)i];
      audio.right[(size_t)i] = .5f * std::cos(i * .12f + (float)time);
    }
  }
  if (recordingCheck) {
    audio.rms = .3f + .1f * std::sin((float)time * 4);
    audio.bass = .25f;
    audio.mid = .2f;
    audio.high = .1f;
    audio.averageMagnitude=.25f;
    for(int i=0;i<2048;++i)audio.scope[(size_t)i]=.5f*std::sin(i*.01f+(float)time);
    for (int i = 0; i < 256; ++i) {
      audio.waveform[(size_t)i] = .5f * std::sin(i * .08f + (float)time);
      audio.left[(size_t)i] = audio.waveform[(size_t)i];
      audio.right[(size_t)i] = .5f * std::cos(i * .12f + (float)time);
    }
  }
  unsigned beats = audio.beats;
  double bpm = processor.hostBpm.load();
  if (bpm > 0) {
    beats = 0; // A playing host clock replaces audio onset timing.
    if (lastHostBpm != bpm)
      hostClockTime = now;
    lastHostBpm = bpm;
    const double period = 60 / bpm;
    if (now - hostClockTime >= period) {
      const double count = std::floor((now - hostClockTime) / period);
      beats = (unsigned)juce::jmin(128.0, count);
      hostClockTime += count * period;
    }
  } else {
    lastHostBpm = 0;
  }
  advanceCycles(beats,clockDelta);
  sync();
  for (int i = 0; i < (int)lyricNumbers.size(); ++i)
    lyricNumbers[(size_t)i]->setToggleState(
        i == decodeChoice(processor.value("lyric"), assets.lyrics.size()),
        juce::dontSendNotification);
  if (!isHome || (controlsWindow && processor.workspace()==Workspace::lab)) {
    SceneState s;
    for (size_t i = 0; i < controls.size(); ++i)
      s.values[i] = processor.value(controls[i].id);
    s.audio = audio;
    s.presetColors={processor.visualColour("color",s.category()==7),processor.visualColour("color2"),processor.visualColour("textColor")};
    s.captureTag = reviewEnabled ? reviewIndex : -1;
    s.tracing = !assets.contour.isEmpty();
    s.hasImage = assets.image.isValid();
    s.time = time;
    s.nativeDefaultColour = juce::File(processor.getAssetProperty("presetFile")).getParentDirectory().getFileName() == "Defaults" &&
      processor.value("color") == defaultTemplateColor(s.category(), s.variant());
    auto nativeProfile=juce::JSON::parse(processor.getAssetProperty("nativeEngineValues"));
    if(nativeProfile["color"].isObject() && (int)nativeProfile["category"]==s.category() &&
      (int)nativeProfile["variant"]==s.variant() && (int)nativeProfile["color"]["cc"]==processor.value("color"))s.nativeDefaultColour=true;
    nativeDynamics.update(s);
    if(s.category()==4) {
      static constexpr bool secondary[]{false,false,true,false,true,true,false,true,false,false,true};
      s.secondaryPalette=secondary[s.variant()] || processor.value("color2")!=colorCC(nativeDefaultColors(4,s.variant())[1]);
    }
    if(s.category()==8)s.secondaryPalette=processor.value("color2")!=colorCC(nativeDefaultColors(8,s.variant())[1]);
    s.detail = quality;
    const auto resolution=renderResolution(processor.value("renderResolution"));
    s.width=resolution.width;s.height=resolution.height;
    juce::Image rendered;
#if GRID_USE_VEL_ENGINE
    if (nativeEngine && nativeEngine->ready()) {
      auto lines=assets.lyrics;
      if (auto result = nativeEngine->setLyrics(lines,decodeChoice(processor.value("lyric"),lines.size())); result.failed())
        showMessage(result.getErrorMessage());
      nativeEngine->setExactValues(juce::JSON::parse(processor.getAssetProperty("nativeEngineValues")));
      nativeEngine->update(s);
      auto imagePath=processor.getAssetProperty("imagePath"), fontPath=processor.getAssetProperty("fontPath");
      if(imagePath!=nativeImagePath || nativeImageCategory!=s.category()) {
        if(imagePath.isNotEmpty()) {
          if(auto result=nativeEngine->importFile(juce::File(imagePath));result.failed())showMessage(result.getErrorMessage());
        } else nativeEngine->clearTextImage();
        nativeImagePath=imagePath;
        nativeImageCategory=s.category();
      }
      if(fontPath!=nativeFontPath) {
        if(fontPath.isNotEmpty()) {
          if(auto result=nativeEngine->importFile(juce::File(fontPath));result.failed())showMessage(result.getErrorMessage());
        } else nativeEngine->clearTextFont();
        nativeFontPath=fontPath;
      }
      rendered = nativeEngine->latestFrame();
      if (rendered.isValid()) {
        canvas.presentNative(rendered, s);
        if (outputCanvas) outputCanvas->presentNative(rendered, s);
      }
      if (reviewEnabled || recordingCheck) {
        const auto &scope = audio.scope;
        nativeEngine->pushAudio(scope.data(), scope.data(), (int)scope.size(), 48000);
      }
    } else {
      showMessage(nativeEngineError.isNotEmpty() ? nativeEngineError : "Waiting for the Vel Engine renderer.");
    }
#else
    rendered = drawScene(s, assets);
    if (reviewEnabled && reviewCases[(size_t)reviewIndex].fixture) {
      const auto &name = reviewCases[(size_t)reviewIndex].name;
      s.time = name.startsWith("symbol-time-")
                   ? 2 + .375 * name.getTrailingIntValue()
                   : 2;
      rendered = juce::Image(juce::Image::ARGB, s.width, s.height, true,
                             juce::SoftwareImageType());
      juce::Graphics fixture(rendered);
      const int kind = reviewCases[(size_t)reviewIndex].fixture;
      fixture.fillAll(juce::Colours::black);
      if (kind == 1) {
        const int width = s.width / 3;
        for (int band = 0; band < 3; ++band) {
          const auto level = (juce::uint8)(band == 0   ? 10
                                           : band == 1 ? 128
                                                       : 235);
          fixture.setColour(juce::Colour(level, level, level));
          fixture.fillRect(band * width, 0,
                           band == 2 ? s.width - band * width : width,
                           s.height);
        }
      } else {
        fixture.setColour(juce::Colours::white);
        const int step = juce::jmax(12, s.height / 18);
        for (int x = 0; x < s.width; x += step)
          fixture.fillRect(x, 0, 2, s.height);
        for (int y = 0; y < s.height; y += step)
          fixture.fillRect(0, y, s.width, 2);
        fixture.fillEllipse(s.width * .44f, s.height * .44f, s.width * .12f,
                            s.height * .12f);
      }
    }
    canvas.present(rendered, s);
    if (outputCanvas)
      outputCanvas->present(rendered, s);
#endif
  }
  if (!reviewEnabled && !recorder.active() && canvas.capture) {
    canvas.capture = false;
    setResizeLimits(960, 620, 2560, 1600);
    setResizable(true, true);
    if (auto *w = dynamic_cast<juce::DocumentWindow *>(getTopLevelComponent()))
      w->setResizable(true, false);
    if (recorder.error().isNotEmpty())
      showMessage(recorder.error());
  }
  if (recorder.active())
    status.setText(
        (recorder.finalizing() ? "Saving recording " : "Recording ") +
            juce::String(recorder.elapsed(), 1) + "s / dropped frames " +
            juce::String(recorder.dropped.load()),
        juce::dontSendNotification);
  else if (processor.value("stats") >= 64) {
#if GRID_USE_VEL_ENGINE
    if(isHome && !controlsWindow && processor.value("pauseMenuRendering")>=64)showMessage("Visual rendering paused in menus");
    else if(nativeEngine) {
      const auto [width,height]=nativeEngine->renderingSize();
      const double ms=nativeEngine->frameMilliseconds();
      showMessage(juce::String(width)+" x "+juce::String(height)+" / "+juce::String(ms>0?1000/ms:0,1)+" FPS");
    }
#else
    const double ms=canvas.frameMilliseconds.load();
    showMessage(juce::String(ms>0?1000/ms:0,1)+" FPS");
#endif
  }
  if(recorder.active())record.repaint();
  home.setEnabled(!recorder.active());
  if (recordingCheckTick)
    recordingCheckTick();
}
void Editor::runRenderChecks(const juce::File &folder,
                             std::function<void()> done, bool effectsOnly) {
  reviewFolder = folder;
  folder.createDirectory();
  folder.getChildFile("backend.json").replaceWithText(
#if GRID_USE_VEL_ENGINE
    "{\"backend\":\"Vel Engine\",\"pixelExpectations\":\"native\"}"
#else
    "{\"backend\":\"Independent renderer\",\"pixelExpectations\":\"reference-fx\"}"
#endif
  );
  checksFinished = std::move(done);
  for (auto control : controls)
    processor.set(control.id, control.initial);
  actionValues.fill(0);
  processor.setAssetProperty("presetName", "");
  processor.setAssetProperty("presetFile", "");
  processor.setAssetProperty("lyrics", "[\"GRID-ARVx\"]");
  processor.setAssetProperty("imagePath", "");
  processor.setAssetProperty("fontPath", "");
  refreshAssets();
  isHome = false;
  updatePresentation();
  reviewIndex = 0;
  reviewFailures = 0;
  reviewPresented = 0;
  reviewCases.clear();
  for (int c = 0; c < 9; ++c)
    reviewCases.push_back({c, 2, -1, 0, 0, categories[c] + " sample"});
  for (int fx = 0; fx < 4; ++fx)
    for (int profile = 0; profile < profiles(fx).size(); ++profile)
      reviewCases.push_back(
          {0, 1, fx, profile, 0,
           juce::String(fxIDs[fx]) + " / " + profiles(fx)[profile]});
  for (int c = 0; c < 9; ++c)
    for (int variant = 0; variant < templates(c).size(); ++variant)
      if (offeredTemplate(c, variant))
        reviewCases.push_back({c, variant, -1, 0, 0,
                               categories[c] + " / " + templates(c)[variant]});
  for (int effect = 0; effect < 7; ++effect)
    reviewCases.push_back(
        {7, 0, -1, 0, effect, "Text effect / " + textEffects[effect]});
  const auto logo =
      juce::SystemStats::getEnvironmentVariable("GRID_TEST_IMAGE", "");
  const auto font =
      juce::SystemStats::getEnvironmentVariable("GRID_TEST_FONT", "");
  const auto gif =
      juce::SystemStats::getEnvironmentVariable("GRID_TEST_GIF", "");
  if (logo.isNotEmpty() && juce::File(logo).existsAsFile()) {
    reviewCases.push_back({0, 1, -1, 0, 0, "Imported logo / traced", logo, {}});
    reviewCases.push_back(
        {7, 0, -1, 0, 0, "Imported logo / Text background", logo, font});
  }
  if (font.isNotEmpty() && juce::File(font).existsAsFile())
    reviewCases.push_back({7, 0, -1, 0, 0, "Imported OTF / Text", {}, font});
  if (gif.isNotEmpty() && juce::File(gif).existsAsFile())
    reviewCases.push_back(
        {7, 0, -1, 0, 0, "Imported GIF / still background", gif, font});
  if (effectsOnly) {
    reviewCases.clear();
#if GRID_USE_VEL_ENGINE
    // Native FX need an actual native source. The independent renderer's
    // synthetic CPU fixture branch is not used by this backend. Import a
    // stationary Text background and an empty lyric instead of a moving scope.
    for(int kind=1;kind<=2;++kind) {
      juce::Image source(juce::Image::RGB,1280,720,true,juce::SoftwareImageType());
      juce::Graphics graphics(source);graphics.fillAll(juce::Colours::black);
      if(kind==1)for(int band=0;band<3;++band) {
        const auto value=(juce::uint8)(band==0?10:band==1?128:235);
        graphics.setColour(juce::Colour(value,value,value));graphics.fillRect(band*1280/3,0,(band+1)*1280/3-band*1280/3,720);
      } else {
        graphics.setColour(juce::Colours::white);
        for(int x=0;x<1280;x+=40)graphics.fillRect(x,0,2,720);
        for(int y=0;y<720;y+=40)graphics.fillRect(0,y,1280,2);
        graphics.fillEllipse(1280*.44f,720*.44f,1280*.12f,720*.12f);
      }
      if(auto stream=folder.getChildFile("fixture-"+juce::String(kind)+".png").createOutputStream())
        juce::PNGImageFormat().writeImageToStream(source,*stream);
    }
#endif
    auto add = [&](juce::String name, int fixture, int fx, int profile,
                   int amount) {
      ReviewCase test{0, 0, fx, profile, 0, name};
      test.fixture = fixture;
      test.amount = amount;
#if GRID_USE_VEL_ENGINE
      test.category=7;test.variant=0;
      test.imagePath=folder.getChildFile("fixture-"+juce::String(fixture)+".png").getFullPathName();
#endif
      reviewCases.push_back(test);
    };
    add("gray-baseline", 1, -1, 0, 0);
    for (int profile = 0; profile < 2; ++profile) {
      const auto label = profile == 0 ? "noise" : "grain";
      for (int amount : {0, 32, 80, 127})
        add(juce::String(label) + "-amount-" + juce::String(amount), 1, 0,
            profile, amount);
      for (int frame = 0; frame < 4; ++frame)
        add(juce::String(label) + "-time-" + juce::String(frame), 1, 0, profile,
            90);
    }
    add("glass-baseline", 2, -1, 0, 0);
    for (int amount : {0, 32, 64, 127})
      add("glass-ball-amount-" + juce::String(amount), 2, 3, 0, amount);
    for (int profile = 1; profile < 6; ++profile)
      add("glass-" + profiles(3)[profile], 2, 3, profile, 127);
    add("symbol-amount-0", 1, 1, 0, 0);
    add("symbol-amount-127", 1, 1, 0, 127);
    for (int frame = 0; frame < 3; ++frame)
      add("symbol-time-" + juce::String(frame), 1, 1, 0, 127);
  }
  const auto filter=juce::SystemStats::getEnvironmentVariable("GRID_REVIEW_MATCH","");
  if(filter.isNotEmpty()) {
    auto selected=juce::StringArray::fromTokens(filter,";","");
    reviewCases.erase(std::remove_if(reviewCases.begin(),reviewCases.end(),[&](const ReviewCase& item){
      for(const auto& part:selected)if(item.name.containsIgnoreCase(part))return false;
      return true;
    }),reviewCases.end());
  }
  if(reviewCases.empty()) {
    folder.getChildFile("result.txt").replaceWithText("FAIL No render cases match the requested filter.");
    if(checksFinished)checksFinished();
    return;
  }
  juce::String inventory;
  for (size_t i = 0; i < reviewCases.size(); ++i)
    inventory += juce::String((int)i).paddedLeft('0', 2) + ".png\t" +
                 reviewCases[i].name + "\n";
  folder.getChildFile("inventory.txt").replaceWithText(inventory);
  reviewEnabled = true;
  canvas.capture = true;
  resized();
}
} // namespace kone::grid
