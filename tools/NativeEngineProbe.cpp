#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine/VelEngine.h"
#include "Core/Graphics.h"

// Development-only host inspection. No license data is exported; the
// inspection mode avoids editors/audio; render/panel checks use the private bridge
// with an isolated profile and an offscreen host, never the installed engine.
class EngineProbe final : public juce::JUCEApplication, private juce::Timer {
public:
  const juce::String getApplicationName() override { return "GRID Engine Probe"; }
  const juce::String getApplicationVersion() override { return "0.1.0"; }
  bool moreThanOneInstanceAllowed() override { return true; }

  void initialise(const juce::String &arguments) override {
    auto tokens = juce::StringArray::fromTokens(arguments, true);
    tokens.removeEmptyStrings();
    if (tokens.size() != 2 && !(tokens.size() == 3 && (tokens[2] == "--graphics" || tokens[2] == "--compact" || tokens[2] == "--profile" || tokens[2] == "--text" || tokens[2] == "--render" || tokens[2] == "--panels" || tokens[2] == "--play" || tokens[2]=="--graph" || tokens[2]=="--multiple" || tokens[2]=="--dialogs"))) {
      setApplicationReturnValue(2);
      quit();
      return;
    }
    pluginFile = juce::File(tokens[0].unquoted());
    reportFile = juce::File(tokens[1].unquoted());
    reportFile.getParentDirectory().createDirectory();
    stage("starting");
    const bool rendering=tokens.size()==3;
    graphicsCheck=rendering && tokens[2]=="--graphics";
    compactCheck=rendering && tokens[2]=="--compact";
    profileCheck=rendering && tokens[2]=="--profile";
    textCheck=rendering && tokens[2]=="--text";
    multipleLiveCheck=rendering && tokens[2]=="--multiple";
    playCheck=rendering && tokens[2]=="--play";
    dialogCheck=rendering && tokens[2]=="--dialogs";
    graphCheck=rendering && (tokens[2]=="--graph" || dialogCheck);
    panelCheck=rendering && (compactCheck || tokens[2]=="--panels" || playCheck || graphCheck || multipleLiveCheck);
    juce::MessageManager::callAsync([this,rendering] {
      if(rendering)startRender();else inspect();
    });
  }
  void shutdown() override { stopTimer(); engine.reset(); }

private:
  void startRender() {
    stage("opening private render bridge");
    engine = std::make_unique<kone::grid::VelEngine>();
    if (auto result = engine->open(pluginFile,reportFile.getParentDirectory().getChildFile("EngineProfile")); result.failed()) {
      auto object = new juce::DynamicObject();
      object->setProperty("error", result.getErrorMessage());
      finish(juce::var(object), 1);
      return;
    }
    kone::grid::SceneState settings;
    for (size_t i = 0; i < kone::grid::controls.size(); ++i)
      settings.values[i] = kone::grid::controls[i].initial;
    settings.width=profileCheck?1920:640;settings.height=profileCheck?1080:360;
    if(profileCheck){settings.values[(size_t)kone::grid::indexOf("texture")]=127;settings.values[(size_t)kone::grid::indexOf("textureType")]=127;}
    settings.values[(size_t)kone::grid::indexOf("frameCap")]=profileCheck?56:8;
    engine->update(settings);profileSettings=settings;
    if(graphicsCheck) {graphicsSettings=settings;beginGraphics();return;}
    if(textCheck) {textSettings=settings;beginText();return;}
    if(panelCheck) {
      engine->setHomeAction([this]{++panelHomeCalls;engine->showWorkspace(kone::grid::Workspace::lab,nullptr,{});});
      engine->setMappingAction([this]{++panelMappingCalls;});
      panelHost.setOpaque(true);
      panelHost.setBounds(-30000,-30000,960,620);
      panelHost.addToDesktop(juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresMouseClicks);
      panelHost.setVisible(true);
      if(multipleLiveCheck) {
        detachedHost.setBounds(-28000,-30000,1280,720);
        detachedHost.addToDesktop(juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresMouseClicks);
        detachedHost.setVisible(true);
      }
      enterPanel();return;
    }
    renderStart = juce::Time::getMillisecondCounterHiRes();
    stage("waiting for native frames");
    startTimerHz(100);
  }
  void enterPanel() {
    graphStage=0;
    panelStart=engine->panelFrames();renderFrameStart=engine->frames();
    renderStart=juce::Time::getMillisecondCounterHiRes();
    stage("painting "+juce::String(panelStep%2==0?"Multiple":"Performance")+" / pass "+juce::String(panelStep+1));
    const int width=compactCheck?(panelStep<2?1120:960):(playCheck || multipleLiveCheck)?(panelStep==0?960:1280):(panelStep<2?1120:960);
    const int height=compactCheck?(panelStep<2?658:740):(playCheck || multipleLiveCheck)?(panelStep==0?620:720):(panelStep<2?658:620);
    auto &host=multipleLiveCheck && panelStep==1?detachedHost:panelHost;
    host.setSize(width,height);
    engine->showWorkspace(!multipleLiveCheck && ((!dialogCheck && graphCheck) || playCheck || panelStep%2!=0)?kone::grid::Workspace::performance:kone::grid::Workspace::multiple,
        &host,{0,compactCheck?82:0,width,height-(compactCheck?82:0)},compactCheck);
    if(multipleLiveCheck) {engine->checkMultipleDone();engine->setFrameCapture(true);}
    if(playCheck) {
      engine->checkPerformancePlay();
      immediateViewport=engine->viewportState();
      engine->showWorkspace(kone::grid::Workspace::performance,nullptr,{});
      engine->setPerformanceHeld(true,panelStep==0,panelStep!=0);
    }
    startTimerHz(20);
  }
  void beginGraphics() {
    using namespace kone::grid;
    const auto resolution=renderResolutions[(size_t)(graphicsStep<7?graphicsStep:0)];
    graphicsSettings.width=resolution.width;graphicsSettings.height=resolution.height;
    graphicsSettings.values[(size_t)indexOf("category")]=encodeChoice(4,9);
    graphicsSettings.values[(size_t)indexOf("variant")]=encodeChoice(1,16);
    graphicsSettings.values[(size_t)indexOf("frameCap")]=frameRateValue(graphicsStep==7?12:graphicsStep==8?30:120);
    graphicsSettings.values[(size_t)indexOf("vsync")]=graphicsStep<7 && graphicsStep%2?127:0;
    graphicsSettings.values[(size_t)indexOf("antialias")]=graphicsStep%2?127:0;
    engine->update(graphicsSettings);
    renderFrameStart=engine->frames();renderStart=juce::Time::getMillisecondCounterHiRes();
    stage("graphics scenario "+juce::String(graphicsStep));startTimerHz(100);
  }
  void beginText() {
    using namespace kone::grid;
    auto set=[&](const char* id,int value){textSettings.values[(size_t)indexOf(id)]=value;};
    set("category",encodeChoice(7,9));set("variant",encodeChoice(textStep<12?textStep:0,16));
    set("color",84);set("textColor",127);set("glass",0);set("textEffect",0);set("speed",0);
    juce::StringArray lines{"GRID TEXT","SECOND LINE"};int active=textStep==12?1:0;
    if(textStep==13)lines.set(0,"EDITED LYRIC");
    if(textStep==14)set("textColor",42);
    if(textStep==15)set("color",20);
    if(textStep==18){set("glass",80);set("glassType",0);}
    if(textStep==19 || textStep==20) {
      set("category",encodeChoice(4,9));set("variant",encodeChoice(2,16));
      set("color",20);set("color2",textStep==19?50:90);textSettings.secondaryPalette=true;
    } else textSettings.secondaryPalette=false;
    if(textStep==21)set("category",0);
    engine->setLyrics(lines,active);engine->update(textSettings);
    if(textStep==16)textImportPassed=engine->importFile(juce::File("C:/Windows/Fonts/arial.ttf")).wasOk();
    if(textStep==17)engine->clearTextFont();
    renderFrameStart=engine->frames();renderStart=juce::Time::getMillisecondCounterHiRes();
    stage("Text/cycle scenario "+juce::String(textStep));startTimerHz(30);
  }
  void timerCallback() override {
    if(graphicsCheck) {
      using namespace kone::grid;
      const auto now=juce::Time::getMillisecondCounterHiRes();
      if(graphicsPauseStage==1) {
        if(now-renderStart<300)return;
        graphicsPassed=graphicsPassed && engine->frames()<=renderFrameStart+1;
        engine->setPaused(false);graphicsPauseStage=2;renderFrameStart=engine->frames();renderStart=now;return;
      }
      const bool ready=engine->frames()>=renderFrameStart+(graphicsStep<7?4:7);
      if(!ready && now-renderStart<8000)return;
      const auto frame=engine->latestFrame();const auto size=engine->renderingSize();
      auto row=new juce::DynamicObject();row->setProperty("scenario",graphicsStep);
      row->setProperty("width",size.first);row->setProperty("height",size.second);
      row->setProperty("timing",engine->frameTimings());row->setProperty("viewport",engine->viewportState());
      bool passed=ready && frame.getWidth()==graphicsSettings.width && frame.getHeight()==graphicsSettings.height &&
        size==std::pair<int,int>{graphicsSettings.width,graphicsSettings.height} && (bool)engine->viewportState()["fitsViewport"];
      if(graphicsStep>=7 && graphicsPauseStage==0) {
        const auto target=frameRate(graphicsSettings.get("frameCap"));
        passed=passed && engine->frameMilliseconds()>=1000.0/target-2;
        row->setProperty("frameCap",target);
      }
      row->setProperty("passed",passed);panelResults.add(juce::var(row));graphicsPassed=graphicsPassed && passed;
      if(++graphicsStep<10){beginGraphics();return;}
      if(graphicsPauseStage==0) {
        engine->setPaused(true);graphicsPauseStage=1;renderFrameStart=engine->frames();renderStart=now;return;
      }
      stopTimer();auto result=new juce::DynamicObject();result->setProperty("graphicsChecks",panelResults);
      result->setProperty("pauseAndResume",graphicsPassed && graphicsPauseStage==2 && ready);
      result->setProperty("passed",graphicsPassed && graphicsPauseStage==2 && ready);finish(juce::var(result),graphicsPassed?0:1);return;
    }
    if(textCheck) {
      using namespace kone::grid;
      engine->update(textSettings); // Match the app timer, including deferred font imports.
      if(engine->frames()<renderFrameStart+12 && juce::Time::getMillisecondCounterHiRes()-renderStart<6000)return;
      const auto frame=engine->latestFrame();auto result=new juce::DynamicObject();
      const auto state=engine->observedState();result->setProperty("scenario",textStep);result->setProperty("state",state);
      int changed=0,bright=0,difference=0;
      if(frame.isValid()) {
        auto bg=frame.getPixelAt(0,0);
        for(int y=0;y<frame.getHeight();++y)for(int x=0;x<frame.getWidth();++x) {
          auto pixel=frame.getPixelAt(x,y);if(pixel!=bg)++changed;
          auto isBright=[](juce::Colour c){return c.getRed()>180 && c.getGreen()>180 && c.getBlue()>180;};
          if(isBright(pixel))++bright;
          if(textBaseline.isValid() && isBright(pixel)!=isBright(textBaseline.getPixelAt(x,y)))++difference;
        }
        if(auto out=reportFile.getSiblingFile("text-"+juce::String(textStep)+".png").createOutputStream())juce::PNGImageFormat().writeImageToStream(frame,*out);
      }
      bool passed=frame.isValid() && frame.getWidth()==640 && frame.getHeight()==360;
      if(textStep==0)textBaseline=frame.createCopy();
      if(textStep<14 || textStep==15 || textStep==16 || textStep==17 || textStep==22)
        passed=passed && bright>1000 && (bool)state["glyphImagePresent"];
      if(textStep==12 || textStep==13 || textStep==16)passed=passed && difference>1000;
      if(textStep==12)passed=passed && (int)state["activeLyric"]==1;
      if(textStep==14)passed=passed && (int)state["textColour"]==decodeChoice(42,20) && difference>1000;
      if(textStep==15)passed=passed && frame.getPixelAt(0,0)!=textBaseline.getPixelAt(0,0);
      if(textStep==16)passed=passed && textImportPassed && (bool)state["customTypefacePresent"];
      if(textStep==17)passed=passed && !(bool)state["customTypefacePresent"];
      if(textStep==19)secondaryBaseline=frame.createCopy();
      if(textStep==20) {
        int paletteDifference=0;
        for(int y=0;y<frame.getHeight();++y)for(int x=0;x<frame.getWidth();++x)
          if(frame.getPixelAt(x,y)!=secondaryBaseline.getPixelAt(x,y))++paletteDifference;
        result->setProperty("secondaryPalettePixelsChanged",paletteDifference);passed=passed && paletteDifference>1000;
      }
      textPassed=textPassed && passed;
      result->setProperty("nonBackgroundPixels",changed);result->setProperty("brightPixels",bright);
      result->setProperty("glyphMaskDifference",difference);result->setProperty("passed",passed);panelResults.add(juce::var(result));
      if(++textStep<23){beginText();return;}
      stopTimer();auto report=new juce::DynamicObject();report->setProperty("textChecks",panelResults);
      report->setProperty("passed",textPassed);finish(juce::var(report),textPassed?0:1);return;
    }
    if(panelCheck) {
      const bool painted=(playCheck || multipleLiveCheck || (graphCheck && graphStage>0) || engine->panelFrames()>panelStart) && engine->frames()>=renderFrameStart+5;
      const bool expired=juce::Time::getMillisecondCounterHiRes()-renderStart>10000;
      if(!painted && !expired)return;
      if(compactCheck) {
        auto state=engine->viewportState();
        bool passed=painted && (int)state["headerCrop"]==60 && (int)state["nativeRegionTop"]==0 && (bool)state["nativeViewportBelowToolbar"] && (bool)state["nativeContentCropped"] && (bool)state["fitsViewport"];
        auto row=new juce::DynamicObject();row->setProperty("viewport",state);
        // Catch both failure paths: the HWND must begin below the toolbar,
        // and the VST3 wrapper must not black-fill its lightweight host paint.
        const auto header=panelHost.createComponentSnapshot({0,0,panelHost.getWidth(),82});
        int coveredPixels=0;
        for(int y=0;y<header.getHeight();++y)for(int x=0;x<header.getWidth();++x)
          if(header.getPixelAt(x,y)!=juce::Colour(0xff183c68))++coveredPixels;
        row->setProperty("toolbarPixelsOverpainted",coveredPixels);passed=passed && coveredPixels==0;
        if(auto stream=reportFile.getSiblingFile("toolbar-"+juce::String(panelStep)+".png").createOutputStream())juce::PNGImageFormat().writeImageToStream(header,*stream);
        if(panelStep%2==0) {
          engine->finishMultipleLayout();row->setProperty("afterDone",engine->viewportState());passed=passed && !engine->multipleLayoutVisible();
          engine->finishMultipleLayout();row->setProperty("afterLayout",engine->viewportState());passed=passed && engine->multipleLayoutVisible();
          engine->saveMultiplePreset();passed=passed && (bool)engine->viewportState()["multipleSaveOpen"];
          row->setProperty("nativeSaveDialogOpened",(bool)engine->viewportState()["multipleSaveOpen"]);
          engine->checkSaveMultiple("GRID toolbar test");
          const auto saved=reportFile.getParentDirectory().getChildFile("EngineProfile").findChildFiles(juce::File::findFiles,true,"*GRID*toolbar*test*");
          row->setProperty("nativePresetFilesWritten",saved.size());passed=passed && !saved.isEmpty();
          engine->checkCancelMultipleSave();
          row->setProperty("saveCancelled",!(bool)engine->viewportState()["multipleSaveOpen"]);
          passed=passed && !(bool)engine->viewportState()["multipleSaveOpen"];
          engine->checkSaveMultiple("GRID second layout");
          engine->checkCancelMultipleSave();
          // Refresh through native layout entry, then exercise both original
          // navigation handlers against two native-serialized saved presets.
          engine->finishMultipleLayout();engine->finishMultipleLayout();
          engine->stepMultiplePreset(1);const auto first=engine->viewportState();
          engine->stepMultiplePreset(1);const auto second=engine->viewportState();
          engine->stepMultiplePreset(-1);const auto previous=engine->viewportState();
          const bool navigation=(int)first["multiplePresetIndex"]>=0 &&
            first["multiplePresetIndex"]!=second["multiplePresetIndex"] &&
            first["multiplePresetIndex"]==previous["multiplePresetIndex"] &&
            first["multipleRows"]==previous["multipleRows"] && first["multipleColumns"]==previous["multipleColumns"];
          row->setProperty("savedLayoutNavigationRestoresSelection",navigation);passed=passed && navigation;
          engine->openMultiplePresets();
          row->setProperty("nativePresetBrowserOpened",(bool)engine->viewportState()["multiplePresetBrowserOpen"]);
          passed=passed && (bool)engine->viewportState()["multiplePresetBrowserOpen"];
        } else {
          passed=passed && !(bool)state["multiplePresetBrowserOpen"] && !(bool)state["multipleSaveOpen"];
          engine->startPerformance();passed=passed && engine->performancePlaying();
          engine->forwardKey(juce::KeyPress(juce::KeyPress::escapeKey));
        }
        row->setProperty("passed",passed);panelResults.add(juce::var(row));
        engine->showWorkspace(kone::grid::Workspace::lab,nullptr,{});
        if(!passed || ++panelStep==4) {
          stopTimer();auto report=new juce::DynamicObject();report->setProperty("compactToolbarChecks",panelResults);report->setProperty("passed",passed);
          finish(juce::var(report),passed?0:1);return;
        }
        enterPanel();return;
      }
      if(multipleLiveCheck) {
        const auto viewport=engine->viewportState();const auto frame=engine->latestFrame();
        const bool passed=painted && (int)viewport["multipleScreen"]!=0 && (bool)viewport["fitsViewport"] &&
          (bool)viewport["nativeContainerFound"] && (bool)viewport["nativeContainerVisible"] &&
          engine->renderingSize()==std::pair<int,int>{640,360} && frame.getWidth()==640 && frame.getHeight()==360;
        auto row=new juce::DynamicObject();row->setProperty("viewport",viewport);row->setProperty("detachedHost",panelStep==1);row->setProperty("passed",passed);
        row->setProperty("sceneWidth",frame.getWidth());row->setProperty("sceneHeight",frame.getHeight());
        panelResults.add(juce::var(row));engine->checkPanelMenuClick();
        if(!passed || ++panelStep==2) {
          stopTimer();auto result=new juce::DynamicObject();result->setProperty("multipleSceneChecks",panelResults);
          result->setProperty("passed",passed);finish(juce::var(result),passed?0:1);return;
        }
        enterPanel();return;
      }
      if(graphCheck && painted && graphStage<2) {
        if(graphStage==0) {
          if(!dialogCheck || panelStep%2!=0)engine->checkPanelMappingClick();
          const auto before=engine->viewportState()["adapterBounds"];
          const auto restarts=engine->viewportState()["contextRestarts"];
          const auto graph=engine->keyboardGraphImage(720,478);
          const auto cached=engine->keyboardGraphImage(720,478);
          graphPaintPassed=graph.isValid() && graph==cached && before==engine->viewportState()["adapterBounds"] &&
            restarts==engine->viewportState()["contextRestarts"];
          // Exercise the dialog path with a live rendering/event loop. Opening
          // and closing Mapping must not detach/recreate the native context.
          const auto coverStart=juce::Time::getMillisecondCounterHiRes();
          engine->coverPanel(true);
          const auto hidden=engine->viewportState();
          graphPaintPassed=graphPaintPassed && (bool)hidden["nativeContainerFound"] &&
            !(bool)hidden["nativeContainerVisible"] && (bool)hidden["hostPeerVisible"];
          coveredViewport=hidden;
          engine->showWorkspace(dialogCheck && panelStep%2==0?kone::grid::Workspace::multiple:kone::grid::Workspace::performance,&panelHost,panelHost.getLocalBounds());
          graphPaintPassed=restarts==engine->viewportState()["contextRestarts"] && graphPaintPassed &&
            juce::Time::getMillisecondCounterHiRes()-coverStart<100;
          juce::Timer::callAfterDelay(120,[this]{engine->coverPanel(false);});
          if(graph.isValid())if(auto stream=reportFile.withFileExtension("png").createOutputStream())
            graphPaintPassed=juce::PNGImageFormat().writeImageToStream(graph,*stream) && graphPaintPassed;
        } else {
          graphPaintPassed=(bool)engine->viewportState()["fitsViewport"] && (bool)engine->viewportState()["nativeContainerVisible"] && graphPaintPassed;
        }
        ++graphStage;panelStart=engine->panelFrames();renderFrameStart=engine->frames();
        renderStart=juce::Time::getMillisecondCounterHiRes();return;
      }
      if(playCheck) {
        const bool live=engine->performancePlaying();
        const auto frame=engine->latestFrame();
        const auto size=engine->renderingSize();
        const float strobe=engine->checkPerformanceHold(true),blackout=engine->checkPerformanceHold(false);
        engine->setPerformanceHeld(false,false,false);
        engine->forwardKey(juce::KeyPress(juce::KeyPress::escapeKey));
        engine->showWorkspace(kone::grid::Workspace::performance,&panelHost,panelHost.getLocalBounds());
        const auto stopped=engine->viewportState();
        const bool passed=painted && (bool)immediateViewport["fitsViewport"] &&
          live && size==std::pair<int,int>{640,360} && frame.getWidth()==640 && frame.getHeight()==360 &&
          strobe==(panelStep==0?1.0f:0.0f) && blackout==(panelStep==0?0.0f:1.0f) &&
          (bool)stopped["fitsViewport"] && (int)stopped["performanceScreen"]!=5;
        auto row=new juce::DynamicObject();row->setProperty("immediate",immediateViewport);
        row->setProperty("live",live);row->setProperty("sceneWidth",frame.getWidth());row->setProperty("sceneHeight",frame.getHeight());
        row->setProperty("strobeHeld",strobe);row->setProperty("blackoutHeld",blackout);
        row->setProperty("afterEscape",stopped);row->setProperty("passed",passed);
        panelResults.add(juce::var(row));
        engine->showWorkspace(kone::grid::Workspace::lab,nullptr,{});
        if(!passed || ++panelStep==2) {
          stopTimer();auto result=new juce::DynamicObject();result->setProperty("playChecks",panelResults);
          result->setProperty("passed",passed);finish(juce::var(result),passed?0:1);return;
        }
        enterPanel();return;
      }
      auto presentation=engine->panelPresentation();
      bool presentationSaved=false;
      if(presentation.isValid())if(auto stream=reportFile.getSiblingFile("panel-"+juce::String(panelStep)+".png").createOutputStream())
        presentationSaved=juce::PNGImageFormat().writeImageToStream(presentation,*stream);
      bool rightControlVisible=false;
      if(presentation.isValid()) {
        for(int y=12;y<juce::jmin(60,presentation.getHeight());++y)
          for(int x=juce::jmax(0,presentation.getWidth()-100);x<presentation.getWidth()-10;++x) {
            const auto pixel=presentation.getPixelAt(x,y);
            if(pixel.getRed()>80 && pixel.getGreen()>80 && pixel.getBlue()>80)rightControlVisible=true;
          }
      }
      const auto presentedViewport=engine->viewportState();
      const int previousHomeCalls=panelHomeCalls;
      const int previousMappingCalls=panelMappingCalls;
      if(painted && panelStep%2!=0)engine->checkPanelMappingClick();
      const bool mappingReturned=panelStep%2==0 || panelMappingCalls==previousMappingCalls+1;
      if(painted)engine->checkPanelMenuClick();
      const bool homeReturned=panelHomeCalls==previousHomeCalls+1 && !engine->requestedHome();
      const bool passed=painted && homeReturned && mappingReturned && rightControlVisible &&
        (juce::int64)presentedViewport["pendingPresentation"]==0 && (bool)presentedViewport["surfaceVisible"] && (!graphCheck || graphPaintPassed);
      auto row=new juce::DynamicObject();row->setProperty("workspace",panelStep%2==0?"Multiple":"Performance");
      row->setProperty("uiPaints",(juce::int64)(engine->panelFrames()-panelStart));
      row->setProperty("viewport",presentedViewport);row->setProperty("rightControlVisible",rightControlVisible);row->setProperty("presentedImageSaved",presentationSaved);row->setProperty("presentedWidth",presentation.getWidth());row->setProperty("presentedHeight",presentation.getHeight());
      row->setProperty("synchronousGridHome",homeReturned);row->setProperty("passed",passed);
      if(panelStep%2!=0)row->setProperty("synchronousGridMapping",mappingReturned);
      if(graphCheck) {row->setProperty("workspace",dialogCheck?(panelStep%2==0?"Multiple dialog cover":"Performance dialog cover"):"Performance keyboard graph");row->setProperty("cachedNativeKeyboardGraph",graphPaintPassed);row->setProperty("whileMappingOpen",coveredViewport);}
      panelResults.add(juce::var(row));
      if(!passed || ++panelStep==(graphCheck?2:4)) {
        stopTimer();auto result=new juce::DynamicObject();result->setProperty("panelChecks",panelResults);
        result->setProperty("passed",passed);result->setProperty("profile","isolated EngineProfile");
        finish(juce::var(result),passed?0:1);return;
      }
      enterPanel();return;
    }
    if(profileCheck) {
      const auto updateStart=juce::Time::getMillisecondCounterHiRes();engine->update(profileSettings);
      profileUpdates.add(juce::Time::getMillisecondCounterHiRes()-updateStart);
    }
    std::array<float, 512> audio{};
    for (auto &sample : audio)
      sample = .3f * std::sin((float)(audioSample++ * .031));
    engine->pushAudio(audio.data(), audio.data(), 512, 48000);
    if(profileCheck && engine->frames()>5)profileSamples.add(engine->frameTimings());
    if (engine->frames() < (profileCheck?90:8) && juce::Time::getMillisecondCounterHiRes() - renderStart < 12000)
      return;
    stopTimer();
    auto object = new juce::DynamicObject();
    object->setProperty("diagnostic", engine->diagnostic());
    if(profileCheck){object->setProperty("framePipelineSamples",profileSamples);object->setProperty("controlUpdateMs",profileUpdates);}
    object->setProperty("frames", (juce::int64)engine->frames());
    object->setProperty("state",engine->observedState());
    auto frame = engine->latestFrame();
    object->setProperty("width", frame.getWidth());
    object->setProperty("height", frame.getHeight());
    bool saved = false;
    if (frame.isValid())
      if (auto stream = reportFile.withFileExtension("png").createOutputStream())
        saved = juce::PNGImageFormat().writeImageToStream(frame, *stream);
    object->setProperty("captured", saved);
    finish(juce::var(object), saved ? 0 : 1);
  }
  void stage(const juce::String &name) {
    reportFile.withFileExtension("progress.txt").replaceWithText(name + "\n");
  }
  void finish(const juce::var &report, int code) {
    const bool written = reportFile.replaceWithText(juce::JSON::toString(report));
    stage(written ? "finished" : "report write failed");
    setApplicationReturnValue(written ? code : 3);
    quit();
  }
  static void describeSchema(const juce::XmlElement &element,
                             juce::Array<juce::var> &nodes, int depth = 0) {
    if (depth > 32 || nodes.size() >= 1024)
      return;
    if (element.isTextElement())
      return;
    auto node = new juce::DynamicObject();
    node->setProperty("element", element.getTagName());
    juce::Array<juce::var> attributes;
    for (int i = 0; i < element.getNumAttributes(); ++i)
      attributes.add(element.getAttributeName(i));
    node->setProperty("attributeNames", attributes);
    node->setProperty("depth", depth);
    nodes.add(juce::var(node));
    for (auto *child : element.getChildIterator())
      describeSchema(*child, nodes, depth + 1);
  }
  void inspect() {
    auto report = new juce::DynamicObject();
    juce::var document(report);
    report->setProperty("plugin", pluginFile.getFullPathName());
    if (!pluginFile.exists()) {
      report->setProperty("error", "Plugin does not exist");
      finish(document, 2);
      return;
    }

    juce::AudioPluginFormatManager formats;
    formats.addFormat(new juce::VST3PluginFormat()); // Manager owns the format.
    juce::OwnedArray<juce::PluginDescription> descriptions;
    stage("scanning VST3");
    formats.getFormat(0)->findAllTypesForFile(descriptions,
                                            pluginFile.getFullPathName());
    if (descriptions.isEmpty()) {
      report->setProperty("error", "No VST3 component found");
      finish(document, 1);
      return;
    }
    const auto &description = *descriptions[0];
    report->setProperty("name", description.name);
    report->setProperty("manufacturer", description.manufacturerName);
    report->setProperty("version", description.version);
    juce::String error;
    stage("creating VST3 instance");
    auto instance = formats.createPluginInstance(description, 48000, 512, error);
    if (!instance) {
      report->setProperty("error", error);
      finish(document, 1);
      return;
    }
    stage("inspecting parameters");
    report->setProperty("hasEditor", instance->hasEditor());
    report->setProperty("inputChannels", instance->getTotalNumInputChannels());
    report->setProperty("outputChannels", instance->getTotalNumOutputChannels());
    juce::Array<juce::var> parameters;
    for (auto *parameter : instance->getParameters()) {
      auto item = new juce::DynamicObject();
      item->setProperty("index", parameter->getParameterIndex());
      if (auto *hosted = dynamic_cast<juce::HostedAudioProcessorParameter *>(parameter))
        item->setProperty("id", hosted->getParameterID());
      item->setProperty("name", parameter->getName(256));
      item->setProperty("label", parameter->getLabel());
      item->setProperty("value", parameter->getValue());
      item->setProperty("defaultValue", parameter->getDefaultValue());
      item->setProperty("steps", parameter->getNumSteps());
      item->setProperty("discrete", parameter->isDiscrete());
      item->setProperty("boolean", parameter->isBoolean());
      item->setProperty("automatable", parameter->isAutomatable());
      juce::Array<juce::var> choices;
      for (const auto &text : parameter->getAllValueStrings())
        choices.add(text);
      item->setProperty("choices", choices);
      juce::Array<juce::var> samples;
      for (float value : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
        samples.add(parameter->getText(value, 256));
      item->setProperty("rangeText", samples);
      parameters.add(juce::var(item));
    }
    report->setProperty("parameters", parameters);
    juce::Array<juce::var> programs;
    for (int i = 0; i < instance->getNumPrograms(); ++i)
      programs.add(instance->getProgramName(i));
    report->setProperty("programs", programs);
    // Export structure only. Never persist attribute values, element text,
    // binary state, license codes or activation/settings files.
    stage("inspecting state structure");
    juce::MemoryBlock state;
    instance->getStateInformation(state);
    auto xml = juce::AudioProcessor::getXmlFromBinary(
        state.getData(), static_cast<int>(state.getSize()));
    juce::Array<juce::var> schema;
    if (xml)
      describeSchema(*xml, schema);
    report->setProperty("stateIsJuceXml", xml != nullptr);
    report->setProperty("stateSchema", schema);
    juce::Array<juce::var> componentSchema;
    if (xml)
      if (auto *component = xml->getChildByName("IComponent")) {
        juce::MemoryBlock bytes;
        if (bytes.fromBase64Encoding(component->getAllSubText())) {
          auto componentXml = juce::AudioProcessor::getXmlFromBinary(
              bytes.getData(), static_cast<int>(bytes.getSize()));
          if (componentXml)
            describeSchema(*componentXml, componentSchema);
        }
        bytes.reset();
      }
    report->setProperty("componentStateSchema", componentSchema);
    state.reset();
    stage("releasing VST3 instance");
    instance.reset();
    finish(document, 0);
  }
  juce::File pluginFile, reportFile;
  struct ProbeHost : juce::Component {
    void paint(juce::Graphics &g) override {g.fillAll(juce::Colour(0xff183c68));}
  } panelHost,detachedHost;
  std::unique_ptr<kone::grid::VelEngine> engine;
  bool graphicsCheck=false,graphicsPassed=true;int graphicsStep=0,graphicsPauseStage=0;
  kone::grid::SceneState graphicsSettings;
  kone::grid::SceneState profileSettings;juce::Array<juce::var> profileUpdates;
  bool compactCheck=false,profileCheck=false;juce::Array<juce::var> profileSamples;
  bool textCheck=false,textPassed=true,textImportPassed=false;int textStep=0;
  kone::grid::SceneState textSettings;juce::Image textBaseline,secondaryBaseline;
  bool panelCheck=false,playCheck=false,graphCheck=false,graphPaintPassed=false,multipleLiveCheck=false,dialogCheck=false;
  int graphStage=0;
  juce::var immediateViewport,coveredViewport;
  int panelStep=0;
  int panelHomeCalls=0;
  int panelMappingCalls=0;
  juce::uint64 panelStart=0,renderFrameStart=0;
  juce::Array<juce::var> panelResults;
  double renderStart = 0;
  juce::uint64 audioSample = 0;
};
START_JUCE_APPLICATION(EngineProbe)
