#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <juce_graphics/juce_graphics.h>
#include <mutex>
#include <thread>
namespace kone::grid {
// OpenGL readback has its first scanline at the bottom. Fit without stretching.
juce::Image recordingFrame(const juce::Image &, int width, int height);
juce::File findEncoder(const juce::String &preferred = {});
struct RecordSettings {
  juce::File encoder, output;
  int width = 1280, height = 720, fps = 30, bitrate = 8000, seconds = 240;
  bool nvidia = false;
};
class Recorder {
public:
  Recorder() = default;
  ~Recorder() { stop(); }
  juce::Result start(const RecordSettings &);
  void submit(juce::Image);
  void requestStop();
  void stop();
  bool finalizing() const { return stopping.load(); }
  bool active() const { return running.load(); }
  juce::String error() const {
    std::lock_guard<std::mutex> l(mutex);
    return failure;
  }
  std::atomic<int> dropped{0}, encoded{0};
  double elapsed() const {
    return (juce::Time::getMillisecondCounterHiRes() - epoch) * .001;
  }

private:
  void run();
  RecordSettings settings;
  std::thread worker;
  mutable std::mutex mutex;
  std::condition_variable ready;
  struct Frame {
    juce::Image image;
    int index;
  };
  std::deque<Frame> queue;
  std::atomic<bool> running{false};
  std::atomic<bool> stopping{false};
  bool finishing = false;
  int stopFrame = 0;
  double epoch = 0;
  juce::String failure;
};
} // namespace kone::grid
