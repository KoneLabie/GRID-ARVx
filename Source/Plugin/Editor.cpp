#include "Editor.h"

namespace kone::grid
{
Editor::Editor(Processor& owner) : AudioProcessorEditor(owner), processor_(owner)
{
    setResizable(true, true); setResizeLimits(760, 600, 2400, 1600);
    for (auto* component : std::initializer_list<juce::Component*>{&canvas_, &title_, &status_, &modeLabel_, &gainLabel_,
            &speedLabel_, &textureLabel_, &textLabel_, &mode_, &gain_, &speed_, &texture_, &text_, &load_, &clear_})
        addAndMakeVisible(component);
    addChildComponent(audio_);
    title_.setText("GRID-ARVx  /  KONE LABIE", juce::dontSendNotification);
    title_.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, juce::Colour(0xffd6c3a1));
    modeLabel_.setText("VISUAL", juce::dontSendNotification);
    gainLabel_.setText("Sensitivity", juce::dontSendNotification);
    speedLabel_.setText("Motion", juce::dontSendNotification);
    textureLabel_.setText("Noise texture", juce::dontSendNotification);
    textLabel_.setText("TEXT OVERLAY", juce::dontSendNotification);
    mode_.addItemList({"Oscilloscope", "Basic Colors", "Plasma"}, 1);
    for (auto* slider : {&gain_, &speed_, &texture_})
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 22);
        slider->setColour(juce::Slider::thumbColourId, juce::Colour(0xff62b7aa));
    }
    modeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor_.state, "mode", mode_);
    gainAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.state, "sensitivity", gain_);
    speedAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.state, "speed", speed_);
    textureAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor_.state, "texture", texture_);
    text_.setMultiLine(true); text_.setReturnKeyStartsNewLine(true);
    text_.setTextToShowWhenEmpty("Type a title or lyrics. Drop .txt, .ttf, .otf, .png or .jpg here.", juce::Colours::grey);
    text_.onTextChange = [this] { processor_.setAssetProperty("overlayText", text_.getText()); };
    clear_.onClick = [this] { processor_.setAssetProperty("imagePath", ""); restoreAssets(); };
    load_.onClick = [this]
    {
        chooser_ = std::make_unique<juce::FileChooser>("Import visual asset", juce::File{}, "*.png;*.jpg;*.jpeg;*.ttf;*.otf;*.txt");
        juce::Component::SafePointer<Editor> safe(this);
        chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& chooser) { if (safe && chooser.getResult().existsAsFile()) safe->importAsset(chooser.getResult()); });
    };
    restoreAssets(); setSize(1060, 650); startTimerHz(60);
}
void Editor::setAudioSettingsAction(std::function<void()> action)
{ audio_.onClick = std::move(action); audio_.setVisible(true); resized(); }
void Editor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff151a20));
    graphics.setColour(juce::Colour(0xff26343a)); graphics.fillRect(0, 62, 282, getHeight() - 62);
}
void Editor::resized()
{
    title_.setBounds(20, 12, getWidth() - 40, 36);
    auto panel = juce::Rectangle<int>(18, 78, 246, getHeight() - 96);
    modeLabel_.setBounds(panel.removeFromTop(24)); mode_.setBounds(panel.removeFromTop(32)); panel.removeFromTop(18);
    for (const auto& pair : {std::pair{&gainLabel_, &gain_}, std::pair{&speedLabel_, &speed_}, std::pair{&textureLabel_, &texture_}})
    { pair.first->setBounds(panel.removeFromTop(24)); pair.second->setBounds(panel.removeFromTop(30)); panel.removeFromTop(5); }
    textLabel_.setBounds(panel.removeFromTop(24)); text_.setBounds(panel.removeFromTop(90)); panel.removeFromTop(14);
    load_.setBounds(panel.removeFromTop(30)); panel.removeFromTop(6); clear_.setBounds(panel.removeFromTop(28));
    panel.removeFromTop(6); audio_.setBounds(panel.removeFromTop(30));
    canvas_.setBounds(298, 78, std::max(1, getWidth() - 316), std::max(1, getHeight() - 124));
    status_.setBounds(298, getHeight() - 40, std::max(1, getWidth() - 316), 25);
}
void Editor::timerCallback()
{
    const auto frame = processor_.analysis.consume();
    auto value = [this](const char* id) { return processor_.state.getRawParameterValue(id)->load(); };
    canvas_.present(frame, static_cast<int>(value("mode")), value("sensitivity"), value("speed"), value("texture"));
    restoreAssets();
    status_.setText(juce::String("INPUT ") + (frame.rms > 0.001f ? "ACTIVE" : "QUIET")
        + "   |   RMS " + juce::String(frame.rms, 3) + "   |   Foundation 0.1.0", juce::dontSendNotification);
}
void Editor::restoreAssets()
{
    const auto imagePath = processor_.getAssetProperty("imagePath");
    const auto fontPath = processor_.getAssetProperty("fontPath");
    const auto content = processor_.getAssetProperty("overlayText");
    if (imagePath != imagePath_)
    {
        imagePath_ = imagePath; auto image = imagePath.isEmpty() ? juce::Image{} : juce::ImageFileFormat::loadFrom(juce::File(imagePath));
        if (image.isValid() && std::max(image.getWidth(), image.getHeight()) > 2048)
        { const auto scale = 2048.0f / static_cast<float>(std::max(image.getWidth(), image.getHeight()));
          image = image.rescaled(std::max(1, static_cast<int>(image.getWidth() * scale)), std::max(1, static_cast<int>(image.getHeight() * scale))); }
        canvas_.setImage(image);
    }
    if (fontPath != fontPath_)
    {
        fontPath_ = fontPath; juce::MemoryBlock bytes; juce::Typeface::Ptr face;
        if (fontPath.isNotEmpty() && juce::File(fontPath).loadFileAsData(bytes))
            face = juce::Typeface::createSystemTypefaceFor(bytes.getData(), bytes.getSize());
        canvas_.setTypeface(face);
    }
    if (content != textValue_)
    { textValue_ = content; text_.setText(content, false); canvas_.setText(content); }
}
bool Editor::isInterestedInFileDrag(const juce::StringArray& paths)
{
    for (const auto& path : paths) if (juce::File(path).hasFileExtension("png;jpg;jpeg;ttf;otf;txt")) return true;
    return false;
}
void Editor::filesDropped(const juce::StringArray& paths, int, int)
{ for (const auto& path : paths) importAsset(juce::File(path)); }
void Editor::importAsset(const juce::File& file)
{
    if (!file.existsAsFile() || file.getSize() > 16 * 1024 * 1024) return;
    if (file.hasFileExtension("txt"))
    { processor_.setAssetProperty("overlayText", file.loadFileAsString()); restoreAssets(); return; }
    const bool font = file.hasFileExtension("ttf;otf");
    if (!font && !file.hasFileExtension("png;jpg;jpeg")) return;
    auto directory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("KoneLabie").getChildFile("GRID-ARVx").getChildFile(font ? "Fonts" : "Images");
    if (directory.createDirectory().failed()) return;
    const auto destination = directory.getChildFile(file.getFileName()).getNonexistentSibling();
    if (!file.copyFileTo(destination)) return;
    processor_.setAssetProperty(font ? "fontPath" : "imagePath", destination.getFullPathName());
    restoreAssets();
}
}
