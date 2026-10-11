#include "Processor.h"
#include "Editor.h"

namespace kone::grid
{
static juce::AudioProcessorValueTreeState::ParameterLayout parameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode", 1}, "Visual mode",
        juce::StringArray{"Oscilloscope", "Basic Colors", "Plasma"}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sensitivity", 1}, "Sensitivity",
        juce::NormalisableRange<float>{0.1f, 8.0f, 0.01f, 0.5f}, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"speed", 1}, "Motion speed", 0.0f, 3.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"texture", 1}, "Texture amount", 0.0f, 1.0f, 0.0f));
    return layout;
}
Processor::Processor() : AudioProcessor(BusesProperties()
    .withInput("Audio", juce::AudioChannelSet::stereo(), true)
    .withOutput("Audio", juce::AudioChannelSet::stereo(), true)),
    state(*this, nullptr, "GRID_ARVX_STATE", parameters()) {}
bool Processor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto channels = layout.getMainInputChannelSet();
    return (channels == juce::AudioChannelSet::mono() || channels == juce::AudioChannelSet::stereo())
        && channels == layout.getMainOutputChannelSet();
}
void Processor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals guard;
    if (audio.getNumChannels() > 0)
        analysis.push(audio.getReadPointer(0), audio.getNumChannels() > 1 ? audio.getReadPointer(1) : nullptr, audio.getNumSamples());
    // Input and output layouts match. Analysis never modifies the host's audio buffer.
}
juce::AudioProcessorEditor* Processor::createEditor() { return new Editor(*this); }
void Processor::getStateInformation(juce::MemoryBlock& block)
{
    auto snapshot = state.copyState();
    { const juce::ScopedLock lock(assetsLock_);
      for (int i = 0; i < assetProperties_.size(); ++i)
          snapshot.setProperty(assetProperties_.getName(i), assetProperties_.getValueAt(i), nullptr); }
    if (const auto xml = snapshot.createXml()) copyXmlToBinary(*xml, block);
}
void Processor::setStateInformation(const void* data, int size)
{
    const auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName("GRID_ARVX_STATE"))
    {
        const auto restored = juce::ValueTree::fromXml(*xml); state.replaceState(restored);
        const juce::ScopedLock lock(assetsLock_);
        for (const auto* name : {"imagePath", "fontPath", "overlayText"})
            assetProperties_.set(name, restored.getProperty(name));
    }
}
juce::String Processor::getAssetProperty(const char* name) const
{ const juce::ScopedLock lock(assetsLock_); return assetProperties_[name].toString(); }
void Processor::setAssetProperty(const char* name, const juce::String& value)
{ const juce::ScopedLock lock(assetsLock_); assetProperties_.set(name, value); }
}
#ifndef GRID_STANDALONE
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new kone::grid::Processor; }
#endif
