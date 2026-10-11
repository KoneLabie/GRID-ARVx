#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/AudioAnalysis.h"

namespace kone::grid
{
class Processor final : public juce::AudioProcessor
{
public:
    Processor();
    void prepareToPlay(double rate, int) override { analysis.setSampleRate(rate); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "GRID-ARVx"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::String getAssetProperty(const char* name) const;
    void setAssetProperty(const char* name, const juce::String& value);
    juce::AudioProcessorValueTreeState state;
    AudioAnalysis analysis;
private:
    mutable juce::CriticalSection assetsLock_;
    juce::NamedValueSet assetProperties_;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
};
}
