#pragma once
#include "Processor.h"
#include "Visual/Canvas.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace kone::grid
{
class Editor final : public juce::AudioProcessorEditor, private juce::Timer,
                     public juce::FileDragAndDropTarget
{
public:
    explicit Editor(Processor&);
    ~Editor() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
    void setAudioSettingsAction(std::function<void()> action);
private:
    void timerCallback() override;
    void importAsset(const juce::File&);
    void restoreAssets();
    Processor& processor_;
    Canvas canvas_;
    juce::Label title_, status_, modeLabel_, gainLabel_, speedLabel_, textureLabel_, textLabel_;
    juce::ComboBox mode_;
    juce::Slider gain_, speed_, texture_;
    juce::TextEditor text_;
    juce::TextButton load_{"Import image / font / text"}, clear_{"Clear image"}, audio_{"Audio input settings"};
    std::unique_ptr<juce::FileChooser> chooser_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment_, speedAttachment_, textureAttachment_;
    juce::String imagePath_, fontPath_, textValue_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};
}
