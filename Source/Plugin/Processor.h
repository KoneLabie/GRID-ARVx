#pragma once
#include "Core/AudioAnalysis.h"
#include "Core/Controls.h"
#include "Core/PerformanceMappings.h"
#include <juce_audio_processors/juce_audio_processors.h>
namespace kone::grid {
#if GRID_USE_VEL_ENGINE
class VelEngine;
#endif
class Processor final : public juce::AudioProcessor {
public:
  Processor();
  void prepareToPlay(double r, int) override { setInputSampleRate(r); }
  void setInputSampleRate(double);
  void analyzeInput(const float *, const float *, int);
#if GRID_USE_VEL_ENGINE
  std::atomic<std::shared_ptr<VelEngine>> velEngine;
#endif
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout &) const override;
  using AudioProcessor::processBlock;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override { return true; }
  const juce::String getName() const override { return "GRID-ARVx"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String &) override {}
  void getStateInformation(juce::MemoryBlock &) override;
  void setStateInformation(const void *, int) override;
  bool restoreState(const void *, int);
  juce::Colour visualColour(const char*, bool background=false) const;
  juce::String getAssetProperty(const char *) const;
  void setAssetProperty(const char *, const juce::String &);
  int value(const char *) const;
  void set(const char *, int);
  void receiveMidi(const juce::MidiMessage &) noexcept;
  bool bind(int, int, Binding, bool note = false);
  void clearBindings();
  juce::var bindingState() const;
  void restoreBindings(const juce::var &);
  struct PerformanceBindingLabels {
    std::array<juce::String,performanceActions.size()> keyboard,midi;
  };
  PerformanceBindingLabels performanceBindingLabels() const;
  juce::KeyPress performanceKey(size_t) const;
  void setPerformanceKey(size_t,const juce::KeyPress &);
  void bindPerformanceMidi(size_t,int channel,int control,bool note);
  juce::String performanceMidiLabel(size_t) const;
  MidiEvents performanceEvents;
  void selectWorkspace(Workspace);
  Workspace workspace() const { return activeWorkspace.load(); }
  std::atomic<bool> performance{false}, learning{false};
  std::atomic<double> hostBpm{0};
  MidiEvents midiEvents;
  MidiEvents mappedEvents;
  MidiEvents appliedMidi;
  juce::AudioProcessorValueTreeState state;
  AudioAnalysis analysis;

private:
  mutable juce::CriticalSection assetLock;
  juce::NamedValueSet assets;
  std::atomic<double> inputSampleRate{48000};
  std::array<juce::RangedAudioParameter *, controls.size()> parameters{};
  std::array<std::array<std::atomic<int>, 4096>, 3> bindings{};
  std::array<std::array<std::atomic<bool>, 4096>, 3> held{};
  std::array<std::atomic<int>,4096> performanceMidi{};
  std::array<std::atomic<bool>,4096> performanceMidiDown{};
  std::atomic<Workspace> activeWorkspace{Workspace::lab};
  std::array<std::array<int, controls.size()>, 3> workspaceValues{};
  std::atomic<bool> switchingWorkspace{false};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Processor)
};
} // namespace kone::grid
