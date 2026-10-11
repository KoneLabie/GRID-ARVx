#include "Plugin/Editor.h"

namespace kone::grid
{
class Input final : public juce::AudioIODeviceCallback
{
public:
    explicit Input(AudioAnalysis& analysis) : analysis_(analysis) {}
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override { analysis_.setSampleRate(device->getCurrentSampleRate()); }
    void audioDeviceStopped() override {}
    void audioDeviceIOCallbackWithContext(const float* const* input, int inputs, float* const* output, int outputs,
        int samples, const juce::AudioIODeviceCallbackContext&) override
    {
        for (int channel = 0; channel < outputs; ++channel)
            if (output[channel]) juce::FloatVectorOperations::clear(output[channel], samples);
        if (inputs > 0) analysis_.push(input[0], inputs > 1 ? input[1] : nullptr, samples);
    }
private:
    AudioAnalysis& analysis_;
};
class Window final : public juce::DocumentWindow
{
public:
    Window(Processor& processor, std::function<void()> quit, std::function<void()> audio)
        : DocumentWindow("GRID-ARVx", juce::Colour(0xff151a20), allButtons), quit_(std::move(quit))
    {
        setUsingNativeTitleBar(true);
        auto* editor = new Editor(processor); editor->setAudioSettingsAction(std::move(audio));
        setContentOwned(editor, true); setResizable(true, false); setResizeLimits(760, 600, 2400, 1600);
        centreWithSize(1060, 680); setVisible(true);
    }
    void closeButtonPressed() override { quit_(); }
private:
    std::function<void()> quit_;
};
class Application final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "GRID-ARVx"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String&) override
    {
        directory_ = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("KoneLabie").getChildFile("GRID-ARVx"); directory_.createDirectory();
        processor_ = std::make_unique<Processor>();
        juce::MemoryBlock state;
        if (directory_.getChildFile("state.bin").loadFileAsData(state))
            processor_->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        input_ = std::make_unique<Input>(processor_->analysis);
        window_ = std::make_unique<Window>(*processor_, [this] { systemRequestedQuit(); }, [this] { settings(); });
        if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
            && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio))
            juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio, [this](bool allowed) { if (allowed) openAudio(); });
        else openAudio();
    }
    void shutdown() override
    {
        if (input_) devices_.removeAudioCallback(input_.get());
        if (auto xml = devices_.createStateXml()) xml->writeTo(directory_.getChildFile("audio.xml"));
        devices_.closeAudioDevice();
        if (processor_)
        { juce::MemoryBlock state; processor_->getStateInformation(state);
          directory_.getChildFile("state.bin").replaceWithData(state.getData(), state.getSize()); }
        window_.reset(); input_.reset(); processor_.reset();
    }
private:
    void openAudio()
    {
        const auto xml = juce::XmlDocument::parse(directory_.getChildFile("audio.xml"));
        const auto error = devices_.initialise(2, 0, xml.get(), true); devices_.addAudioCallback(input_.get());
        if (error.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Audio input", error + "\nChoose an input in Audio input settings.");
    }
    void settings()
    {
        juce::DialogWindow::LaunchOptions dialog;
        auto* selector = new juce::AudioDeviceSelectorComponent(devices_, 0, 2, 0, 0, false, false, true, false);
        selector->setSize(500, 400); dialog.content.setOwned(selector);
        dialog.dialogTitle = "GRID-ARVx audio input"; dialog.componentToCentreAround = window_.get();
        dialog.dialogBackgroundColour = juce::Colour(0xff151a20); dialog.useNativeTitleBar = true;
        dialog.escapeKeyTriggersCloseButton = true; dialog.launchAsync();
    }
    juce::File directory_; juce::AudioDeviceManager devices_;
    std::unique_ptr<Processor> processor_; std::unique_ptr<Input> input_; std::unique_ptr<Window> window_;
};
}
START_JUCE_APPLICATION(kone::grid::Application)
