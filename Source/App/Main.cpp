#include "Audio/SystemInput.h"
#include "Plugin/Editor.h"
#include "RuntimeChecks.h"
#if JUCE_WINDOWS
#include <windows.h>
#undef small
#endif

namespace kone::grid {
static juce::Rectangle<int> monitorWorkArea(int number) {
#if JUCE_WINDOWS
  struct Query {juce::String device;juce::Rectangle<int> area;};
  Query query{juce::String("\\\\.\\DISPLAY")+juce::String(number),{}};
  EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR monitor,HDC,LPRECT,LPARAM parameter)->BOOL {
    auto &query=*reinterpret_cast<Query *>(parameter);
    MONITORINFOEXW info{};info.cbSize=sizeof(info);
    if(GetMonitorInfoW(monitor,reinterpret_cast<MONITORINFO *>(&info)) && query.device==juce::String(info.szDevice)) {
      const auto r=info.rcWork;
      query.area=juce::Desktop::getInstance().getDisplays().physicalToLogical(
          juce::Rectangle<int>(r.left,r.top,r.right-r.left,r.bottom-r.top));
      return FALSE;
    }
    return TRUE;
  },reinterpret_cast<LPARAM>(&query));
  return query.area;
#else
  const auto &displays=juce::Desktop::getInstance().getDisplays().displays;
  return juce::isPositiveAndBelow(number-1,displays.size())?displays[number-1].userArea:juce::Rectangle<int>{};
#endif
}

class Input final : public juce::AudioIODeviceCallback,
                    public juce::MidiInputCallback {
public:
  Input(Processor &processor, juce::AudioDeviceManager &devices) : processor_(processor), devices_(devices) {
    midi_.ensureSize(65536);
  }
  void audioDeviceAboutToStart(juce::AudioIODevice *device) override {
    processor_.setInputSampleRate(device->getCurrentSampleRate());
    collector_.reset(device->getCurrentSampleRate());
  }
  void handleIncomingMidiMessage(juce::MidiInput *,
                                 const juce::MidiMessage &m) override {
    collector_.addMessageToQueue(m);
  }
  void pollMidi(int samples) {
    refreshOutput();
    collector_.removeNextBlockOfMessages(midi_, samples);
    for (auto m : midi_)
      processor_.receiveMidi(m.getMessage());
    midi_.clear();
  }
  void sendFeedback(const juce::MidiMessage &message) {
    const juce::ScopedLock lock(outputLock_);
    if (output_) output_->sendMessageNow(message);
  }
  void setSystemSource(bool enabled) noexcept { systemSource_.store(enabled); }
  void audioDeviceStopped() override {}
  void audioDeviceIOCallbackWithContext(
      const float *const *input, int inputs, float *const *output, int outputs,
      int samples, const juce::AudioIODeviceCallbackContext &) override {
    for (int channel = 0; channel < outputs; ++channel)
      if (output[channel])
        juce::FloatVectorOperations::clear(output[channel], samples);
    pollMidi(samples);
    if (!systemSource_.load() && inputs > 0 && processor_.value("muteInput") < 64)
      processor_.analyzeInput(input[0], inputs > 1 ? input[1] : nullptr,
                               samples);
  }

private:
  void refreshOutput() {
    const auto id = devices_.getDefaultMidiOutputIdentifier();
    const juce::ScopedLock lock(outputLock_);
    if (id == outputId_) return;
    output_.reset(); outputId_ = id;
    if (id.isNotEmpty()) output_ = juce::MidiOutput::openDevice(id);
  }
  Processor &processor_;
  juce::AudioDeviceManager &devices_;
  juce::MidiMessageCollector collector_;
  juce::MidiBuffer midi_;
  std::atomic<bool> systemSource_{false};
  juce::CriticalSection outputLock_;
  std::unique_ptr<juce::MidiOutput> output_;
  juce::String outputId_;
};
class Window final : public juce::DocumentWindow {
public:
  Window(Processor &processor, std::function<void()> quit,
         std::function<std::unique_ptr<juce::Component>()> audio,
         juce::File presetRoot = {},juce::Rectangle<int> targetArea = {})
      : DocumentWindow("GRID-ARVx - " GRID_APP_VERSION, juce::Colour(0xff151a20), allButtons),
        quit_(std::move(quit)) {
    setUsingNativeTitleBar(true);
    auto *editor = new Editor(processor, presetRoot);
    editor->setAudioSettingsAction(std::move(audio));
    setContentOwned(editor, true);
    setResizable(true, false);
    setResizeLimits(960, 620, 2560, 1600);
    if(targetArea.isEmpty())centreWithSize(1120,740);
    else setBounds(juce::Rectangle<int>(juce::jmin(1120,targetArea.getWidth()),juce::jmin(740,targetArea.getHeight()))
          .withCentre(targetArea.getCentre()));
    setVisible(true);
    if(targetArea.isEmpty())editor->grabKeyboardFocus();
  }
  void closeButtonPressed() override { quit_(); }
  juce::BorderSize<int> getBorderThickness() const override {
    return isUsingNativeTitleBar()?DocumentWindow::getBorderThickness():juce::BorderSize<int>{};
  }
  Editor *editor() { return dynamic_cast<Editor *>(getContentComponent()); }

private:
  std::function<void()> quit_;
};
class Application final : public juce::JUCEApplication {
public:
  const juce::String getApplicationName() override { return "GRID-ARVx"; }
  const juce::String getApplicationVersion() override { return GRID_APP_VERSION; }
  bool moreThanOneInstanceAllowed() override {
    // Every regression mode uses its own profile. Never redirect its launch
    // to a running review instance, where no test would actually execute.
    return isolatedCheck(getCommandLineParameters());
  }
  void initialise(const juce::String &arguments) override {
    selfTest_ = isolatedCheck(arguments);
    if (arguments.startsWith("--self-test ")) {
      auto folder = juce::File(
          arguments.fromFirstOccurrenceOf("--self-test ", false, false)
              .unquoted());
      folder.createDirectory();
      auto report = runtimeChecks(folder);
      setApplicationReturnValue(report.contains("FAIL") ? 1 : 0);
      quit();
      return;
    }
    directory_ =
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Cenaceae")
            .getChildFile("GRID-ARVx");
    if (selfTest_)
      directory_ =
          juce::File(
              arguments.fromFirstOccurrenceOf(" ", false, false).unquoted())
              .getChildFile("State");
    directory_.createDirectory();
    processor_ = std::make_unique<Processor>();
    juce::MemoryBlock state;
    if (!selfTest_ &&
        directory_.getChildFile("state.bin").loadFileAsData(state))
      processor_->setStateInformation(state.getData(),
                                      static_cast<int>(state.getSize()));
    else if (!selfTest_)
      processor_->setAssetProperty("freshStart", "1");
    juce::Rectangle<int> targetArea=selfTest_?monitorWorkArea(2):juce::Rectangle<int>{};
    if(arguments.startsWith("--monitor=")) {
      targetArea=monitorWorkArea(arguments.fromFirstOccurrenceOf("=",false,false).getIntValue());
      if(targetArea.isEmpty()){setApplicationReturnValue(4);quit();return;}
    }
    input_ = std::make_unique<Input>(*processor_, devices_);
    processor_->setMidiFeedback([this](const juce::MidiMessage &message) {
      if (input_) input_->sendFeedback(message);
    });
    systemInput_ = std::make_unique<SystemInput>(*processor_);
    systemInput_->midiPoll = [this](int n) { input_->pollMidi(n); };
    window_ = std::make_unique<Window>(
        *processor_, [this] { systemRequestedQuit(); },
        [this] { return makeAudioPanel(); },
        selfTest_ ? directory_.getChildFile("Presets") : juce::File(),targetArea);
    window_->editor()->setSystemAudioAction(
        [this](bool system) { return selectAudioSource(system); });
    if(arguments.startsWith("--transition-check ")) {
      const auto folder=juce::File(arguments.fromFirstOccurrenceOf(" ",false,false).unquoted());
      window_->editor()->runTransitionChecks(folder,[this,folder]{
        setApplicationReturnValue(folder.getChildFile("result.txt").loadFileAsString().startsWith("PASS")?0:1);quit();
      });return;
    }
    if (arguments.startsWith("--record-check ")) {
      auto folder = juce::File(
          arguments.fromFirstOccurrenceOf("--record-check ", false, false)
              .unquoted());
      window_->editor()->runRecordingChecks(folder, [this, folder] {
        setApplicationReturnValue(folder.getChildFile("recording-checks.txt")
                                          .loadFileAsString()
                                          .contains("FAIL")
                                      ? 1
                                      : 0);
        quit();
      });
      return;
    }
    if (arguments.startsWith("--render-check ") ||
        arguments.startsWith("--fx-check ")) {
      const bool effectsOnly = arguments.startsWith("--fx-check ");
      auto folder = juce::File(
          arguments.fromFirstOccurrenceOf(" ", false, false).unquoted());
      window_->editor()->runRenderChecks(
          folder,
          [this, folder] {
            setApplicationReturnValue(folder.getChildFile("result.txt")
                                              .loadFileAsString()
                                              .contains("FAIL")
                                          ? 1
                                          : 0);
            quit();
          },
          effectsOnly);
      return;
    }
    if (arguments.startsWith("--ui-check ")) {
      auto folder = juce::File(
          arguments.fromFirstOccurrenceOf("--ui-check ", false, false)
              .unquoted());
      openAudio();
      auto report = window_->editor()->runUiChecks(folder);
      setApplicationReturnValue(report.contains("FAIL") ? 1 : 0);
      quit();
      return;
    }
    if (juce::RuntimePermissions::isRequired(
            juce::RuntimePermissions::recordAudio) &&
        !juce::RuntimePermissions::isGranted(
            juce::RuntimePermissions::recordAudio))
      juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
                                        [this](bool allowed) {
                                          if (allowed)
                                            openAudio();
                                        });
    else
      openAudio();
  }
  void shutdown() override {
    if (systemInput_)
      systemInput_->close();
    if (input_) {
      devices_.removeAudioCallback(input_.get());
      devices_.removeMidiInputDeviceCallback({}, input_.get());
    }
    if (!selfTest_)
      if (auto xml = devices_.createStateXml())
        xml->writeTo(directory_.getChildFile("audio.xml"));
    devices_.closeAudioDevice();
    window_.reset();
    if (processor_ && !selfTest_) {
      juce::MemoryBlock state;
      processor_->getStateInformation(state);
      directory_.getChildFile("state.bin")
          .replaceWithData(state.getData(), state.getSize());
    }
    systemInput_.reset();
    input_.reset();
    processor_.reset();
  }

private:
  static bool isolatedCheck(const juce::String &arguments) {
    for(const char *flag:{"--transition-check ","--render-check ","--self-test ","--ui-check ","--record-check ","--fx-check "})
      if(arguments.startsWith(flag))return true;
    return false;
  }
  void openAudio() {
    const auto xml =
        juce::XmlDocument::parse(directory_.getChildFile("audio.xml"));
    // Analysis never plays audio. Do not open/claim an output device, which can
    // block music playback when the interface uses an exclusive driver.
    const auto error = devices_.initialise(2, 0, xml.get(), true);
    devices_.addAudioCallback(input_.get());
    devices_.addMidiInputDeviceCallback({}, input_.get());
    if (processor_->getAssetProperty("audioSource") == "system") {
      const auto result = selectAudioSource(true);
      if (result.failed())
        processor_->setAssetProperty("audioSource", "hardware");
    }
    if (error.isNotEmpty() && !selfTest_ && !usingSystem_)
      juce::AlertWindow::showMessageBoxAsync(
          juce::MessageBoxIconType::WarningIcon, "Audio input",
          error + "\nChoose an input in Audio input settings.");
  }
  juce::Result selectAudioSource(bool system) {
    const auto requested = processor_->getAssetProperty("speakerDevice");
    if (system == usingSystem_ && (!system || requested == activeSpeaker_)) {
      processor_->setAssetProperty("audioSource", system ? "system" : "hardware");
      return juce::Result::ok();
    }
    if (!system) {
      input_->setSystemSource(false);
      systemInput_->close();
      processor_->analysis.reset();
      devices_.addAudioCallback(input_.get());
      usingSystem_ = false;
    } else {
      const bool previousSystem = usingSystem_;
      input_->setSystemSource(true);
      if (!usingSystem_) devices_.removeAudioCallback(input_.get());
      auto result = systemInput_->open(requested);
      if (result.failed()) {
        processor_->setAssetProperty("speakerDevice", activeSpeaker_);
        if (previousSystem && systemInput_->open(activeSpeaker_).wasOk()) {
          usingSystem_ = true;
          processor_->setAssetProperty("audioSource", "system");
        } else {
          input_->setSystemSource(false);
          devices_.addAudioCallback(input_.get());
          usingSystem_ = false;
          processor_->setAssetProperty("audioSource", "hardware");
        }
        return result;
      }
      activeSpeaker_ = requested;
      usingSystem_ = true;
    }
    processor_->setAssetProperty("audioSource", system ? "system" : "hardware");
    return juce::Result::ok();
  }
  juce::Result selectSpeaker(const juce::String &id) {
    processor_->setAssetProperty("speakerDevice", id);
    return selectAudioSource(true);
  }
  std::unique_ptr<juce::Component> makeAudioPanel() {
    class AudioPanel final : public juce::Component {
    public:
      AudioPanel(juce::AudioDeviceManager &devices, Processor &p,
                 std::function<juce::Result(bool)> sourceAction,
                 std::function<juce::Result(const juce::String &)> speakerAction)
          : processor(p), changeSource(std::move(sourceAction)),
            changeSpeaker(std::move(speakerAction)),
            audioSelector(devices, 0, 2, 0, 0, false, false, true, false),
            midiSelector(devices, 0, 0, 0, 0, true, true, false, false) {
        sourceGroup.setText("Audio source");
        addAndMakeVisible(sourceGroup);
        deviceGroup.setText("Audio devices");
        addAndMakeVisible(deviceGroup);
        midiGroup.setText("MIDI inputs");
        addAndMakeVisible(midiGroup);
        sourceLabel.setText("Analyze audio from", juce::dontSendNotification);
        addAndMakeVisible(sourceLabel);
        source.setComponentID("capture-source");
        source.addItem("Microphone / interface", 1);
#if JUCE_WINDOWS
        source.addItem("Speakers / system audio", 2);
#endif
        addAndMakeVisible(source);
        source.onChange = [this] {
          const auto result = changeSource(source.getSelectedId() == 2);
          refreshSource();
          message.setText(result.failed() ? result.getErrorMessage() :
              source.getSelectedId() == 2 ? "Play music through the selected speakers or headphones. No virtual microphone is needed."
                                         : "Choose a microphone or interface below. MIDI inputs work in either mode.",
              juce::dontSendNotification);
        };
        speakersLabel.setText("Output", juce::dontSendNotification);
        addAndMakeVisible(speakersLabel);
        speakers.setComponentID("speaker-device");
        speakers.onChange = [this] {
          const int selected = speakers.getSelectedId();
          if (selected < 1 || selected > endpoints.size() + 1) return;
          const auto id = selected == 1 ? juce::String() : endpoints[selected - 2].id;
          const auto result = changeSpeaker(id);
          refreshSource();
          refreshDevices();
          message.setText(result.failed() ? result.getErrorMessage() :
              "Listening to " + speakers.getText() + ". Play music to animate the visuals.",
              juce::dontSendNotification);
        };
        addAndMakeVisible(speakers);
        refresh.setButtonText("Refresh");
        refresh.onClick = [this] { refreshDevices(); };
        addAndMakeVisible(refresh);
        message.setComponentID("capture-help");
        message.setText("Choose a microphone or capture speaker playback directly.", juce::dontSendNotification);
        addAndMakeVisible(message);
        activity.setComponentID("capture-activity");
        addAndMakeVisible(activity);
        addAndMakeVisible(mute);
        mute.setComponentID("muteInput");
        mute.setButtonText("Mute analysis input");
        mute.setToggleState(p.value("muteInput") >= 64, juce::dontSendNotification);
        mute.onClick = [this] { processor.set("muteInput", mute.getToggleState() ? 127 : 0); };
        deviceNote.setText("Microphone/interface and MIDI inputs apply when using either source mode.", juce::dontSendNotification);
        addAndMakeVisible(deviceNote);
        addAndMakeVisible(audioViewport);
        audioViewport.setViewedComponent(&audioSelector, false);
        addAndMakeVisible(midiViewport);
        midiViewport.setViewedComponent(&midiSelector, false);
        refreshDevices(); refreshSource();
        setSize(640, 520);
      }
      void refreshDevices() {
        endpoints = SystemInput::playbackDevices();
        speakers.clear(juce::dontSendNotification);
        speakers.addItem("Windows default speakers / headphones", 1);
        for (int i = 0; i < endpoints.size(); ++i) speakers.addItem(endpoints[i].name, i + 2);
        const auto saved = processor.getAssetProperty("speakerDevice");
        int selected = 1;
        for (int i = 0; i < endpoints.size(); ++i) if (endpoints[i].id == saved) selected = i + 2;
        if (saved.isNotEmpty() && selected == 1) {
          selected = endpoints.size() + 2;
          speakers.addItem("Saved playback device (unavailable)", selected);
          speakers.setItemEnabled(selected, false);
        }
        speakers.setSelectedId(selected, juce::dontSendNotification);
      }
      void refreshSource() {
        const bool system = processor.getAssetProperty("audioSource") == "system";
        source.setSelectedId(system ? 2 : 1, juce::dontSendNotification);
#if JUCE_WINDOWS
        speakers.setEnabled(system); refresh.setEnabled(system);
#else
        speakers.setEnabled(false); refresh.setEnabled(false);
#endif
      }
      void resized() override {
        const auto width = getWidth() - 30;
        sourceGroup.setBounds(8, 0, getWidth() - 16, 198);
        deviceGroup.setBounds(8, 207, getWidth() - 16, 165);
        midiGroup.setBounds(8, 380, getWidth() - 16, getHeight() - 380);
        sourceLabel.setBounds(22, 30, 175, 28);
        source.setBounds(205, 30, getWidth() - 220, 28);
        speakersLabel.setBounds(22, 66, 175, 28);
        speakers.setBounds(205, 66, getWidth() - 300, 28);
        refresh.setBounds(getWidth() - 88, 66, 73, 28);
        message.setBounds(22, 99, width, 36);
        mute.setBounds(22, 138, width, 26);
        activity.setBounds(22, 166, width, 24);
        deviceNote.setBounds(22, 232, width, 30);
        audioViewport.setBounds(15, 266, getWidth() - 30, 100);
        midiViewport.setBounds(15, 424, getWidth() - 30, getHeight() - 438);
        audioSelector.setSize(getWidth() - audioViewport.getScrollBarThickness(), 96);
        midiSelector.setSize(getWidth() - midiViewport.getScrollBarThickness(), 220);
      }
      Processor &processor;
      std::function<juce::Result(bool)> changeSource;
      std::function<juce::Result(const juce::String &)> changeSpeaker;
      juce::Array<SystemInput::PlaybackDevice> endpoints;
      juce::AudioDeviceSelectorComponent audioSelector, midiSelector;
      juce::Viewport audioViewport, midiViewport;
      juce::ComboBox source, speakers;
      juce::TextButton refresh;
      juce::Label sourceLabel, speakersLabel, message, deviceNote, activity;
      juce::ToggleButton mute;
      juce::GroupComponent sourceGroup, deviceGroup, midiGroup;
    };
    return std::make_unique<AudioPanel>(devices_, *processor_,
        [this](bool system) { return selectAudioSource(system); },
        [this](const juce::String &id) { return selectSpeaker(id); });
  }
  juce::File directory_;
  juce::AudioDeviceManager devices_;
  bool selfTest_ = false;
  bool usingSystem_ = false;
  juce::String activeSpeaker_;
  std::unique_ptr<SystemInput> systemInput_;
  std::unique_ptr<Processor> processor_;
  std::unique_ptr<Input> input_;
  std::unique_ptr<Window> window_;
};
} // namespace kone::grid
START_JUCE_APPLICATION(kone::grid::Application)
