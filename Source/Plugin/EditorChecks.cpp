#include "Editor.h"
#if JUCE_WINDOWS
#include <windows.h>
#undef small
#endif
namespace kone::grid {
juce::String Editor::runUiChecks(const juce::File &folder) {
  stopTimer();
  folder.createDirectory();
  Presets isolated(processor, folder.getChildFile("Presets"));
  presets.root = isolated.root;
  juce::String report;
  const bool quietDesktop=juce::SystemStats::getEnvironmentVariable("GRID_TEST_QUIET_DESKTOP","")=="1";
  const auto *testDisplay=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());
  const int quietMaximumWidth=testDisplay?juce::jmax(960,testDisplay->userArea.getWidth()-48):1120;
  auto check = [&](bool passed, const juce::String &name) {
    report += (passed ? "PASS " : "FAIL ") + name + "\n";
  };
  auto checkCaptureOverlay = [&](const juce::String &mode) {
#if JUCE_WINDOWS
    DWORD affinity=0;
    auto handle=controlsWindow && controlsWindow->getPeer()?static_cast<HWND>(controlsWindow->getPeer()->getNativeHandle()):nullptr;
    const bool affinityRead=handle && GetWindowDisplayAffinity(handle,&affinity);const auto error=GetLastError();
    report+="DIAG "+mode+" overlay capture="+juce::String((int)captureExcluded)+" read="+juce::String((int)affinityRead)+
      " affinity="+juce::String((int)affinity)+" setError="+juce::String(captureExclusionError)+" error="+juce::String((int)error)+
      " owned="+juce::String((int)(handle && GetWindow(handle,GW_OWNER)==static_cast<HWND>(getPeer()->getNativeHandle())))+"\n";
    check(handle && captureExcluded && affinityRead && affinity==0x11 &&
          GetWindow(handle,GW_OWNER)==static_cast<HWND>(getPeer()->getNativeHandle()),
          mode+" controls have Windows capture exclusion and belong to the visual output window");
    DWORD outputAffinity=0xffffffff;
    check(GetWindowDisplayAffinity(static_cast<HWND>(getPeer()->getNativeHandle()),&outputAffinity) && outputAffinity==0,
          mode+" visual output window remains available to capture");
    auto region=CreateRectRgn(0,0,0,0);
    const auto scale=controlsWindow->getPeer()->getPlatformScaleFactor();
    const bool shaped=GetWindowRgn(handle,region)!=ERROR;
    const int x=juce::roundToInt(controlsWindow->getWidth()*.5*scale);
    const bool toolbar=PtInRegion(region,x,juce::roundToInt(40*scale))!=FALSE;
    const bool middle=PtInRegion(region,x,juce::roundToInt(controlsWindow->getHeight()*.5*scale))!=FALSE;
    check(shaped && toolbar && (processor.workspace()==Workspace::multiple?middle:!middle),
          mode+" overlay shape preserves controls and visual openings");
    DeleteObject(region);
#else
    juce::ignoreUnused(mode);
#endif
  };
  auto snapshot = [&](const juce::String &name) {
    juce::Image image(juce::Image::ARGB, getWidth(), getHeight(), true,
                      juce::SoftwareImageType());
    {
      juce::Graphics g(image);
      overlaySnapshot = true;
      paintEntireComponent(g, true);
      overlaySnapshot = false;
    }
    auto stream = folder.getChildFile(name + ".png").createOutputStream();
    check(stream && juce::PNGImageFormat().writeImageToStream(image, *stream),
          "UI capture / " + name);
  };
  auto click = [](juce::TextButton &button) {
    auto action = button.onClick;
    if (action)
      action();
  };
  auto panelButton = [&](const juce::String &text) -> juce::TextButton * {
    for (auto &widget : panelWidgets)
      if (auto *button = dynamic_cast<juce::TextButton *>(widget.get()))
        if (button->getButtonText() == text)
          return button;
    return nullptr;
  };
  auto panelText = [&]() -> juce::TextEditor * {
    for (auto &widget : panelWidgets)
      if (auto *input = dynamic_cast<juce::TextEditor *>(widget.get()))
        return input;
    return nullptr;
  };
  isHome = true;
  resized();
  check(lab.isVisible() && settings.isVisible() &&
            multiple.isEnabled()==(GRID_USE_VEL_ENGINE!=0) &&
            performanceButton.isEnabled()==(GRID_USE_VEL_ENGINE!=0),
        "Home enables native workspaces only in the engine build");
  snapshot("home");
#if GRID_USE_VEL_ENGINE
  const auto initialBounds=getBounds();
  check(canvas.presentationTarget()==nullptr,"Home has no attached scene GL surface after startup");
  // Regression: enter the native menus first, without Lab or a window resize.
  for(auto *entry:{&performanceButton,&multiple}) {
    click(*entry);
    check(canvas.presentationTarget()==nullptr && !canvas.isVisible() && getBounds()==initialBounds,
          "Direct Home entry has no overlapping host GL surface or resize / "+entry->getButtonText());
    click(home);
    check(canvas.presentationTarget()==nullptr,"Returning Home leaves scene GL detached");
  }
#endif
  auto defaults = presets.files(0, "Defaults");
  check(defaults.size() == 15 &&
            defaults[0].getFileNameWithoutExtension() == "Wave" &&
            defaults[1].getFileNameWithoutExtension() == "Lisaj",
        "Default preset browser preserves the demonstrated template order");
  click(lab);
  check(canvas.presentationTarget()==this,"Lab attaches scene presentation to the editor");
  bool toolbarOrder = true;
  for (int width : {960, 1120, quietDesktop?quietMaximumWidth:2560}) {
    setSize(width, 740);
    toolbarOrder &= home.getRight() < midi.getX() &&
                    midi.getRight() < record.getX() &&
                    record.getRight() < category.getX() &&
                    full.getRight()<popOut.getX() && popOut.getRight()<hide.getX();
  }
  setSize(1120, 740);
  check(toolbarOrder,
        "Toolbar order is MENU, MIDI, Record, mode at every supported width");
  bool centredSelectors = true, controlsFit = true;
  for (int width : {960, 1120, quietDesktop?quietMaximumWidth:2560}) {
    setSize(width, 740);
    for (int mode = 0; mode < 9; ++mode) {
      processor.set("category", encodeChoice(mode, 9));
      sync();
      resized();
      // Native borders can constrain the maximum size; use the actual client
      // area.
      const int clientWidth = getWidth();
      centredSelectors &=
          std::abs(category.getBounds().getCentreX() * 2 - clientWidth) <= 1 &&
          std::abs(preset.getBounds().getCentreX() * 2 - clientWidth) <= 1;
      if (mode == 7)
        centredSelectors &=
            std::abs(textFx.getBounds().getCentreX() * 2 - clientWidth) <= 1;
      for (auto &knob : knobs) {
        if (!knob->isVisible())
          continue;
        controlsFit &= knob->getX() >= 0 && knob->getRight() <= clientWidth;
        if (knob->id == "presetCycle")
          centredSelectors &=
              std::abs(knob->getBounds().getCentreX() * 2 - clientWidth) <= 1;
      }
    }
  }
  setSize(1120, 740);
  check(centredSelectors, "Mode, preset, Text effect and Cycle remain at true "
                          "center across all modes / widths");
  check(controlsFit,
        "Extra side controls fit without moving the center selectors");
  processor.set("category", encodeChoice(4, 9));
  sync();
  resized();
  presets.userFile(4, "UI roundtrip")
      .deleteFile(); // Only this isolated test fixture.
  saveDialog();
  auto *input = panelText();
  auto *submit = panelButton("SAVE");
  check(input && submit && !submit->isEnabled(),
        "Save disabled for empty name");
  if (input && submit) {
    input->setText("../invalid");
    input->onTextChange();
    check(!submit->isEnabled(), "Save disabled for invalid path name");
    input->setText("UI roundtrip");
    input->onTextChange();
    snapshot("save-preset");
    click(*submit);
    check(!panel && presets.userFile(4, "UI roundtrip").existsAsFile() &&
              !presets.userFile(0, "UI roundtrip").existsAsFile(),
          "Save writes the current category's User folder");
    processor.set("color", 50);
    saveDialog();
    input = panelText();
    submit = panelButton("SAVE");
    input->setText("UI roundtrip");
    input->onTextChange();
    const auto original =
        presets.userFile(4, "UI roundtrip").loadFileAsString();
    click(*submit);
    check(panel && submit->getButtonText() == "OVERWRITE" &&
              original ==
                  presets.userFile(4, "UI roundtrip").loadFileAsString(),
          "Existing preset requires explicit overwrite confirmation");
    snapshot("overwrite-confirmation");
    click(*submit);
    check(!panel && original !=
                        presets.userFile(4, "UI roundtrip").loadFileAsString(),
          "Confirmed overwrite saves changed controls");
  }
  closePanel();
  processor.set("category", encodeChoice(7, 9));
  sync();
  resized();
  check(category.getButtonText() == "TEXT" && textFx.isVisible() &&
            lyricViewport.isVisible() &&
            presets.directory(7, "User") != presets.directory(6, "User"),
        "Text is an independent mode with its own effects, lyrics and User "
        "folder");
  const auto textNames = templates(7);
  auto textDefaults = presets.files(7, "Defaults");
  bool recordedOnly = textDefaults.size() == 11;
  for (const auto &file : textDefaults)
    recordedOnly &= file.getFileNameWithoutExtension() != "Normal";
  check(recordedOnly, "Text browser offers only the eleven recorded styles");
  {
    int count=0;
    for(int c=0;c<9;++c) count+=presets.files(c,"Defaults").size();
    check(count==87 && templates(4).contains("Pinwheel") && templates(3).contains("Skull") &&
      templates(8)[0]=="Mirror", "All 87 original Defaults are available; Arcade is excluded");
    const auto legacy=presets.directory(3,"Defaults").getChildFile("Ticker Tape.json");
    legacy.replaceWithText("{\"old\":true}");
    presets.directory(3,"Defaults").getChildFile("catalog-version.txt").replaceWithText("2");
    const auto userBefore=presets.userFile(4,"UI roundtrip").loadFileAsString();
    Presets migrated(processor,presets.root);
    check(!legacy.existsAsFile() && migrated.files(3,"Defaults").size()==8 &&
      migrated.directory(3,"Previous Defaults").getChildFile("Before catalog "+juce::String(templateCatalogRevision)+"/Ticker Tape.json").existsAsFile() &&
      userBefore==presets.userFile(4,"UI roundtrip").loadFileAsString(),
      "Default migration backs up old files and preserves User presets");
    const auto file=presets.directory(7,"Defaults").getChildFile("Ransom.json");
    auto data=juce::JSON::parse(file.loadFileAsString());
    data["parameters"].getDynamicObject()->setProperty("color",45);
    file.replaceWithText(juce::JSON::toString(data));
    const auto edited=file.loadFileAsString();
    Presets preserved(processor,presets.root);
    check(file.loadFileAsString()==edited,"Current catalog keeps subsequent user edits to Defaults");
  }
  check(textNames.contains("Pixel") && textNames.contains("Ransom") &&
            textNames.contains("Noskale") && textNames.contains("Picnic") &&
            textNames.contains("Flor") && textNames.contains("Myrtille"),
        "Recorded Text styles belong to Text, separately from the Pixel visual "
        "mode");
  auto text = folder.getChildFile("lyrics.txt");
  text.replaceWithText("ONE\n\nTWO\nTHREE\n");
  importAsset(text);
  check(assets.lyrics.size() == 3 && lyricEditors.size() == 3 &&
            assets.lyrics[1] == "TWO",
        "TXT imports nonempty lines in order");
  auto before = processor.getAssetProperty("lyrics");
  text.replaceWithText("VALID\n" + juce::String::repeatedString("X", 121));
  importAsset(text);
  check(before == processor.getAssetProperty("lyrics") &&
            status.getText().contains("line 2"),
        "Overlong TXT rejects entire import and identifies line");
  click(*lyricNumbers[1]);
  click(addLyric);
  check(assets.lyrics.size() == 4 &&
            decodeChoice(processor.value("lyric"), 4) == 1,
        "Adding a lyric preserves the active line");
  click(*lyricRemove[0]);
  check(assets.lyrics.size() == 3 &&
            decodeChoice(processor.value("lyric"), 3) == 0 &&
            assets.lyrics[0] == "TWO",
        "Removing a preceding lyric preserves the active text");
  lyricEditors[0]->setText("EDITED");
  lyricsDirty = true;
  setLyrics();
  check(processor.getAssetProperty("lyrics").contains("EDITED"),
        "Editing lyric fields updates persistent state");
  snapshot("lyrics");
  click(clearLyrics);
  check(assets.lyrics.size() == 1 && assets.lyrics[0].isEmpty() &&
            processor.value("lyric") == 0,
        "Clear lyrics resets editor and selection");
  juce::Image logo(juce::Image::ARGB, 200, 200, true,
                   juce::SoftwareImageType());
  {
    juce::Graphics g(logo);
    g.setColour(juce::Colours::white);
    g.fillEllipse(10, 10, 180, 180);
  }
  auto logoFile = folder.getChildFile("import-image.png");
  {
    auto stream = logoFile.createOutputStream();
    juce::PNGImageFormat().writeImageToStream(logo, *stream);
  }
  importAsset(logoFile);
  resized();
  check(assets.image.isValid() && assets.contour.isEmpty() &&
            clearImage.isVisible(),
        "Text image import exposes scale / clear");
  click(clearImage);
  check(!assets.image.isValid() &&
            processor.getAssetProperty("imagePath").isEmpty(),
        "Text image clear removes the stored asset");
  processor.set("category", 0);
  importAsset(logoFile);
  check(!assets.contour.isEmpty(),
        "Image import in Oscilloscope creates a contour");
  isHome=true;
  const juce::StringArray dropped{logoFile.getFullPathName()};
  check(isInterestedInFileDrag(dropped),"Home accepts an image drag");
  fileDragEnter(dropped,400,300);
  snapshot("drop-image-prompt");
  filesDropped(dropped,400,300);
  check(!isHome && assets.image.isValid() && !assets.contour.isEmpty() && !dragging,
    "Image dropped on Home opens Lab and creates a contour");
  processor.set("category",encodeChoice(4,9)); importAsset(logoFile);
  check(!assets.contour.isEmpty(),"Image tracing also replaces procedural mode output");
  auto referenceImage =
      juce::SystemStats::getEnvironmentVariable("GRID_TEST_IMAGE", "");
  if (referenceImage.isNotEmpty()) {
    importAsset(juce::File(referenceImage));
    check(assets.image.isValid() && !assets.contour.isEmpty(),
          "User reference logo imports as contours");
  }
  auto referenceFont =
      juce::SystemStats::getEnvironmentVariable("GRID_TEST_FONT", "");
  if (referenceFont.isNotEmpty()) {
    processor.set("category", encodeChoice(7, 9));
    importAsset(juce::File(referenceFont));
    check(assets.font != nullptr, "User reference OTF imports in Text");
  }
  processor.learning = true;
  check(learn("glassType", encodeChoice(2, 6)) &&
            selectedFixed == encodeChoice(2, 6),
        "FX profile can be selected as a MIDI button target");
  effectMenu(3);
  juce::PopupMenu::dismissAllActiveMenus();
  check(selectedControl == "glassType" && selectedFixed == encodeChoice(2, 6),
        "Opening an FX settings menu preserves MIDI selection");
  processor.learning = false;
  selectedControl.clear();
  processor.set("texture", 90);
  processor.set("textureReact", 127);
  processor.set("glassType", 127);
  processor.set("in", 127);
  processor.set("int", 127);
  processor.set("glow", 127);
  processor.set("color", 90);
  processor.set("textColorCycle", 127);
  reset();
  check(
      processor.value("texture") == 0 && processor.value("textureReact") == 0 &&
          processor.value("glassType") == 0 &&
          processor.value("in") == controls[(size_t)indexOf("in")].initial &&
          processor.value("int") == controls[(size_t)indexOf("int")].initial &&
          processor.value("glow") ==
              controls[(size_t)indexOf("glow")].initial &&
          processor.value("textColorCycle") == 0,
      "Reset restores FX, reactivity, IN, INT, GLOW and color cycling");
  intervalDialog("colorInterval");
  auto *intervalSlider =
      dynamic_cast<juce::Slider *>(panelWidgets[0]->getChildComponent(0));
  if (intervalSlider)
    intervalSlider->setValue(2, juce::sendNotificationSync);
  check(intervalSlider && processor.value("colorInterval") == 2 &&
            intervalSlider->getTextFromValue(2) == "3",
        "Custom cycle accepts three beats through the CC interval parameter");
  snapshot("cycle-settings");
  closePanel();
  intervalDialog("lyricInterval", true);
  snapshot("lyric-cycle-settings");
  closePanel();
  isHome = true;
  processor.hostBpm = 120;
  lastHostBpm = 120;
  auto pulse = [&] {
    const auto now = juce::Time::getMillisecondCounterHiRes() * .001;
    hostClockTime = now - .51;
    lastTick = now - .01;
    timerCallback();
  };
  processor.set("colorInterval", 2);
  processor.set("colorCycle", 0);
  processor.set("presetCycle", 0);
  for (int i = 0; i < 7; ++i)
    pulse();
  processor.set("color", 20);
  processor.set("colorCycle", 127);
  pulse();
  pulse();
  check(processor.value("color") == 20,
        "Color cycle waits the full interval after enabling");
  pulse();
  check(processor.value("color") == 31,
        "Color cycle advances on its third beat");
  processor.set("colorCycle", 0);
  processor.set("category", 0);
  processor.set("variant", 0);
  processor.setAssetProperty("presetFile", "");
  processor.setAssetProperty("presetName", "");
  processor.set("presetCycle", 127);
  processor.set("presetInterval", 0);
  pulse();
  pulse();
  check(processor.value("presetCycle") == 127 &&
            decodeChoice(processor.value("variant"), 16) == 2,
        "Preset cycling remains enabled across successive default recalls");
  processor.set("presetCycle", 0);
  processor.set("category", encodeChoice(7, 9));
  processor.setAssetProperty("lyrics", "[\"ONE\",\"TWO\",\"THREE\"]");
  rebuildLyrics();
  processor.set("lyric", 0);
  processor.set("lyricCycle", 127);
  processor.set("lyricTimeMode", 127);
  processor.set("lyricInterval", 1);
  for (int i = 0; i < 2; ++i) {
    lastTick = juce::Time::getMillisecondCounterHiRes() * .001 - 1.1;
    timerCallback();
  }
  check(decodeChoice(processor.value("lyric"), 3) == 1,
        "Seconds-based lyrics retain elapsed time across slow preview frames");
  processor.hostBpm = 0;
  lastHostBpm = 0;
  processor.set("lyricCycle", 0);
  processor.set("category",encodeChoice(7,9));sync();isHome=false;resized();
  bool textHit=true;
  for(int x:{4,textFx.getWidth()/2,textFx.getWidth()-5})for(int y:{3,textFx.getHeight()/2,textFx.getHeight()-4}) {
    const auto point=textFx.getPosition()+juce::Point<int>{x,y};
    textHit=textHit && uiHost().getComponentAt(point)==&textFx;
  }
  check(textHit,"Text motion button receives clicks across its entire face, unobstructed by status or Cycle");
  isHome=true;
  // Exercise the real audio analysis -> timer -> cycle path. The UI cadence
  // deliberately includes 250 ms stalls; no artificial BPM/beat injection.
  auto cycleAudio=std::make_unique<AudioAnalysis>();cycleAudio->setSampleRate(48000);
  processor.set("category",encodeChoice(7,9));processor.set("variant",0);
  processor.setAssetProperty("lyrics","[\"ONE\",\"TWO\",\"THREE\"]");rebuildLyrics();
  processor.set("lyric",0);processor.set("lyricTimeMode",0);
  for(auto id:{"colorCycle","color2Cycle","textColorCycle","lyricCycle"})processor.set(id,0);
  advanceCycles(0,0);
  for(auto id:{"colorCycle","color2Cycle","textColorCycle","lyricCycle"})processor.set(id,127);
  for(auto id:{"colorInterval","color2Interval","textColorInterval","lyricInterval"})processor.set(id,2);
  processor.set("color",20);processor.set("color2",40);processor.set("textColor",60);
  processor.set("presetCycle",0);advanceCycles(0,0);
  std::array<float,12000> beatAudio{};
  auto kick=[&] {
    for(int i=0;i<(int)beatAudio.size();++i)beatAudio[(size_t)i]=i<5760?
      .005f*std::exp(-i/1200.0f)*std::sin(i*6.283185307179586f*65/48000):0;
    cycleAudio->push(beatAudio.data(),nullptr,(int)beatAudio.size());advanceCycles(cycleAudio->consume(.25f).beats,.25);
    beatAudio.fill(0);cycleAudio->push(beatAudio.data(),nullptr,(int)beatAudio.size());advanceCycles(cycleAudio->consume(.25f).beats,.25);
  };
  kick();kick();
  check(processor.value("color")==20 && processor.value("color2")==40 &&
    processor.value("textColor")==60 && processor.value("lyric")==0,
    "All audio-driven cycles wait for three actual quiet kick beats");
  kick();
  check(processor.value("color")==31 && processor.value("color2")==51 &&
    processor.value("textColor")==71 && decodeChoice(processor.value("lyric"),3)==1,
    "Primary, secondary, text color and lyric cycles advance on the same third audio beat");
  processor.set("presetCycle",127);processor.set("presetInterval",1);advanceCycles(0,0);
  const auto beforeAutoPreset=processor.getAssetProperty("presetFile");kick();kick();
  check(processor.getAssetProperty("presetFile")!=beforeAutoPreset,
    "Preset cycle recalls the next native default after two actual audio beats");
  processor.set("presetCycle",0);
  check(processor.value("colorCycle")==127 && processor.value("color2Cycle")==127 &&
    processor.value("textColorCycle")==127 && processor.value("lyricCycle")==127 &&
    processor.value("colorInterval")==2 && processor.value("lyricInterval")==2,
    "Automatic preset recalls retain enabled cycles and beat intervals");
  for(auto id:{"colorCycle","color2Cycle","textColorCycle","lyricCycle"})processor.set(id,0);
  stopTimer();
  isHome = false;
  resized();
  recordDialog();
  snapshot("record-settings");
  check(panelButton(u8"CHOOSE…") && panelButton("RECORD"),
        "Recording dialog has folder selection and recording controls");
  closePanel();
  // Exercise the same mouse handlers used by every visible Lab control,
  // including fine adjustment, endpoint clamping, MIDI interception and menus.
  auto mouse = [](juce::Component &component, float y, int modifiers) {
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),
                            {20, y}, juce::ModifierKeys(modifiers), 1, 0, 0, 0,
                            0, &component, &component, now, {20, 40}, now, 1,
                            false);
  };
  juce::StringArray audited;
  processor.learning = false;
  processor.setAssetProperty("imagePath", logoFile.getFullPathName());
  refreshAssets();
  for (int mode = 0; mode < 9; ++mode) {
    processor.set("category", encodeChoice(mode, 9));
    sync();
    resized();
    for (auto &control : knobs) {
      if (!control->isVisible() || audited.contains(control->id))
        continue;
      audited.add(control->id);
      const auto id = control->id;
      processor.set(id.toRawUTF8(), 64);
      processor.learning = true;
      control->mouseDown(
          mouse(*control, 40, juce::ModifierKeys::leftButtonModifier));
      check(selectedControl == id && processor.value(id.toRawUTF8()) == 64,
            "MIDI selection does not change control / " + id);
      processor.learning = false;
      if (control->button) {
        processor.set(id.toRawUTF8(), 0);
        control->mouseDown(
            mouse(*control, 40, juce::ModifierKeys::leftButtonModifier));
        const bool on = processor.value(id.toRawUTF8()) == 127;
        control->mouseDown(
            mouse(*control, 40, juce::ModifierKeys::leftButtonModifier));
        check(on && processor.value(id.toRawUTF8()) == 0,
              "Toggle both directions / " + id);
      } else if (control->swatch) {
        control->mouseDown(
            mouse(*control, 40, juce::ModifierKeys::leftButtonModifier));
        const bool menu = juce::PopupMenu::dismissAllActiveMenus();
        control->mouseDrag(
            mouse(*control, -100, juce::ModifierKeys::leftButtonModifier));
        check(menu && processor.value(id.toRawUTF8()) == 64,
              "Color swatch opens palette without accidental drag changes / " +
                  id);
      } else {
        control->mouseDown(
            mouse(*control, 40, juce::ModifierKeys::leftButtonModifier));
        control->mouseDrag(
            mouse(*control, -300, juce::ModifierKeys::leftButtonModifier));
        const bool maximum = processor.value(id.toRawUTF8()) == 127;
        control->mouseDrag(
            mouse(*control, 300, juce::ModifierKeys::leftButtonModifier));
        const bool minimum = processor.value(id.toRawUTF8()) == 0;
        processor.set(id.toRawUTF8(), 64);
        control->mouseDown(
            mouse(*control, 40, juce::ModifierKeys::leftButtonModifier));
        control->mouseDrag(mouse(*control, 35,
                                 juce::ModifierKeys::leftButtonModifier |
                                     juce::ModifierKeys::shiftModifier));
        check(maximum && minimum && processor.value(id.toRawUTF8()) == 65,
              "Knob range and Shift fine adjustment / " + id);
      }
      if (id == "texture" || id == "dither" || id == "track" || id == "glass" ||
          id.endsWith("Cycle")) {
        selectedControl = "sentinel";
        control->mouseDown(
            mouse(*control, 40, juce::ModifierKeys::rightButtonModifier));
        check(juce::PopupMenu::dismissAllActiveMenus() &&
                  selectedControl == "sentinel",
              "Right-click settings open without MIDI selection / " + id);
      }
    }
  }
  check(audited.size() == 21,
        "All 21 contextual Lab knobs / swatches / toggles audited");
  const char *actions[] = {"nextPreset",  "previousPreset", "resetFx",
                           "savePreset",  "recordVideo",    "hideUI",
                           "fullscreen",  "home",           "addLyric",
                           "clearLyrics", "clearImage"};
  ToolButton *buttons[] = {&next,     &previous,    &resetButton, &save,
                           &record,   &hide,        &full,        &home,
                           &addLyric, &clearLyrics, &clearImage};
  processor.learning = true;
  for (int i = 0; i < 11; ++i) {
    selectedControl.clear();
    click(*buttons[i]);
    check(selectedControl == actions[i] && !panel,
          "Toolbar / lyric action selects MIDI target / " +
              juce::String(actions[i]));
  }
  processor.learning = false;
  click(midi);
  const bool learningOn = processor.learning.load();
  click(midi);
  check(learningOn && !processor.learning, "MIDI button enters / exits Learn");
  processor.set("lock", 0);
  click(lockButton);
  const bool locked = processor.value("lock") == 127;
  click(lockButton);
  check(locked && processor.value("lock") == 0,
        "Lock button toggles both directions");
  processor.set("lyricCycle", 0);
  click(lyricCycle);
  const bool cycleOn = processor.value("lyricCycle") == 127;
  click(lyricCycle);
  check(cycleOn && processor.value("lyricCycle") == 0,
        "Lyric Cycle toggles both directions");
  click(category);
  check(juce::PopupMenu::dismissAllActiveMenus(),
        "Mode / preset browser opens");
  click(textFx);
  check(juce::PopupMenu::dismissAllActiveMenus(), "Text effect selector opens");
  lyricCycle.context();
  check(juce::PopupMenu::dismissAllActiveMenus(), "Lyric timing menu opens");
  for (int mode = 0; mode < 9; ++mode) {
    processor.set("category", encodeChoice(mode, 9));
    processor.setAssetProperty("presetFile", "");
    processor.setAssetProperty("presetName", "");
    processor.set("variant", 0);
    click(next);
    const auto selected = processor.getAssetProperty("presetFile");
    click(previous);
    const bool returned = decodeChoice(processor.value("variant"), 16) == 0;
    click(preset);
    check(returned && selected == processor.getAssetProperty("presetFile"),
          "Next / Previous / preset scroller / " + categories[mode]);
  }
  processor.set("category", encodeChoice(7, 9));
  sync();
  click(hide);
  const bool hidden = uiHidden && !category.isVisible() && !hide.isVisible() && !popOut.isVisible();
  keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
  check(hidden && !uiHidden && category.isVisible(),
        "Hide UI and Escape restore");
  auto mouseEvent = [&](juce::Component *target,float y,bool dragged=false) {
    const auto now=juce::Time::getCurrentTime();
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),{10,y},
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,target,target,now,{10,40},now,1,dragged);
  };
  check(captureOverlay && processor.getAssetProperty("controlsPresentation")=="overlay",
        "Capture control button defaults to the in-place overlay for existing profiles");
  click(popOut);
  check(controlsWindow && captureOverlay && !controlsWindow->isUsingNativeTitleBar() &&
        controlsWindow->getBounds()==getScreenBounds(),"Lab capture controls align with the output and have no separate window frame");
  checkCaptureOverlay("Lab");
  const bool moved=controlsWindow && &uiHost()!=this && preset.getParentComponent()==&uiHost();
  CCControl *intensity=nullptr;
  for(auto &control:knobs)if(control->id=="int")intensity=control.get();
  const int oldIntensity=processor.value("int");
  processor.set("int",64);
  if(intensity) {intensity->mouseDown(mouseEvent(intensity,40));intensity->mouseDrag(mouseEvent(intensity,30,true));}
  check(moved && intensity && processor.value("int")==74,
        "Capture controls reparent existing widgets and update the same processor");
  processor.set("int",oldIntensity);
  click(hide);
  const bool allHidden=uiHidden && !controlsWindow->isVisible() && !hide.isVisible();
  mouseDown(mouseEvent(this,200));
  check(allHidden && !uiHidden && controlsWindow->isVisible(),"Click on visuals restores the entire control layer");
  click(popOut);
  check(!controlsWindow && &uiHost()==this && preset.getParentComponent()==this,
        "Docking restores existing controls to the original editor");
  click(full);
  auto *window=dynamic_cast<juce::DocumentWindow *>(getTopLevelComponent());
  if(!window)window=outputWindow.get();
  const bool borderless=windowMode==1 && window && !window->isUsingNativeTitleBar() &&
      !window->isFullScreen() && full.iconColour==juce::Colour(0xff00dd91);
  click(full);
  const bool fullscreen=windowMode==2 && window && window->isFullScreen() && full.iconColour==juce::Colour(0xffff4d5a);
  click(full);
  check(borderless && fullscreen && windowMode==0 && full.iconColour==juce::Colour(0xff858589) &&
        (outputWindow==nullptr) && (!dynamic_cast<juce::DocumentWindow *>(getTopLevelComponent()) ||
          !dynamic_cast<juce::DocumentWindow *>(getTopLevelComponent())->isFullScreen()),
        "Windowed / borderless green / fullscreen red / windowed grey cycle");
  click(full);
  keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
  check(windowMode==0 && full.iconColour==juce::Colour(0xff858589),"Escape restores windowed mode and grey icon");
  click(record);
  auto *cancelRecord = panelButton("CANCEL");
  if (cancelRecord)
    click(*cancelRecord);
  check(cancelRecord && !panel && !recorder.active(),
        "Record button opens dialog; Cancel preserves idle state");
  click(save);
  auto *cancelSave = panelButton("CANCEL");
  if (cancelSave)
    click(*cancelSave);
  check(cancelSave && !panel, "Save button opens dialog; Cancel closes it");
  const auto uiEncoder = juce::File(
      juce::SystemStats::getEnvironmentVariable("GRID_TEST_FFMPEG", ""));
  if (uiEncoder.existsAsFile()) {
    processor.setAssetProperty("encoderPath", uiEncoder.getFullPathName());
    click(record);
    std::vector<juce::TextEditor *> fields;
    std::vector<juce::ComboBox *> choices;
    for (auto &w : panelWidgets) {
      if (auto *field = dynamic_cast<juce::TextEditor *>(w.get()))
        fields.push_back(field);
      if (auto *choice = dynamic_cast<juce::ComboBox *>(w.get()))
        choices.push_back(choice);
    }
    auto *go = panelButton("RECORD");
    if (fields.size() == 4 && choices.size() == 4 && go) {
      fields[0]->setText("../invalid");
      click(*go);
      check(panel && !recorder.active(),
            "Record dialog rejects an invalid filename");
      const auto recordingName =
          "UI-recording-" + juce::String(juce::Time::getMillisecondCounter());
      fields[0]->setText(recordingName);
      fields[1]->setText("relative-folder");
      click(*go);
      check(panel && !recorder.active(),
            "Record dialog rejects a relative folder");
      fields[1]->setText(folder.getFullPathName());
      fields[2]->setText("1");
      click(*go);
      check(panel && !recorder.active(),
            "Record dialog rejects bitrate below its limit");
      fields[2]->setText("800");
      fields[3]->setText("0");
      click(*go);
      check(panel && !recorder.active(), "Record dialog rejects zero duration");
      fields[3]->setText("1");
      choices[0]->setSelectedId(1, juce::dontSendNotification);
      choices[1]->setSelectedId(1, juce::dontSendNotification);
      click(*go);
      timerCallback();
      check(!panel && recorder.active() && record.getToggleState() &&
                !isResizable(),
            "Record dialog starts capture, turns on indicator and locks "
            "resizing");
      // This synchronous component audit pauses the native event loop. Supply a
      // frame fixture; --record-check separately exercises real GPU capture.
      juce::Image frame(juce::Image::ARGB, 640, 360, true,
                        juce::SoftwareImageType());
      {
        juce::Graphics g(frame);
        g.fillAll(juce::Colours::blue);
      }
      recorder.submit(frame);
      const auto deadline = juce::Time::getMillisecondCounterHiRes() + 5000;
      while (recorder.active() &&
             juce::Time::getMillisecondCounterHiRes() < deadline) {
        timerCallback();
        juce::Thread::sleep(10);
      }
      timerCallback();
      check(!recorder.active() && recorder.error().isEmpty() &&
                recorder.encoded == 24 &&
                folder.getChildFile(recordingName + ".mp4").getSize() > 1024 &&
                isResizable(),
            "UI recording finalizes the selected resolution / FPS and restores "
            "resizing");
      stopTimer();
      click(record);
      fields.clear();
      for (auto &w : panelWidgets)
        if (auto *field = dynamic_cast<juce::TextEditor *>(w.get()))
          fields.push_back(field);
      if (fields.size() == 4) {
        fields[0]->setText(recordingName + "-stop");
        fields[1]->setText(folder.getFullPathName());
        fields[3]->setText("10");
        click(*panelButton("RECORD"));
        recorder.submit(frame);
        juce::Thread::sleep(100);
        click(record);
        const auto end = juce::Time::getMillisecondCounterHiRes() + 5000;
        while (recorder.active() &&
               juce::Time::getMillisecondCounterHiRes() < end)
          juce::Thread::sleep(10);
        timerCallback();
        check(!recorder.active() && !panel && recorder.error().isEmpty() &&
                  recorder.encoded > 0 && recorder.encoded < 300 &&
                  isResizable(),
              "Recording dot stops and finalizes an active recording");
        stopTimer();
      }
    } else
      check(false, "All recording dialog configuration controls exist");
  }
  click(home);
  check(isHome && lab.isVisible() && !processor.learning,
        "Menu returns to home");
  click(settings);
  auto widget = [&](const juce::String &id) -> juce::Component * {
    for (auto &w : panelWidgets)
      if (w->getComponentID() == id)
        return w.get();
    return nullptr;
  };
  auto *tier = dynamic_cast<juce::ComboBox *>(widget("graphics-tier"));
  check(panel && back.getParentComponent() == panel.get() &&
            !back.isCurrentlyBlockedByAnotherModalComponent() &&
            !panelButton("DONE") && !panelButton("APPLY GRAPHICS"),
        "Settings Back receives input inside modal panel; no Done or Apply "
        "button");
  bool allTiers = tier != nullptr;
  for (int q = 0; q < 7 && tier; ++q) {
    tier->setSelectedId(q + 1, juce::sendNotificationSync);
    const auto &t = graphicsTiers[(size_t)q];
    allTiers &= processor.value("frameCap") == frameRateValue(t.fps) &&
                processor.value("renderResolution") == encodeChoice(q,7) &&
                currentGraphicsTier() == q;
    // Lab rendering must receive the settings, rather than just displaying
    // them.
    isHome = false;
    timerCallback();
    const auto scene =
#if GRID_USE_VEL_ENGINE
        nativeEngine->requestedScene();
#else
        canvas.lastPresentedScene();
#endif
    allTiers &= scene.width == renderResolutions[(size_t)q].width &&
                scene.height == renderResolutions[(size_t)q].height &&
                scene.detail == t.detail &&
                fps == frameRate(processor.value("frameCap"));
    isHome = true;
  }
  stopTimer();
  check(allTiers, "Every graphics tier immediately changes actual render size, "
                  "detail and frame cap");
  check(!widget("frameCap") && !widget("renderScale") && !widget("audio-source"),
        "General Settings has no frame-cap, render-scale or audio-source widgets");
  auto *graphics=panelButton("ADVANCED GRAPHICS"),*audioMenuButton=panelButton("AUDIO / MIDI DEVICES");
  check(graphics && audioMenuButton && graphics->getWidth()==audioMenuButton->getWidth() &&
        graphics->getX()+audioMenuButton->getRight()==panel->getWidth(),
        "Settings submenu columns have equal widths and share the frame center");
  processor.set("texture", 100);
  processor.set("glass", 100);
  const auto graphicsBefore = processor.value("frameCap");
  click(*panelButton("RESET CONTROLS"));
  check(processor.value("texture") == 0 && processor.value("glass") == 0 &&
            processor.value("frameCap") == graphicsBefore,
        "Settings Reset restores visual controls and keeps graphics");
  processor.bind(1, 11, {indexOf("texture"), -1, Hardware::knob});
  click(*panelButton("CLEAR LAB MIDI BINDINGS"));
  check(processor.bindingState().getArray()->isEmpty(),
        "Clear MIDI Bindings removes saved mappings");
  showMessage("");
  snapshot("settings");
  click(*panelButton("ADVANCED GRAPHICS"));
  check(settingsScreen == SettingsPage::graphics &&
            !back.isCurrentlyBlockedByAnotherModalComponent() &&
            !panelButton("DONE"),
        "Advanced graphics opens with working Back and no Done");
  for (const char *id :
       {"frameCap", "renderDetail", "bloomQuality",
        "trackingRate", "trackingResolution"}) {
    auto *control = dynamic_cast<juce::Slider *>(widget(id));
#if GRID_USE_VEL_ENGINE
    if (juce::String(id) == "renderDetail" || juce::String(id) == "bloomQuality" ||
        juce::String(id) == "trackingRate" || juce::String(id) == "trackingResolution") {
      check(!control,
            "Unavailable native setting is absent / " + juce::String(id));
      continue;
    }
#endif
    bool changed = false;
    if (control) {
      control->setValue(control->getMinimum(), juce::sendNotificationSync);
      const int minimum = processor.value(id);
      control->setValue(control->getMaximum(), juce::sendNotificationSync);
      changed = processor.value(id) != minimum;
    }
    check(changed,
          "Advanced slider changes renderer parameter / " + juce::String(id));
  }
  for (const char *id : {"antialias", "vsync", "pauseMenuRendering", "stats"}) {
    auto *toggle = dynamic_cast<juce::ToggleButton *>(widget(id));
    bool changed = false;
    if (toggle) {
      toggle->setToggleState(false, juce::sendNotificationSync);
      const bool off = processor.value(id) == 0;
      toggle->setToggleState(true, juce::sendNotificationSync);
      changed = off && processor.value(id) == 127;
    }
    check(changed,
          "Advanced toggle changes renderer parameter / " + juce::String(id));
  }
  auto *resolution=dynamic_cast<juce::ComboBox *>(widget("renderResolution"));
  bool fixedResolution=resolution!=nullptr;
  const auto windowBounds=getBounds();
  processor.set("adaptive",127);processor.set("renderScale",0);
  for(int option=0;resolution && option<(int)renderResolutions.size();++option) {
    resolution->setSelectedId(option+1,juce::sendNotificationSync);
    isHome=false;timerCallback();isHome=true;
    const auto scene=
#if GRID_USE_VEL_ENGINE
      nativeEngine->requestedScene();
#else
      canvas.lastPresentedScene();
#endif
    fixedResolution &= scene.width==renderResolutions[(size_t)option].width &&
      scene.height==renderResolutions[(size_t)option].height && getBounds()==windowBounds;
  }
  check(fixedResolution,"480p–4K sets scene dimensions before rendering, independently of window size and legacy scale/adaptive state");
  snapshot("advanced-graphics");
  click(back);
  check(settingsScreen == SettingsPage::general && panel,
        "Advanced Back returns to Settings");
  click(*panelButton("AUDIO / MIDI DEVICES"));
  check(settingsScreen == SettingsPage::audio && panel &&
            !back.isCurrentlyBlockedByAnotherModalComponent(),
        "Audio / MIDI devices opens inside Settings navigation");
  if (audioSettings && !panelWidgets.empty()) {
    auto *devices = panelWidgets.front().get();
    auto *mute = dynamic_cast<juce::ToggleButton *>(
        devices->findChildWithID("muteInput"));
    if (mute) {
      mute->setToggleState(true, juce::dontSendNotification);
      mute->onClick();
      const bool muted = processor.value("muteInput") == 127;
      mute->setToggleState(false, juce::dontSendNotification);
      mute->onClick();
      check(muted && processor.value("muteInput") == 0,
            "Audio analysis mute toggles both directions");
    }
    check(devices->getNumChildComponents() >= 2,
          "Native audio / MIDI device selector is mounted");
    auto *captureSource = dynamic_cast<juce::ComboBox *>(devices->findChildWithID("capture-source"));
    auto *speaker = dynamic_cast<juce::ComboBox *>(devices->findChildWithID("speaker-device"));
#if JUCE_WINDOWS
    check(captureSource && speaker, "Audio page exposes microphone and named speaker selectors");
    if (captureSource && speaker) {
      captureSource->setSelectedId(2, juce::sendNotificationSync);
      check(processor.getAssetProperty("audioSource") == "system" && speaker->isEnabled(),
            "Audio page switches directly from microphone to speaker capture");
      if (speaker->getNumItems() > 1) {
        speaker->setSelectedId(2, juce::sendNotificationSync);
        check(processor.getAssetProperty("speakerDevice").isNotEmpty() &&
                  processor.getAssetProperty("audioSource") == "system",
              "Named speaker selection opens a real playback endpoint");
      }
      speaker->setSelectedId(1, juce::sendNotificationSync);
      check(processor.getAssetProperty("speakerDevice").isEmpty(),
            "Speaker selector returns to the Windows default endpoint");
      captureSource->setSelectedId(1, juce::sendNotificationSync);
      check(processor.getAssetProperty("audioSource") == "hardware" && !speaker->isEnabled(),
            "Audio page switches speaker capture back to microphone input");
    }
#endif
    snapshot("audio-speaker-selector");
  }
  snapshot("audio-midi-devices");
  keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
  check(settingsScreen == SettingsPage::general && panel,
        "Audio Escape returns to Settings");
  // File actions must at least open and cancel their actual native chooser.
  for (const auto *name : {"SAVE COMPLETE STATE", "LOAD COMPLETE STATE",
                           "CHOOSE FFMPEG ENCODER"}) {
    if(quietDesktop){report+="SKIP Native desktop dialog while another monitor is in use / "+juce::String(name)+"\n";continue;}
    auto *button = panelButton(name);
    if (button)
      click(*button);
    check(button && chooser != nullptr,
          "Native file chooser opens / " + juce::String(name));
    chooser.reset();
  }
  const auto stateFile = folder.getChildFile("UI-state.gridstate");
  processor.setAssetProperty("styleFontRoot", "Local style font folder");
  processor.set("texture", 78);
  processor.bind(1, 22, {indexOf("texture"), -1, Hardware::knob});
  check(writeStateFile(stateFile).wasOk() && stateFile.existsAsFile(),
        "Save Complete State writes an actual state file");
  processor.setAssetProperty("styleFontRoot", "");
  processor.clearBindings();
  processor.set("texture", 0);
  const auto loaded = readStateFile(stateFile);
  check(loaded.wasOk() && processor.value("texture") == 78 &&
            processor.getAssetProperty("styleFontRoot") ==
                "Local style font folder" &&
            processor.bindingState().getArray()->size() == 1,
        "Load Complete State restores controls, local font folder and MIDI "
        "mappings");
  const auto invalidState = folder.getChildFile("invalid.gridstate");
  invalidState.replaceWithText("Invalid state");
  check(readStateFile(invalidState).failed() &&
            processor.value("texture") == 78,
        "Invalid complete-state file preserves current controls");
  const auto encoder = juce::File(
      juce::SystemStats::getEnvironmentVariable("GRID_TEST_FFMPEG", ""));
  if (encoder.existsAsFile()) {
    selectEncoderFile(encoder);
    check(findEncoder(processor.getAssetProperty("encoderPath")) == encoder,
          "Chosen encoder reaches the recorder's actual executable lookup");
  }
  if (auto *open = quietDesktop?nullptr:panelButton("OPEN PRESET FOLDERS")) {
    click(*open);
    check(presets.root.isDirectory() &&
              status.getText() == "Preset folders opened.",
          "Open Preset Folders launches the actual folder tree");
  }
  click(back);
  check(!panel && isHome && lab.isVisible() && !back.isVisible(),
        "Settings Back returns to home");
  // Visit the real nested Mapping / Bindings widgets and measure transitions.
#if GRID_USE_VEL_ENGINE
  selectWorkspace(Workspace::performance);
  check(category.isVisible() && home.isVisible() && midi.isVisible() && midi.getButtonText()=="MAPPING" && workspaceAction.isVisible() &&
        !record.isVisible() && !full.isVisible() && !popOut.isVisible() && !hide.isVisible(),
        "Performance setup has one Menu / Mapping / centered title / Play toolbar without Recording");
  check(workspaceAction.getRight()==uiHost().getWidth()-18,
        "Performance Play is anchored at the far right without moving its centered title");
  // Exercise the actual toolbar callbacks with the playback presentation active.
  performanceLive=true;updatePresentation();resized();
  const auto performanceBindings=juce::JSON::toString(processor.bindingState());
  check(home.isVisible() && midi.getButtonText()=="MAPPING" && midi.isVisible() &&
        !record.isVisible() && full.isVisible() && popOut.isVisible() && hide.isVisible() &&
        full.getRight()<popOut.getX() && popOut.getRight()<hide.getX(),
        "Performance playback has Fullscreen / Pop-out / Hide UI and no recording dot");
  for(int detached=0;detached<2;++detached) {
    if(detached){click(popOut);checkCaptureOverlay("Performance");}
    check(!detached || (controlsWindow && captureOverlay && !controlsWindow->isUsingNativeTitleBar() &&
          controlsWindow->getBounds()==getScreenBounds() && popOut.getParentComponent()==&uiHost()),
          "Performance capture overlay stays aligned and reuses the live toolbar");
    click(midi);
    auto *graph=panel?panel->findChildWithID("performance-keyboard-graph"):nullptr;
    auto *bindingTable=panel?panel->findChildWithID("performance-bindings-scroll"):nullptr;
    auto *bindingButton=panel?dynamic_cast<juce::TextButton *>(panel->findChildWithID("performance-bindings")):nullptr;
    auto *mappingBack=panel?dynamic_cast<juce::TextButton *>(panel->findChildWithID("performance-mapping-back")):nullptr;
    check(graph && graph->isVisible() && bindingTable && !bindingTable->isVisible() && bindingButton && bindingButton->isVisible(),
          "Playback MAPPING opens the keyboard graph before Bindings, docked or popped out");
    if(bindingButton)click(*bindingButton);
    check(graph && !graph->isVisible() && bindingTable && bindingTable->isVisible(),
          "Bindings button alone opens the separate binding table");
    if(mappingBack)click(*mappingBack);
    check(graph && graph->isVisible() && bindingTable && !bindingTable->isVisible(),
          "Bindings Back returns to the visual mapping graph");
    closePanel();
  }
  check(processor.workspace()==Workspace::performance && juce::JSON::toString(processor.bindingState())==performanceBindings,
        "Performance pop-out and Mapping preserve the active workspace and MIDI bindings");
  // The native engine is on its setup screen: its next tick must dock controls.
  timerCallback();stopTimer();
  check(!performanceLive && !controlsWindow && category.isVisible() && midi.isVisible() && workspaceAction.isVisible() &&
        !popOut.isVisible() && !record.isVisible(),
        "Leaving playback docks Performance controls and restores its setup header");
  click(home);
  for(int pass=0;pass<3;++pass) {
    selectWorkspace(Workspace::multiple);check(!isHome && processor.workspace()==Workspace::multiple,"Multiple enters from Home");
    if(pass==0) {
      check(save.isVisible() && category.getRight()<save.getX() && save.getRight()<midiTile.getX() &&
        workspaceAction.isVisible() && workspaceAction.getButtonText()=="DONE",
        "Multiple toolbar has Done and native Save between centered Multiple title and tile selector");
      check(preset.isVisible() && previous.isVisible() && next.isVisible() && preset.getButtonText()=="PRESET" &&
            preset.getY()==96 && std::abs(preset.getBounds().getCentreX()*2-uiHost().getWidth())<=1,
            "Multiple restores its centered saved-layout preset selector and navigation row");
      bool aligned=true;
      const auto oldSize=getBounds();
      for(int width:{960,1120,quietDesktop?quietMaximumWidth:2560}) {
        setSize(width,740);
        aligned=aligned && workspaceAction.getRight()==uiHost().getWidth()-18 && hide.getRight()<workspaceAction.getX() &&
          category.getBounds().getCentreX()==uiHost().getWidth()/2 && preset.getBounds().getCentreX()==uiHost().getWidth()/2 &&
          category.getRight()<save.getX() && save.getRight()<midiTile.getX() && midiTile.getRight()<full.getX() &&
          full.getRight()<popOut.getX() && popOut.getRight()<hide.getX();
      }
      setBounds(oldSize);check(aligned,"Multiple Done remains far right with no toolbar collisions at supported widths");
      snapshot("multiple-toolbar");
      const auto engine=processor.velEngine.load();
      const auto bindingsBefore=juce::JSON::toString(processor.bindingState());
      click(popOut);
      auto *display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());
      check(controlsWindow && captureOverlay && !controlsWindow->isUsingNativeTitleBar() &&
            controlsWindow->getBounds()==getScreenBounds() &&
            midiTile.getParentComponent()==&uiHost() && processor.velEngine.load()==engine &&
            (!display || display->userArea.contains(controlsWindow->getBounds().getCentre())),
            "Multiple capture controls overlay the same output window and keep the engine instance");
      checkCaptureOverlay("Multiple");
      click(midi);const bool learning=processor.learning.load();click(midi);
      check(learning && !processor.learning && processor.workspace()==Workspace::multiple &&
            juce::JSON::toString(processor.bindingState())==bindingsBefore,
            "Multiple detached MIDI toggle uses the existing workspace and preserves its bindings");
      click(popOut);
      check(!controlsWindow && midiTile.getParentComponent()==this && canvas.presentationTarget()==nullptr,
            "Multiple docks its existing controls and detaches the host scene layer");
    }
    click(home);check(isHome,"Multiple returns to GRID Home");
    selectWorkspace(Workspace::performance);
    const auto start=juce::Time::getMillisecondCounterHiRes();performanceMapping();
    if(pass==0)snapshot("performance-mapping");
    report+="TIMING Mapping open ms: "+juce::String(juce::Time::getMillisecondCounterHiRes()-start,2)+"\n";
    auto *bindings=panel?dynamic_cast<juce::TextButton *>(panel->findChildWithID("performance-bindings")):nullptr;
    auto *mappingBack=panel?dynamic_cast<juce::TextButton *>(panel->findChildWithID("performance-mapping-back")):nullptr;
    const auto begin=juce::Time::getMillisecondCounterHiRes();if(bindings)click(*bindings);
    const auto elapsed=juce::Time::getMillisecondCounterHiRes()-begin;
    if(pass==0)snapshot("performance-bindings");
    report+="TIMING Bindings open ms: "+juce::String(elapsed,2)+"\n";
    auto *scroll=panel?dynamic_cast<juce::Viewport *>(panel->findChildWithID("performance-bindings-scroll")):nullptr;
    check(bindings && mappingBack && scroll && scroll->isVisible() && elapsed<100,
      "Mapping opens separate Bindings frame without a 100ms main-thread stall");
    if(scroll){scroll->setViewPosition(0,100000);check(scroll->getViewPositionY()>0,"Bindings scrolls to final actions");}
    if(mappingBack)click(*mappingBack);
    check(bindings && bindings->isVisible() && scroll && !scroll->isVisible(),"Bindings Back restores original keyboard graph");
    if(mappingBack)click(*mappingBack);
    check(!panel,"Mapping Back closes its frame");click(home);check(isHome,"Performance returns to GRID Home");
  }
#endif
  applyGraphicsTier(2);
  processor.set("adaptive", 0);
  processor.set("stats", 0);
  stopTimer();
  folder.getChildFile("ui-checks.txt").replaceWithText(report);
  return report;
}
} // namespace kone::grid
