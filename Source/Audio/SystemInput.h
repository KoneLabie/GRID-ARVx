#pragma once
#include "Plugin/Processor.h"
namespace kone::grid {
class SystemInput final : private juce::Thread {
  public:
    explicit SystemInput(Processor &p) : Thread("GRID system audio"), processor(p) {}
    ~SystemInput() override { stopThread(3000); }
    struct PlaybackDevice { juce::String id, name; };
    static juce::Array<PlaybackDevice> playbackDevices();
    juce::Result open(const juce::String &deviceId = {});
    void close() { stopThread(3000); }
    std::function<void(int)> midiPoll;
    std::atomic<juce::uint64> capturedFrames{0};

  private:
    void run() override;
    bool captureEndpoint();
    Processor &processor;
    juce::WaitableEvent initialized;
    juce::CriticalSection statusLock;
    juce::String failure;
    juce::String endpointId;
};
} // namespace kone::grid
