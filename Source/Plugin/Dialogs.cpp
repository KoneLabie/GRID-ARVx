#include "Editor.h"
#include "EmbeddedFrame.h"
namespace kone::grid {
class Panel final : public EmbeddedFrame {
public:
  explicit Panel(juce::String t) : title(std::move(t)) {}
  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff1c1c20));
    g.setColour(juce::Colour(0xff4b4b51));
    g.drawRect(getLocalBounds(), 1);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(15, juce::Font::bold));
    const int left = findChildWithID("settings-back") ? 108 : 20;
    g.drawText(title, left, 12, getWidth() - left - 20, 36,
               juce::Justification::centred);
  }
  juce::String title;
};
class PerformanceMappingPanel final : public EmbeddedFrame,private juce::Timer {
public:
  PerformanceMappingPanel(Processor &p,std::function<void()> close,
      std::function<juce::Image(int,int)> graph)
      :processor(p),closed(std::move(close)),graphPresentation(std::move(graph)) {
    setComponentID("performance-mapping");
    bindings.setComponentID("performance-bindings");back.setComponentID("performance-mapping-back");
    graphSurface.setComponentID("performance-keyboard-graph");
    viewport.setComponentID("performance-bindings-scroll");
    setWantsKeyboardFocus(true);
    addAndMakeVisible(bindings);addAndMakeVisible(back);addAndMakeVisible(help);addAndMakeVisible(viewport);addAndMakeVisible(graphSurface);
    graphSurface.onResize=[this]{if(!editing)presentGraph();};
    bindings.onClick=[this]{editing=true;cancelLearning();refresh();};
    back.onClick=[this]{
      if(editing){editing=false;cancelLearning();refresh();}
      else if(closed)closed();
    };
    viewport.setViewedComponent(&body,false);viewport.setScrollBarsShown(true,false);
    for(size_t i=0;i<performanceActions.size();++i) {
      auto label=std::make_unique<juce::Label>();label->setText(performanceActions[i].name,juce::dontSendNotification);
      body.addAndMakeVisible(*label);names.push_back(std::move(label));
      auto key=std::make_unique<ToolButton>("");key->onClick=[this,i]{
        cancelLearning();keyboard=(int)i;help.setText("Press a shortcut. Esc cancels; Backspace clears it.",juce::dontSendNotification);
        refreshButtons();grabKeyboardFocus();
      };
      body.addAndMakeVisible(*key);keys.push_back(std::move(key));
      auto midiButton=std::make_unique<ToolButton>("");midiButton->onClick=[this,i]{
        cancelLearning();midiAction=(int)i;processor.learning.store(true);
        help.setText("Press a MIDI button, then confirm Button. Knobs cannot bind to these actions.",juce::dontSendNotification);
        refreshButtons();
      };
      body.addAndMakeVisible(*midiButton);midis.push_back(std::move(midiButton));
    }
    refresh();startTimerHz(20);
  }
  ~PerformanceMappingPanel() override {cancelLearning();}
  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xff1c1c20));g.setColour(juce::Colour(0xff4b4b51));g.drawRect(getLocalBounds());
    g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(15,juce::Font::bold));
    g.drawText(editing?"PERFORMANCE / BINDINGS":"PERFORMANCE / MAPPING",120,12,getWidth()-240,32,juce::Justification::centred);
    if(!editing)return;
    g.setFont(juce::FontOptions(12,juce::Font::bold));
    const int column=(getWidth()-56)/3;
    g.drawText("ACTION",22,94,column,24,juce::Justification::centredLeft);
    g.drawText("KEYBOARD",22+column,94,column,24,juce::Justification::centred);
    g.drawText("MIDI",22+column*2,94,column,24,juce::Justification::centred);
  }
  void resized() override {
    back.setBounds(16,14,76,30);bindings.setBounds(getWidth()-128,14,112,30);
    help.setBounds(20,49,getWidth()-40,40);
    graphSurface.setBounds(20,62,getWidth()-40,getHeight()-82);
    viewport.setBounds(20,122,getWidth()-40,getHeight()-142);
    body.setSize(viewport.getWidth()-viewport.getScrollBarThickness(),(int)performanceActions.size()*44);
    const int column=body.getWidth()/3;
    for(size_t i=0;i<names.size();++i) {
      const int y=(int)i*44;
      names[i]->setBounds(0,y,column,40);
      keys[i]->setBounds(column+4,y+3,column-8,34);
      midis[i]->setBounds(column*2+4,y+3,column-8,34);
    }
  }
  bool keyPressed(const juce::KeyPress &key) override {
    if(keyboard<0)return true; // The mapping panel never performs live shortcuts.
    if(key.getKeyCode()==juce::KeyPress::escapeKey){cancelLearning();refresh();return true;}
    processor.setPerformanceKey((size_t)keyboard,key.getKeyCode()==juce::KeyPress::backspaceKey?juce::KeyPress{}:key);
    cancelLearning();refresh();return true;
  }
  bool keyStateChanged(bool down) override {
    if(keyboard>=0 && down && juce::ModifierKeys::getCurrentModifiersRealtime().isCtrlDown()) {
      // Ctrl alone is the native auto-cue shortcut. Chords are captured by keyPressed.
      ctrlOnlyPending=true;
    }
    if(keyboard>=0 && !down && ctrlOnlyPending && !juce::ModifierKeys::getCurrentModifiersRealtime().isCtrlDown()) {
      processor.setPerformanceKey((size_t)keyboard,{0x11,0,0});cancelLearning();refresh();
    }
    return true;
  }
private:
  void cancelLearning() {
    keyboard=midiAction=-1;ctrlOnlyPending=false;processor.learning.store(false);
    MidiEvent ignored;while(processor.midiEvents.pop(ignored)){}
  }
  void refreshButtons() {
    const auto labels=processor.performanceBindingLabels();
    for(size_t i=0;i<keys.size();++i) {
      keys[i]->setEnabled(editing);midis[i]->setEnabled(editing);
      keys[i]->setButtonText(keyboard==(int)i?"Press shortcut...":labels.keyboard[i]);
      midis[i]->setButtonText(midiAction==(int)i?"Press MIDI button...":labels.midi[i]);
    }
  }
  void refresh() {
    bindings.setVisible(!editing);help.setVisible(editing);viewport.setVisible(editing);graphSurface.setVisible(!editing);
    help.setText("Click a Keyboard or MIDI cell to change its binding. Scroll for more actions.",juce::dontSendNotification);
    refreshButtons();resized();presentGraph();repaint();
  }
  void presentGraph() {
    if(!editing && graphPresentation && graphSurface.isShowing() && !graphSurface.getLocalBounds().isEmpty())
      graphSurface.image=graphPresentation(graphSurface.getWidth(),graphSurface.getHeight());
    graphSurface.repaint();
  }
  void timerCallback() override {
    if(midiAction<0 || confirming)return;
    MidiEvent event;
    if(!processor.midiEvents.pop(event) || event.value==0)return;
    confirming=true;const auto action=(size_t)midiAction;
    juce::Component::SafePointer<PerformanceMappingPanel> safe(this);
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withTitle("Confirm MIDI hardware")
      .withMessage("Bind "+juce::String(performanceActions[action].name)+" to channel "+juce::String(event.channel)+
        (event.note?" note ":" CC ")+juce::String(event.cc)+". Choose the physical control type.")
      .withButton("Button").withButton("Knob").withButton("Cancel"),[safe,action,event](int choice) {
        if(!safe)return;safe->confirming=false;
        if(choice==1 && (event.value==0 || event.value==127)) {
          safe->processor.bindPerformanceMidi(action,event.channel,event.cc,event.note);
          safe->cancelLearning();safe->refresh();
        } else {
          safe->help.setText(choice==2 || (choice==1 && event.value!=127)?
            "Rejected: Performance actions require a MIDI button sending 0 and 127.":"Binding cancelled.",juce::dontSendNotification);
          safe->cancelLearning();safe->refreshButtons();
        }
      });
  }
  Processor &processor;
  std::function<void()> closed;
  std::function<juce::Image(int,int)> graphPresentation;
  struct GraphSurface final : juce::Component {
    std::function<void()> onResize;
    juce::Image image;
    void resized() override {if(onResize)onResize();}
    void paint(juce::Graphics &g) override {if(image.isValid())g.drawImageAt(image,0,0);}
  } graphSurface;
  ToolButton bindings{"BINDINGS"},back{"BACK"};
  juce::Label help;
  juce::Viewport viewport;
  juce::Component body;
  std::vector<std::unique_ptr<juce::Label>> names;
  std::vector<std::unique_ptr<ToolButton>> keys,midis;
  int keyboard=-1,midiAction=-1;
  bool editing=false,confirming=false,ctrlOnlyPending=false;
};
void Editor::performanceMapping() {
  if(panel)closePanel();processor.learning.store(false);selectedControl.clear();
  midiStrobes=midiBlackout=autoKeyDown=false;
  const juce::Component::SafePointer<Editor> safe(this);
  std::function<juce::Image(int,int)> graph;
#if GRID_USE_VEL_ENGINE
  graph=[engine=nativeEngine](int width,int height) {
    return engine?engine->keyboardGraphImage(width,height):juce::Image{};
  };
#endif
  panel=std::make_unique<PerformanceMappingPanel>(processor,[safe]{if(safe)safe->closePanel();},std::move(graph));
  uiHost().addAndMakeVisible(*panel);resized();panel->enterModalState(!quietDesktopChecks,nullptr,false);
}
bool Editor::performanceKeyPressed(const juce::KeyPress &key) {
  if(panel || processor.learning.load() || isHome)return true;
  if(autoKeyDown)autoKeyChord=true;
#if GRID_USE_VEL_ENGINE
  for(size_t i=0;i<performanceActions.size();++i) {
    const auto binding=processor.performanceKey(i);
    if(binding.isValid() && samePerformanceKey(key,binding)) {
      if(nativeEngine && i!=30)nativeEngine->triggerPerformanceAction(i);
      return true;
    }
  }
#endif
  return true; // A remapped action's previous shortcut must stop working.
}
void Editor::updatePerformanceKeys() {
#if GRID_USE_VEL_ENGINE
  if(!nativeEngine)return;
  MidiEvent event;
  while(processor.performanceEvents.pop(event)) {
    if(!performanceLive || panel || processor.learning.load())continue;
    if(event.cc<0 || event.cc>=(int)performanceActions.size())continue;
    const auto id=juce::String(performanceActions[(size_t)event.cc].id);
    if(id=="strobes")midiStrobes=event.value==127;
    else if(id=="blackout")midiBlackout=event.value==127;
    else if(event.value==127)nativeEngine->triggerPerformanceAction((size_t)event.cc);
  }
  const bool active=performanceLive && !panel && !processor.learning.load();
  const bool keyboardActive=active && juce::Process::isForegroundProcess();
  const auto down=[&](size_t action) {
    const auto key=processor.performanceKey(action);
    if(!key.isValid())return false;
    const auto mods=juce::ModifierKeys::getCurrentModifiersRealtime();
    if(key.getKeyCode()==0x11)return mods.isCtrlDown();
    return juce::KeyPress::isKeyCurrentlyDown(key.getKeyCode()) &&
      (mods.getRawFlags()&juce::ModifierKeys::allKeyboardModifiers)==key.getModifiers().getRawFlags();
  };
  nativeEngine->setPerformanceHeld(performanceLive,active && (midiStrobes || (keyboardActive && down(24))),
    active && (midiBlackout || (keyboardActive && down(25))));
  const bool autoDown=keyboardActive && down(30);
  const double now=juce::Time::getMillisecondCounterHiRes();
  if(autoDown && !autoKeyDown){autoKeyStart=now;autoKeyChord=false;}
  if(!autoDown && autoKeyDown && keyboardActive && !autoKeyChord && now-autoKeyStart<300)
    nativeEngine->triggerPerformanceAction(30);
  autoKeyDown=autoDown;
#endif
}
void Editor::intervalDialog(const juce::String &id, bool lyric) {
  closePanel();
  panel = std::make_unique<Panel>(lyric ? "LYRIC CYCLE SETTINGS"
                                        : "CYCLE SETTINGS");
  uiHost().addAndMakeVisible(*panel);
  class IntervalControl final : public juce::Component {
  public:
    class LearnSlider final : public juce::Slider {
    public:
      std::function<bool()> select;
      void mouseDown(const juce::MouseEvent &e) override {
        if (e.mods.isPopupMenu())
          return;
        if (select && select())
          return;
        Slider::mouseDown(e);
      }
      void mouseDrag(const juce::MouseEvent &e) override {
        if (blocked && blocked())
          return;
        Slider::mouseDrag(e);
      }
      std::function<bool()> blocked;
    } slider;
    IntervalControl(Processor &p, const juce::String &target,
                    std::function<bool()> learn) {
      addAndMakeVisible(slider);
      slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 30);
      slider.select = std::move(learn);
      slider.blocked = [&p] { return p.learning.load(); };
      attachment = std::make_unique<
          juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, target,
                                                                slider);
      slider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v) + 1);
      };
      slider.valueFromTextFunction = [](const juce::String &t) {
        return (double)t.getIntValue() - 1;
      };
      slider.updateText();
    }
    void resized() override { slider.setBounds(getLocalBounds()); }
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attachment;
  };
  auto interval = std::make_unique<IntervalControl>(
      processor, id, [this, id] { return learn(id); });
  interval->setBounds(250, 120, 180, 180);
  panel->addAndMakeVisible(*interval);
  panelWidgets.push_back(std::move(interval));
  if (lyric) {
    auto mode = std::make_unique<CCControl>(processor, "lyricTimeMode",
                                            "SECONDS", true);
    mode->setBounds(270, 315, 140, 45);
    mode->select = [this] { return learn("lyricTimeMode"); };
    panel->addAndMakeVisible(*mode);
    panelWidgets.push_back(std::move(mode));
  }
  auto description = std::make_unique<juce::Label>();
  description->setText(lyric
                           ? "Choose any interval from 1 to 128.\nSECONDS off "
                             "= beats; SECONDS on = seconds.\nIn MIDI Learn, "
                             "select the interval knob or mode toggle."
                           : "Choose any interval from 1 to 128 beats.\nType a "
                             "number below the knob for exact timing.\nIn MIDI "
                             "Learn, select the interval knob to bind it.",
                       juce::dontSendNotification);
  description->setBounds(70, 375, 540, 70);
  panel->addAndMakeVisible(*description);
  panelWidgets.push_back(std::move(description));
  auto done = std::make_unique<ToolButton>("DONE");
  done->setBounds(240, 490, 200, 42);
  done->onClick = [this] { closePanel(); };
  panel->addAndMakeVisible(*done);
  panelWidgets.push_back(std::move(done));
  resized();
  panel->enterModalState(false, nullptr, false);
}
void Editor::saveDialog() {
  closePanel();
  panel = std::make_unique<Panel>("SAVE PRESET");
  uiHost().addAndMakeVisible(*panel);
  auto input = std::make_unique<juce::TextEditor>();
  auto *name = input.get();
  name->setTextToShowWhenEmpty(u8"Enter preset name…",
                               juce::Colour(0xff888888));
  name->setBounds(40, 110, 560, 38);
  panel->addAndMakeVisible(*name);
  panelWidgets.push_back(std::move(input));
  auto warning = std::make_unique<juce::Label>();
  auto *note = warning.get();
  note->setBounds(40, 160, 560, 60);
  panel->addAndMakeVisible(*note);
  panelWidgets.push_back(std::move(warning));
  auto submit = std::make_unique<ToolButton>("SAVE");
  auto *button = submit.get();
  button->setBounds(360, 240, 160, 42);
  button->setEnabled(false);
  auto armed = std::make_shared<juce::String>();
  name->onTextChange = [name, button, armed, note] {
    button->setEnabled(Presets::validName(name->getText()));
    button->setButtonText("SAVE");
    armed->clear();
    note->setText("", juce::dontSendNotification);
  };
  button->onClick = [this, name, note, button, armed] {
    int c = decodeChoice(processor.value("category"), 9);
    auto text = name->getText();
    bool overwrite = *armed == text;
    if (presets.userFile(c, text).existsAsFile() && !overwrite) {
      *armed = text;
      button->setButtonText("OVERWRITE");
      note->setText("This User preset already exists. Select OVERWRITE to "
                    "confirm replacement.",
                    juce::dontSendNotification);
      return;
    }
    setLyrics();
    auto result = presets.save(c, text, overwrite);
    if (result.failed())
      note->setText(result.getErrorMessage(), juce::dontSendNotification);
    else {
      closePanel();
      sync();
      showMessage("Preset saved.");
    }
  };
  panel->addAndMakeVisible(*button);
  panelWidgets.push_back(std::move(submit));
  auto cancel = std::make_unique<ToolButton>("CANCEL");
  cancel->setBounds(120, 240, 160, 42);
  cancel->onClick = [this] { closePanel(); };
  panel->addAndMakeVisible(*cancel);
  panelWidgets.push_back(std::move(cancel));
  resized();
  panel->enterModalState(false, nullptr, false);
  if(!quietDesktopChecks)name->grabKeyboardFocus();
}
void Editor::stateFile(bool saving) {
  chooser = std::make_unique<juce::FileChooser>(
      saving ? "Save complete state" : "Load complete state",
      juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
      "*.gridstate");
  juce::Component::SafePointer<Editor> safe(this);
  chooser->launchAsync(
      saving ? juce::FileBrowserComponent::saveMode |
                   juce::FileBrowserComponent::canSelectFiles |
                   juce::FileBrowserComponent::warnAboutOverwriting
             : juce::FileBrowserComponent::openMode |
                   juce::FileBrowserComponent::canSelectFiles,
      [safe, saving](const juce::FileChooser &fc) {
        if (!safe)
          return;
        auto f = fc.getResult();
        if (f == juce::File())
          return;
        const auto result =
            saving ? safe->writeStateFile(f) : safe->readStateFile(f);
        if (result.failed())
          safe->showMessage(result.getErrorMessage());
      });
}
juce::Result Editor::writeStateFile(const juce::File &file) {
  setLyrics();
  juce::MemoryBlock data;
  processor.getStateInformation(data);
  return file.withFileExtension("gridstate")
                 .replaceWithData(data.getData(), data.getSize())
             ? juce::Result::ok()
             : juce::Result::fail("Unable to save state.");
}
juce::Result Editor::readStateFile(const juce::File &file) {
  if (file.getSize() > 16 * 1024 * 1024)
    return juce::Result::fail("State file exceeds 16 MB.");
  juce::MemoryBlock data;
  if (!file.loadFileAsData(data) ||
      !processor.restoreState(data.getData(), (int)data.getSize()))
    return juce::Result::fail(
        "Invalid or unsupported state file. Current settings were preserved.");
  refreshAssets();
  sync();
  const auto result =
      systemAudio
          ? systemAudio(processor.getAssetProperty("audioSource") == "system")
          : juce::Result::ok();
  if (settingsScreen == SettingsPage::general)
    settingsPage();
  else if (settingsScreen == SettingsPage::graphics)
    advancedGraphics();
  else if (settingsScreen == SettingsPage::audio)
    audioPage();
  return result;
}
void Editor::selectEncoderFile(const juce::File &file) {
  if (file.existsAsFile()) {
    processor.setAssetProperty("encoderPath", file.getFullPathName());
    showMessage("Recording encoder selected: " + file.getFileName());
  }
}
// Preferences display real units. The processor retains its integer host/MIDI
// representation, with conversion at the UI boundary.
class SettingSlider final : public juce::Slider, private juce::Timer {
public:
  SettingSlider(Processor &p, const char *target, double minimum,
                double maximum, double step, const juce::String &suffix,
                std::function<double(int)> display,
                std::function<int(double)> encode)
      : processor(p), id(target), native(std::move(display)) {
    setComponentID(id);
    setSliderStyle(juce::Slider::LinearHorizontal);
    setTextBoxStyle(juce::Slider::TextBoxRight, false, 95, 28);
    setRange(minimum, maximum, step);
    setTextValueSuffix(suffix);
    refresh();
    onValueChange = [this, encode = std::move(encode)] {
      processor.set(id.toRawUTF8(), encode(getValue()));
      if (changed)
        changed();
    };
    startTimerHz(12);
  }
  void refresh() {
    bool editing = false;
    for (auto *child : getChildren())
      if (auto *label = dynamic_cast<juce::Label *>(child))
        editing |= label->isBeingEdited();
    if (!isMouseButtonDown() && !editing)
      setValue(native(processor.value(id.toRawUTF8())),
               juce::dontSendNotification);
  }

  std::function<void()> changed;

private:
  void timerCallback() override { refresh(); }
  Processor &processor;
  juce::String id;
  std::function<double(int)> native;
};
class TierChoice final : public juce::ComboBox, private juce::Timer {
public:
  explicit TierChoice(std::function<int()> read) : current(std::move(read)) {
    setComponentID("graphics-tier");
    addItemList({"Potato", "Light", "Medium", "Medium Heavy", "Heavy",
                 "Extreme", "Ultra", "Custom"},
                1);
    refresh();
    startTimerHz(12);
  }
  void refresh() {
    if (!isPopupActive())
      setSelectedId(current() + 1, juce::dontSendNotification);
  }

private:
  void timerCallback() override { refresh(); }
  std::function<int()> current;
};
void Editor::mountSettingsBack() { panel->addAndMakeVisible(back); }
void Editor::navigateBack() {
  const auto screen = settingsScreen;
  if (screen == SettingsPage::graphics || screen == SettingsPage::audio)
    settingsPage();
  else
    closePanel();
}
int Editor::currentGraphicsTier() const {
  for (int q = 0; q < (int)graphicsTiers.size(); ++q) {
    const auto &t = graphicsTiers[(size_t)q];
    if (processor.value("frameCap") == frameRateValue(t.fps) &&
        processor.value("renderResolution") == encodeChoice(q,(int)renderResolutions.size()) &&
#if !GRID_USE_VEL_ENGINE
        processor.value("renderDetail") == encodeChoice(t.detail, 7) &&
        processor.value("bloomQuality") == t.bloom &&
        processor.value("trackingRate") == t.tracking &&
        processor.value("trackingResolution") == encodeChoice(t.trackResolution, 4) &&
#endif
        processor.value("antialias") == (t.filter ? 127 : 0))
      return q;
  }
  return 7;
}
void Editor::applyGraphicsTier(int q) {
  if (q < 0 || q >= (int)graphicsTiers.size())
    return;
  const auto &t = graphicsTiers[(size_t)q];
  processor.set("renderResolution",encodeChoice(q,(int)renderResolutions.size()));
  processor.set("adaptive",0);
  processor.set("frameCap", frameRateValue(t.fps));
  processor.set("renderScale", scaleValue(t.scale));
  processor.set("renderDetail", encodeChoice(t.detail, 7));
  processor.set("bloomQuality", t.bloom);
  processor.set("trackingRate", t.tracking);
  processor.set("trackingResolution", encodeChoice(t.trackResolution, 4));
  processor.set("antialias", t.filter ? 127 : 0);
  fps = frameRate(processor.value("frameCap"));
  quality = t.detail;
  startTimerHz(fps);
  for (auto &widget : panelWidgets)
    if (auto *slider = dynamic_cast<SettingSlider *>(widget.get()))
      slider->refresh();
  showMessage("Graphics quality changed to " + juce::String(q + 1) + " of 7.");
}
void Editor::settingsPage() {
  closePanel();
  settingsScreen = SettingsPage::general;
  panel = std::make_unique<Panel>("SETTINGS / GRAPHICS & AUDIO");
  uiHost().addAndMakeVisible(*panel);
  mountSettingsBack();
  auto label = [this](juce::String text, int y) {
    auto l = std::make_unique<juce::Label>();
    l->setText(text, juce::dontSendNotification);
    l->setBounds(30, y, 250, 24);
    panel->addAndMakeVisible(*l);
    panelWidgets.push_back(std::move(l));
  };
  auto button = [this](juce::String text, int x, int y, int w,
                       std::function<void()> action) {
    auto b = std::make_unique<ToolButton>(text);
    b->setBounds(x, y, w, 34);
    b->onClick = std::move(action);
    panel->addAndMakeVisible(*b);
    panelWidgets.push_back(std::move(b));
  };
  label("Overall graphics quality", 62);
  auto tier =
      std::make_unique<TierChoice>([this] { return currentGraphicsTier(); });
  auto *choice = tier.get();
  choice->onChange = [this, choice] {
    applyGraphicsTier(choice->getSelectedId() - 1);
  };
  tier->setBounds(290, 62, 360, 28);
  panel->addAndMakeVisible(*tier);
  panelWidgets.push_back(std::move(tier));
  button("ADVANCED GRAPHICS", 30, 200, 300, [this] { advancedGraphics(); });
  button("AUDIO / MIDI DEVICES", 350, 200, 300, [this] { audioPage(); });
  button("SAVE COMPLETE STATE", 30, 248, 300, [this] { stateFile(true); });
  button("LOAD COMPLETE STATE", 350, 248, 300, [this] { stateFile(false); });
  button("RESET CONTROLS", 30, 296, 300, [this] {
    reset();
    showMessage("Visual controls reset. Graphics preferences and MIDI bindings "
                "were kept.");
  });
  button("OPEN PRESET FOLDERS", 350, 296, 300, [this] {
    showMessage(presets.root.startAsProcess()
                    ? "Preset folders opened."
                    : "Unable to open preset folders.");
  });
  button("CHOOSE FFMPEG ENCODER", 30, 344, 300, [this] {
    chooser = std::make_unique<juce::FileChooser>("Select FFmpeg executable",
                                                  juce::File(), "*");
    juce::Component::SafePointer<Editor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser &fc) {
                           if (safe)
                             safe->selectEncoderFile(fc.getResult());
                         });
  });
  button("CLEAR LAB MIDI BINDINGS", 350, 344, 300, [this] {
    processor.clearBindings();
    showMessage("MIDI bindings cleared.");
  });
  auto info = std::make_unique<juce::Label>();
  info->setBounds(30, 440, 620, 50);
  info->setText("Set resolution and frame rate in Advanced Graphics.\n"
                "Choose your microphone or speakers in Audio / MIDI Devices.",
                juce::dontSendNotification);
  panel->addAndMakeVisible(*info);
  panelWidgets.push_back(std::move(info));
  resized();
  panel->enterModalState(false, nullptr, false);
}
void Editor::advancedGraphics() {
  closePanel();
  settingsScreen = SettingsPage::graphics;
  panel = std::make_unique<Panel>("ADVANCED GRAPHICS");
  uiHost().addAndMakeVisible(*panel);
  mountSettingsBack();
  int y = 70;
  auto slider = [this, &y](const char *id, const char *name, double lo,
                           double hi, double step, const char *suffix,
                           std::function<double(int)> display,
                           std::function<int(double)> encode) {
    auto label = std::make_unique<juce::Label>();
    label->setText(name, juce::dontSendNotification);
    label->setBounds(30, y, 255, 28);
    panel->addAndMakeVisible(*label);
    panelWidgets.push_back(std::move(label));
    auto control =
        std::make_unique<SettingSlider>(processor, id, lo, hi, step, suffix,
                                        std::move(display), std::move(encode));
    control->setBounds(290, y, 340, 30);

    panel->addAndMakeVisible(*control);
    panelWidgets.push_back(std::move(control));
    y += 43;
  };
  auto resolutionLabel=std::make_unique<juce::Label>();
  resolutionLabel->setText("Render resolution",juce::dontSendNotification);
  resolutionLabel->setBounds(30,y,255,28);
  panel->addAndMakeVisible(*resolutionLabel);panelWidgets.push_back(std::move(resolutionLabel));
  class ResolutionChoice final : public juce::ComboBox, private juce::Timer {
  public:
    explicit ResolutionChoice(Processor &p):processor(p) {
      setComponentID("renderResolution");
      for(int i=0;i<(int)renderResolutions.size();++i)addItem(renderResolutions[(size_t)i].name,i+1);
      refresh();
      onChange=[this]{processor.set("renderResolution",encodeChoice(getSelectedId()-1,(int)renderResolutions.size()));};
      startTimerHz(12);
    }
  private:
    void refresh(){if(!isPopupActive())setSelectedId(decodeChoice(processor.value("renderResolution"),(int)renderResolutions.size())+1,juce::dontSendNotification);}
    void timerCallback() override{refresh();}
    Processor &processor;
  };
  auto resolution=std::make_unique<ResolutionChoice>(processor);
  resolution->setBounds(290,y,340,30);
  panel->addAndMakeVisible(*resolution);panelWidgets.push_back(std::move(resolution));y+=43;
  slider("frameCap", "Frame cap", 12, 120, 1, " FPS", frameRate,
         frameRateValue);

#if !GRID_USE_VEL_ENGINE
  slider(
      "renderDetail", "Geometry / particle detail", 1, 7, 1, " / 7",
      [](int v) { return decodeChoice(v, 7) + 1; },
      [](double v) { return encodeChoice((int)v - 1, 7); });
  slider(
      "bloomQuality", "Bloom kernel quality", 1, 6, 1, " / 6",
      [](int v) { return 1 + v * 5 / 127; },
      [](double v) { return (int)std::ceil((v - 1) * 127 / 5); });
  slider(
      "trackingRate", "Tracking update interval", 2, 12, 1, " frames",
      [](int v) { return 12 - v / 12; },
      [](double v) { return (12 - (int)v) * 12; });
  slider(
      "trackingResolution", "Tracking grid width", 32, 128, 32, " px",
      [](int v) { return 32 * (1 + decodeChoice(v, 4)); },
      [](double v) { return encodeChoice(juce::roundToInt(v / 32) - 1, 4); });
#endif
  const char *ids[] = {"antialias", "vsync", "pauseMenuRendering", "stats"};
  const char *names[] = {"Filtered sampling", "VSync",
                         "Pause visuals in menus", "Show FPS"};
  for (int i = 0; i < 4; ++i) {
    class SettingToggle final : public juce::ToggleButton, private juce::Timer {
    public:
      SettingToggle(Processor &p, const char *id, const char *name)
          : processor(p), target(id) {
        setComponentID(id);
        setButtonText(name);
        setToggleState(p.value(id) >= 64, juce::dontSendNotification);
        onClick = [this] {
          processor.set(target.toRawUTF8(), getToggleState() ? 127 : 0);
        };
        startTimerHz(12);
      }

    private:
      void timerCallback() override {
        setToggleState(processor.value(target.toRawUTF8()) >= 64,
                       juce::dontSendNotification);
      }
      Processor &processor;
      juce::String target;
    };
    auto toggle = std::make_unique<SettingToggle>(processor, ids[i], names[i]);
    toggle->setBounds(30 + (i % 2) * 300, y + 10 + (i / 2) * 42, 285, 32);
    panel->addAndMakeVisible(*toggle);
    panelWidgets.push_back(std::move(toggle));
  }
  auto description = std::make_unique<juce::Label>();
  description->setBounds(30, y+108, 600, 80);
  description->setText(
#if GRID_USE_VEL_ENGINE
      "Scenes render directly at the selected resolution, from 480p to 4K.\n"
      "The controls stay at your window resolution. Recording has its own output size.",
#else
                       "Changes apply immediately. VSync can limit preview FPS "
                       "to display refresh.\n"
                       "Scenes render at the selected resolution. "
                       "Tracking controls affect the Track FX.",
#endif
                       juce::dontSendNotification);
  panel->addAndMakeVisible(*description);
  panelWidgets.push_back(std::move(description));
  resized();
  panel->enterModalState(false, nullptr, false);
}
void Editor::audioPage() {
  closePanel();
  settingsScreen = SettingsPage::audio;
  panel = std::make_unique<Panel>("AUDIO / MIDI DEVICES");
  uiHost().addAndMakeVisible(*panel);
  mountSettingsBack();
  if (audioSettings) {
    auto devices = audioSettings();
    if (devices) {
      devices->setBounds(20, 63, 640, 460);
      panel->addAndMakeVisible(*devices);
      panelWidgets.push_back(std::move(devices));
    }
  } else {
    auto description = std::make_unique<juce::Label>();
    description->setBounds(50, 170, 580, 100);
    description->setText("Your DAW supplies audio and MIDI input. Select "
                         "devices and routing in the host.",
                         juce::dontSendNotification);
    panel->addAndMakeVisible(*description);
    panelWidgets.push_back(std::move(description));
  }
  resized();
  panel->enterModalState(false, nullptr, false);
}
void Editor::recordDialog() {
  closePanel();
  panel = std::make_unique<Panel>("RECORD VIDEO");
  uiHost().addAndMakeVisible(*panel);
  auto label = [this](juce::String t, int y) {
    auto l = std::make_unique<juce::Label>();
    l->setText(t, juce::dontSendNotification);
    l->setBounds(30, y, 190, 25);
    panel->addAndMakeVisible(*l);
    panelWidgets.push_back(std::move(l));
  };
  auto edit = [this](juce::String text, int y) {
    auto e = std::make_unique<juce::TextEditor>();
    auto *p = e.get();
    p->setText(text);
    p->setBounds(230, y, 400, 28);
    panel->addAndMakeVisible(*p);
    panelWidgets.push_back(std::move(e));
    return p;
  };
  label("Filename", 70);
  auto *name = edit("GRID-ARVx Capture", 70);
  label("Folder", 112);
  auto *folder =
      edit(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
               .getFullPathName(),
           112);
  folder->setBounds(230, 112, 285, 28);
  auto browse = std::make_unique<ToolButton>(u8"CHOOSE…");
  browse->setBounds(525, 112, 105, 28);
  browse->onClick = [this, folder] {
    chooser = std::make_unique<juce::FileChooser>(
        "Choose recording folder", juce::File(folder->getText()));
    juce::Component::SafePointer<Editor> safe(this);
    juce::Component::SafePointer<juce::TextEditor> target(folder);
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories,
                         [safe, target](const juce::FileChooser &fc) {
                           if (safe && target && fc.getResult().isDirectory())
                             target->setText(fc.getResult().getFullPathName());
                         });
  };
  panel->addAndMakeVisible(*browse);
  panelWidgets.push_back(std::move(browse));
  label("Resolution", 154);
  auto resolution = std::make_unique<juce::ComboBox>();
  auto *res = resolution.get();
  res->addItemList(
      {"640x360", "1280x720", "1920x1080", "2560x1440", "3840x2160"}, 1);
  res->setSelectedId(2);
  res->setBounds(230, 154, 400, 28);
  panel->addAndMakeVisible(*res);
  panelWidgets.push_back(std::move(resolution));
  label("Frame rate", 196);
  auto rate = std::make_unique<juce::ComboBox>();
  auto *fpsBox = rate.get();
  fpsBox->addItemList({"24", "30", "60", "120"}, 1);
  fpsBox->setSelectedId(2);
  fpsBox->setBounds(230, 196, 400, 28);
  panel->addAndMakeVisible(*fpsBox);
  panelWidgets.push_back(std::move(rate));
  label("Bitrate (kbps)", 238);
  auto *bits = edit("8000", 238);
  bits->setInputRestrictions(6, "0123456789");
  label("Duration limit (seconds)", 280);
  auto *seconds = edit("240", 280);
  seconds->setInputRestrictions(5, "0123456789");
  label("Container", 322);
  auto formatChoice = std::make_unique<juce::ComboBox>();
  auto *format = formatChoice.get();
  format->addItemList({"MP4", "Matroska / MKV"}, 1);
  format->setSelectedId(1);
  format->setBounds(230, 322, 400, 28);
  panel->addAndMakeVisible(*format);
  panelWidgets.push_back(std::move(formatChoice));
  label("Encoding", 364);
  auto codec = std::make_unique<juce::ComboBox>();
  auto *enc = codec.get();
  enc->addItemList({"Software MPEG-4", "NVIDIA NVENC H.264"}, 1);
  enc->setSelectedId(1);
  enc->setBounds(230, 364, 400, 28);
  panel->addAndMakeVisible(*enc);
  panelWidgets.push_back(std::move(codec));
  label("Visuals only; locked window size.", 410);
  auto warning = std::make_unique<juce::Label>();
  auto *note = warning.get();
  note->setBounds(30, 430, 600, 50);
  panel->addAndMakeVisible(*note);
  panelWidgets.push_back(std::move(warning));
  auto cancel = std::make_unique<ToolButton>("CANCEL");
  cancel->setBounds(120, 490, 160, 42);
  cancel->onClick = [this] { closePanel(); };
  panel->addAndMakeVisible(*cancel);
  panelWidgets.push_back(std::move(cancel));
  auto go = std::make_unique<ToolButton>("RECORD");
  go->setBounds(380, 490, 160, 42);
  go->onClick = [this, name, folder, res, fpsBox, bits, seconds, enc, format,
                 note, button = go.get()] {
    if (!Presets::validName(name->getText())) {
      note->setText("Enter a valid filename.", juce::dontSendNotification);
      return;
    }
    if (!juce::File::isAbsolutePath(folder->getText()) ||
        bits->getText().getIntValue() < 500 ||
        bits->getText().getIntValue() > 100000 ||
        seconds->getText().getIntValue() < 1 ||
        seconds->getText().getIntValue() > 7200) {
      note->setText(
          u8"Choose an absolute folder path, bitrate 500–100000 kbps, "
          u8"and duration 1–7200 seconds.",
          juce::dontSendNotification);
      return;
    }
    RecordSettings s;
    s.encoder = findEncoder(processor.getAssetProperty("encoderPath"));
    const int widths[] = {640, 1280, 1920, 2560, 3840},
              heights[] = {360, 720, 1080, 1440, 2160},
              rates[] = {24, 30, 60, 120};
    int index = res->getSelectedId() - 1;
    s.width = widths[index];
    s.height = heights[index];
    s.fps = rates[fpsBox->getSelectedId() - 1];
    s.bitrate = juce::jlimit(500, 100000, bits->getText().getIntValue());
    s.seconds = juce::jlimit(1, 7200, seconds->getText().getIntValue());
    s.nvidia = enc->getSelectedId() == 2;
    s.output =
        juce::File(folder->getText())
            .getChildFile(name->getText() +
                          (format->getSelectedId() == 1 ? ".mp4" : ".mkv"));
    if (s.nvidia && s.encoder.existsAsFile()) {
      button->setEnabled(false);
      note->setText(u8"Checking NVIDIA encoder…", juce::dontSendNotification);
      juce::Component::SafePointer<Editor> safe(this);
      juce::Component::SafePointer<juce::Component> dialog(panel.get());
      juce::Component::SafePointer<juce::Label> label(note);
      juce::Component::SafePointer<juce::TextButton> submit(button);
      juce::Thread::launch([safe, dialog, label, submit, s]() mutable {
        juce::ChildProcess probe;
        bool available =
            probe.start(juce::StringArray{
                s.encoder.getFullPathName(), "-hide_banner", "-f", "lavfi",
                "-i", "color=size=320x180:rate=1", "-frames:v", "1", "-c:v",
                "h264_nvenc", "-f", "null", "-"}) &&
            probe.waitForProcessToFinish(5000) && probe.getExitCode() == 0;
        if (probe.isRunning())
          probe.kill();
        s.nvidia = available;
        juce::MessageManager::callAsync(
            [safe, dialog, label, submit, s, available] {
              if (!safe || !dialog || !label || !submit)
                return;
              auto result = safe->startRecording(s);
              if (result.failed()) {
                submit->setEnabled(true);
                label->setText(result.getErrorMessage(),
                               juce::dontSendNotification);
              } else if (!available)
                safe->showMessage(
                    "NVENC unavailable; recording with software encoding.");
            });
      });
      return;
    }
    auto result = startRecording(s);
    if (result.failed()) {
      note->setText(result.getErrorMessage(), juce::dontSendNotification);
      return;
    }
  };
  panel->addAndMakeVisible(*go);
  panelWidgets.push_back(std::move(go));
  resized();
  panel->enterModalState(false, nullptr, false);
}
juce::Result Editor::startRecording(const RecordSettings &recordSettings) {
  auto result = recorder.start(recordSettings);
  if (result.failed())
    return result;
  canvas.capture = true;
  setResizeLimits(getWidth(), getHeight(), getWidth(), getHeight());
  setResizable(false, false);
  if (auto *window =
          dynamic_cast<juce::DocumentWindow *>(getTopLevelComponent()))
    window->setResizable(false, false);
  closePanel();
  return result;
}
} // namespace kone::grid
