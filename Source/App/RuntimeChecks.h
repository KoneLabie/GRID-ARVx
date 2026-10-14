#pragma once
#include "Audio/SystemInput.h"
#include "Core/Presets.h"
#include "Output/Recorder.h"
#include "Plugin/Processor.h"
#include "Visual/ReferenceFx.h"
#include "SpeakerCheck.h"
namespace kone::grid {
inline juce::String runtimeChecks(const juce::File &directory) {
  juce::String report;
  auto check = [&](bool yes, const juce::String &label) {
    report += (yes ? "PASS " : "FAIL ") + label + "\n";
  };
  auto p = std::make_unique<Processor>();
  p->prepareToPlay(48000, 512);
  juce::AudioBuffer<float> audio(2, 512);
  for (int i = 0; i < 512; ++i) {
    audio.setSample(0, i, .3f * std::sin(i * .1f));
    audio.setSample(1, i, .2f * std::cos(i * .07f));
  }
  auto original = audio;
  juce::MidiBuffer midi;
  p->processBlock(audio, midi);
  bool unchanged = true;
  for (int c = 0; c < 2; ++c)
    for (int i = 0; i < 512; ++i)
      unchanged &= audio.getSample(c, i) == original.getSample(c, i);
  check(unchanged, "Audio passthrough");
  check(p->bind(1, 21, {indexOf("texture"), -1, Hardware::knob}),
        "CC knob binding");
  p->receiveMidi(juce::MidiMessage::controllerEvent(1, 21, 93));
  check(p->value("texture") == 93, "CC knob value");
  check(!p->bind(1, 22, {indexOf("texture"), -1, Hardware::button}),
        u8"Reject button → knob");
  check(!p->bind(1, 22, {indexOf("textureReact"), -1, Hardware::knob}),
        u8"Reject knob → toggle");
  p->bind(1, 22, {indexOf("textureReact"), -1, Hardware::button});
  p->receiveMidi(juce::MidiMessage::controllerEvent(1, 22, 64));
  check(p->value("textureReact") == 0,
        "Reject intermediate values for configured button");
  p->receiveMidi(juce::MidiMessage::controllerEvent(1, 22, 127));
  p->receiveMidi(juce::MidiMessage::controllerEvent(1, 22, 127));
  check(p->value("textureReact") == 127, "Held CC button triggers once");
  p->receiveMidi(juce::MidiMessage::controllerEvent(1, 22, 0));
  p->receiveMidi(juce::MidiMessage::controllerEvent(1, 22, 127));
  check(p->value("textureReact") == 0, "Second CC press toggles");
  check(!p->bind(1, 60, {indexOf("glow"), -1, Hardware::knob}, true),
        u8"Reject note button → knob");
  p->bind(1, 60, {indexOf("glassType"), 127, Hardware::button}, true);
  p->receiveMidi(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100));
  check(p->value("glassType") == 127, "Note button selects FX profile");
  p->performance = true;
  p->receiveMidi(juce::MidiMessage::controllerEvent(1, 21, 10));
  check(p->value("texture") == 93, "Suspend Lab MIDI in Performance");
  p->performance = false;
  juce::MemoryBlock state;
  p->getStateInformation(state);
  auto restored = std::make_unique<Processor>();
  restored->setStateInformation(state.getData(), (int)state.getSize());
  restored->receiveMidi(juce::MidiMessage::controllerEvent(1, 21, 49));
  check(restored->value("texture") == 49, "State restores MIDI bindings");
  const char garbage[] = "not a GRID state";
  check(!restored->restoreState(garbage, (int)sizeof(garbage)) &&
            restored->value("texture") == 49,
        "Invalid complete state preserves current settings");
  auto badTree = p->state.copyState();
  badTree.setProperty("schema", 2, nullptr);
  for (auto child : badTree)
    if (child["id"].toString() == "texture")
      child.setProperty("value", 128, nullptr);
  juce::MemoryBlock invalidState;
  juce::AudioProcessor::copyXmlToBinary(*badTree.createXml(), invalidState);
  check(!restored->restoreState(invalidState.getData(),
                                (int)invalidState.getSize()),
        "Reject complete state outside CC range");
  juce::ValueTree legacy("GRID_ARVX_STATE");
  for (auto pair : {std::pair<const char *, float>{"mode", 1.0f},
                    {"sensitivity", .5f},
                    {"speed", .5f},
                    {"texture", .5f}}) {
    juce::ValueTree child("PARAM");
    child.setProperty("id", pair.first, nullptr);
    child.setProperty("value", pair.second, nullptr);
    legacy.appendChild(child, nullptr);
  }
  juce::MemoryBlock legacyState;
  juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), legacyState);
  auto migrated = std::make_unique<Processor>();
  check(migrated->restoreState(legacyState.getData(),
                               (int)legacyState.getSize()) &&
            migrated->value("category") == encodeChoice(4, 9) &&
            migrated->value("int") == 32 && migrated->value("texture") == 64,
        "Foundation state migrates category and normalized controls");
  Presets presets(*p, directory.getChildFile("Presets"));
  p->set("category", encodeChoice(4, 9));
  p->set("variant", encodeChoice(2, 16));
  p->setAssetProperty("lyrics", "[\"FIRST\",\"SECOND\"]");
  presets.userFile(4, "Roundtrip")
      .deleteFile(); // Isolated runtime fixture only.
  check(presets.save(4, "Roundtrip", false).wasOk(), "Save User preset");
  check(presets.save(4, "Roundtrip", false).failed(),
        "Reject overwrite without confirmation");
  check(presets.save(4, "Roundtrip", true).wasOk(), "Confirmed overwrite");
  check(presets.save(4, "../outside", false).failed(),
        "Reject preset path traversal");
  p->set("texture", 0);
  check(presets.load(presets.userFile(4, "Roundtrip")).wasOk() &&
            p->value("texture") == 93,
        "Preset round trip");
  p->set("lock", 127);
  p->set("texture", 15);
  p->set("color", 99);
  presets.load(presets.userFile(4, "Roundtrip"));
  check(p->value("texture") == 15 && p->value("color") == 20,
        "Lock keeps FX while restoring color");
  p->set("presetCycle", 127);
  p->set("presetInterval", 2);
  p->set("muteInput", 127);
  presets.load(presets.userFile(4, "Roundtrip"));
  check(p->value("lock") == 127 && p->value("presetCycle") == 127 &&
            p->value("presetInterval") == 2 && p->value("muteInput") == 127,
        "Preset recalls preserve lock, sequencer and analysis mute settings");
  p->set("muteInput", 0);
  auto bad = presets.directory(4, "User").getChildFile("Bad.json");
  bad.replaceWithText("{\"schema\":2,\"category\":\"Basic "
                      "Colors\",\"parameters\":{\"texture\":128}}");
  check(presets.load(bad).failed(), "Reject out-of-range preset values");
  auto oversized =
      juce::JSON::parse(presets.userFile(4, "Roundtrip").loadFileAsString());
  oversized.getDynamicObject()->setProperty(
      "lyrics", juce::Array<juce::var>{juce::String::repeatedString("X", 121)});
  auto invalidLyrics = directory.getChildFile("invalid-lyrics.json");
  invalidLyrics.replaceWithText(juce::JSON::toString(oversized));
  check(presets.load(invalidLyrics).failed(),
        "Preset imports reject lyrics beyond the editor limits");
  auto sourceAsset = directory.getChildFile("asset-contents.bin");
  sourceAsset.replaceWithText("version one");
  p->setAssetProperty("imagePath", sourceAsset.getFullPathName());
  presets.save(4, "Asset First", true);
  const auto firstAsset =
      juce::JSON::parse(
          presets.userFile(4, "Asset First").loadFileAsString())["imagePath"]
          .toString();
  sourceAsset.replaceWithText("version two");
  presets.save(4, "Asset Second", true);
  const auto secondAsset =
      juce::JSON::parse(
          presets.userFile(4, "Asset Second").loadFileAsString())["imagePath"]
          .toString();
  check(firstAsset != secondAsset &&
            presets.directory(4, "User")
                    .getChildFile(firstAsset)
                    .loadFileAsString() == "version one" &&
            presets.directory(4, "User")
                    .getChildFile(secondAsset)
                    .loadFileAsString() == "version two",
        "Changed asset contents create independent portable preset copies");
  p->setAssetProperty("imagePath", "");
  p->set("category", encodeChoice(7, 9));
  p->setAssetProperty("lyrics", "[\"KEEP ME\"]");
  p->set("textEffect", encodeChoice(3, 7));
  p->set("lyricCycle", 127);
  presets.load(presets.files(7, "Defaults")[1]);
  check(p->getAssetProperty("lyrics").contains("KEEP ME") &&
            p->value("textEffect") == encodeChoice(3, 7) &&
            p->value("lyricCycle") == 127,
        "Text default style changes preserve input lyrics and independent text "
        "effects");
  juce::Image mask(juce::Image::ARGB, 100, 100, true,
                   juce::SoftwareImageType());
  {
    juce::Graphics g(mask);
    g.setColour(juce::Colours::white);
    g.fillEllipse(10, 10, 80, 80);
    g.setColour(juce::Colours::transparentBlack);
    g.setOpacity(1);
    g.fillAll();
  } // Construct a visible annulus without relying on alpha clear blending.
  for (int y = 0; y < 100; ++y)
    for (int x = 0; x < 100; ++x) {
      float r = std::hypot(x - 50.0f, y - 50.0f);
      mask.setPixelAt(x, y,
                      r > 20 && r < 40 ? juce::Colours::white
                                       : juce::Colours::transparentBlack);
    }
  auto path = traceImage(mask);
  check(!path.isEmpty() && path.getBounds().getWidth() > 1.5f,
        "Trace image with internal hole");
  auto faintMask=mask.createCopy();
  faintMask.multiplyAllAlphas(.25f);
  check(!traceImage(faintMask).isEmpty(),"Trace retains outlines in 25-percent-opacity images");
  juce::Image uniform(juce::Image::ARGB,16,16,true,juce::SoftwareImageType());
  {juce::Graphics g(uniform);g.fillAll(juce::Colours::grey);}
  check(traceImage(uniform).isEmpty(),"Uniform image reports no trace instead of generating a frame border");
  Assets assets;
  assets.lyrics = {"GRID-ARVx"};
  bool scenes = true;
  for (int c = 0; c < 9; ++c)
    for (int v = 0; v < templates(c).size(); ++v) {
      SceneState s;
      for (size_t i = 0; i < controls.size(); ++i)
        s.values[i] = controls[i].initial;
      s.values[(size_t)indexOf("category")] = encodeChoice(c, 9);
      s.values[(size_t)indexOf("variant")] = encodeChoice(v, 16);
      s.width = 320;
      s.height = 180;
      s.audio.rms = .3f;
      s.time = 1.5;
      auto im = drawScene(s, assets);
      scenes &= im.isValid() && im.getWidth() == 320;
    }
  check(scenes, "Every shipped scene template renders");
  {
    SceneState s;
    for (size_t i = 0; i < controls.size(); ++i)
      s.values[i] = controls[i].initial;
    s.values[(size_t)indexOf("category")] = encodeChoice(7, 9);
    s.width = 320;
    s.height = 180;
    assets.lyrics = {"TEXT"};
    auto clean = drawScene(s, assets);
    assets.contour = path;
    auto withContour = drawScene(s, assets);
    bool independent = true;
    for (int y = 0; y < s.height; ++y)
      for (int x = 0; x < s.width; ++x)
        independent &= clean.getPixelAt(x, y) == withContour.getPixelAt(x, y);
    check(independent,
          "Text rendering does not combine with a visual-mode contour");
    assets.contour.clear();
    for (int v = 0; v < templates(7).size(); ++v) {
      s.values[(size_t)indexOf("variant")] = encodeChoice(v, 16);
      assets.lyrics = {""};
      auto blank = drawScene(s, assets);
      assets.lyrics = {"TEXT"};
      auto letters = drawScene(s, assets);
      int changed = 0;
      for (int y = 0; y < s.height; ++y)
        for (int x = 0; x < s.width; ++x)
          changed += blank.getPixelAt(x, y) != letters.getPixelAt(x, y);
      check(changed > 20, "Text glyphs are visible / " + templates(7)[v]);
    }
  }
#if JUCE_WINDOWS
  {
    SystemInput loopback(*p);
    auto opened = loopback.open();
    if (opened.failed() &&
        juce::SystemStats::getEnvironmentVariable("CI", "").isNotEmpty())
      report += "SKIP WASAPI playback endpoint unavailable on CI: " +
                opened.getErrorMessage() + "\n";
    else
      check(opened.wasOk(),
            "Windows WASAPI loopback opens default playback endpoint");
    if (opened.wasOk()) {
      juce::Thread::sleep(100);
      loopback.close();
      check(std::isfinite(p->analysis.consume().rms),
            "System-audio analysis remains finite");
    }
    const auto speakers = SystemInput::playbackDevices();
    if (!speakers.isEmpty()) {
      check(speakers[0].id.isNotEmpty() && speakers[0].name.isNotEmpty(),
            "Playback devices expose friendly names and selectable endpoint IDs");
      auto selected = loopback.open(speakers[0].id);
      check(selected.wasOk(), "WASAPI loopback opens an explicitly selected speaker endpoint");
      loopback.close();
    }
    check(loopback.open("GRID-ARVx-unavailable-test-endpoint").failed(),
          "Unavailable playback endpoint returns an error instead of opening the microphone");
    p->setAssetProperty("speakerDevice", "test-selected-speaker");
    p->setAssetProperty("audioSource", "system");
    juce::MemoryBlock saved; p->getStateInformation(saved);
    Processor speakerRestored; speakerRestored.setStateInformation(saved.getData(), (int)saved.getSize());
    check(speakerRestored.getAssetProperty("speakerDevice") == "test-selected-speaker" &&
              speakerRestored.getAssetProperty("audioSource") == "system",
          "Speaker device and source mode survive a complete-state round trip");
    p->setAssetProperty("speakerDevice", ""); p->setAssetProperty("audioSource", "hardware");
  }
#endif
  {
    juce::Image portrait(juce::Image::RGB, 100, 200, true,
                         juce::SoftwareImageType());
    juce::Graphics g(portrait);
    g.fillAll(juce::Colours::red);
    g.setColour(juce::Colours::blue);
    g.fillRect(0, 0, 100, 50);
    auto fitted = recordingFrame(portrait, 320, 180);
    check(fitted.getPixelAt(10, 90) == juce::Colours::black &&
              fitted.getPixelAt(160, 20).getRed() > 200 &&
              fitted.getPixelAt(160, 165).getBlue() > 200,
          "Recording preserves aspect ratio and flips GPU scanlines");
  }
  auto encoder = findEncoder(
      juce::SystemStats::getEnvironmentVariable("GRID_TEST_FFMPEG", ""));
  if (encoder.existsAsFile()) {
    Recorder recorder;
    RecordSettings rs;
    rs.encoder = encoder;
    rs.output = directory.getChildFile(
        "capture-" + juce::String(juce::Time::getMillisecondCounter()) +
        ".mp4");
    rs.width = 320;
    rs.height = 180;
    rs.fps = 24;
    rs.seconds = 2;
    auto started = recorder.start(rs);
    check(started.wasOk(), "Recorder starts");
    if (started.wasOk()) {
      juce::Image frame(juce::Image::ARGB, 320, 180, true,
                        juce::SoftwareImageType());
      {
        juce::Graphics g(frame);
        g.fillAll(juce::Colours::orange);
        g.setColour(juce::Colours::black);
        g.fillRect(0, 0, 100, 100);
      }
      for (int i = 0; i < 16; ++i) {
        recorder.submit(frame);
        juce::Thread::sleep(45);
      }
      recorder.stop();
      check(recorder.error().isEmpty() && rs.output.getSize() > 1024 &&
                recorder.encoded > 10,
            "Recorder writes nonempty MP4 without encoder errors");
      rs.output = directory.getChildFile(
          "timed-" + juce::String(juce::Time::getMillisecondCounter()) +
          ".mkv");
      rs.seconds = 1;
      check(recorder.start(rs).wasOk(), "Recorder starts MKV");
      recorder.submit(frame);
      for (int i = 0; i < 30 && recorder.active(); ++i)
        juce::Thread::sleep(100);
      const bool stopped = !recorder.active();
      recorder.stop();
      check(stopped && recorder.error().isEmpty() && recorder.encoded == 24 &&
                rs.output.getSize() > 1024,
            "Recording time limit finalizes without further preview frames");
    }
  } else
    report += "SKIP Recorder integration: GRID_TEST_FFMPEG is not set to an "
              "installed executable.\n";
  {
    SceneState s;
    for (size_t i = 0; i < controls.size(); ++i) s.values[i] = controls[i].initial;
    auto set = [&](const char *id, int v) { s.values[(size_t)indexOf(id)] = v; };
    set("color", 127); set("glow", 0); s.time = 2;
    ReferenceFx fx;
    ReferenceFx::Buffer fixture((size_t)ReferenceFx::width * ReferenceFx::height * 4);
    for (int y = 0; y < ReferenceFx::height; ++y) for (int x = 0; x < ReferenceFx::width; ++x) {
      const auto n = (size_t)(y * ReferenceFx::width + x) * 4;
      const auto v = (juce::uint8)(20 + x * 210 / ReferenceFx::width);
      fixture[n] = fixture[n + 1] = fixture[n + 2] = v; fixture[n + 3] = 255;
    }
    auto result = fixture; fx.ditherAndTexture(result, s, 0, 0); fx.glass(result, s, 0);
    check(result == fixture, "Native FX zero values bypass exactly");
    for (int type = 0; type < 4; ++type) {
      set("textureType", encodeChoice(type, 4)); result = fixture;
      fx.ditherAndTexture(result, s, 0, 1);
      check(result != fixture, "Native Texture changes pixels / " + profiles(0)[type]);
      if (type != 2) {
        bool valid = true;
        for (size_t n = 0; n < result.size(); n += 4)
          valid &= (result[n] == 0 || result[n] == 255) && result[n] == result[n + 1] && result[n] == result[n + 2] && result[n + 3] == 255;
        check(valid, "Native palette quantization stays in the selected palette / " + profiles(0)[type]);
      }
    }
    set("textureType", 0);
    auto a = fixture, b = fixture;
    fx.ditherAndTexture(a, s, 0, 1); fx.ditherAndTexture(b, s, 0, 1);
    check(a != b, "Native Noise regenerates independently on each frame");
    for (int type = 0; type < 4; ++type) {
      set("ditherType", encodeChoice(type, 4)); result = fixture;
      fx.ditherAndTexture(result, s, 1, 0);
      check(result != fixture, "Native Dither produces glyph/shape pixels / " + profiles(1)[type]);
    }
    std::vector<ReferenceFx::Buffer> glassFrames;
    for (int type = 0; type < 6; ++type) {
      set("glassType", encodeChoice(type, 6)); result = fixture;
      fx.glass(result, s, 1);
      bool valid = result != fixture;
      for (size_t n = 3; n < result.size(); n += 4) valid &= result[n] == 255;
      check(valid, "Native Glass changes pixels and retains opaque alpha / " + profiles(3)[type]);
      for (const auto &previous : glassFrames) check(previous != result, "Native Glass shapes have distinct output");
      glassFrames.push_back(result);
    }
    const auto &ball = glassFrames.front();
    check(std::equal(ball.begin(), ball.begin() + 40, fixture.begin()), "Native Glass retains the corner outside the lens at maximum");
    // Isolate the lens outline from scene movement and animated reflections.
    auto flat=fixture;
    for(size_t n=0;n<flat.size();n+=4)flat[n]=flat[n+1]=flat[n+2]=40;
    for(int type:{0,1,2,4}) {
      set("glassType",encodeChoice(type,6));
      auto first=flat,second=flat;
      s.time=0;fx.reset();fx.glass(first,s,1);
      s.time=1.2;fx.glass(second,s,1);
      auto bounds=[&](const ReferenceFx::Buffer &pixels) {
        int left=600,right=-1,top=500,bottom=-1;
        for(int y=0;y<500;++y)for(int x=0;x<600;++x) {
          const auto n=(size_t)(y*600+x)*4;
          if(pixels[n]!=40||pixels[n+1]!=40||pixels[n+2]!=40) {
            left=juce::jmin(left,x);right=juce::jmax(right,x);top=juce::jmin(top,y);bottom=juce::jmax(bottom,y);
          }
        }
        return juce::Rectangle<int>(left,top,right-left+1,bottom-top+1);
      };
      check(bounds(first)==bounds(second),"Glass lens outline stays stationary / "+profiles(3)[type]);
    }
    s.audio.averageMagnitude=.015f;s.audio.rms=.017f;set("int",0);
    for(const char* id:{"texture","dither","track","glass"}) {
      set(id,100);set((juce::String(id)+"React").toRawUTF8(),127);
    }
    fx.reset();
    float strongest=0;bool off=false;
    for(int frame=0;frame<120;++frame) {
      s.time=frame/60.0;
      for(auto amount:fx.effectiveAmounts(s)){strongest=juce::jmax(strongest,amount);off|=amount==0;}
    }
    check(strongest>.6f&&off,"Quiet playback gates selected FX strength independently of INT instead of shrinking effects");
    s.audio.averageMagnitude=0;
    const auto silent=fx.effectiveAmounts(s);
    check(std::all_of(silent.begin(),silent.end(),[](float amount){return amount==0;}),"Reactive FX turn off in silence");
  }
#if JUCE_WINDOWS
  if(juce::SystemStats::getEnvironmentVariable("CI", "").isEmpty()) report+=speakerSignalCheck();
#endif
  directory.getChildFile("runtime-checks.txt").replaceWithText(report);
  return report;
}
} // namespace kone::grid
