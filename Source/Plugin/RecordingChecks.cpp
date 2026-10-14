#include "Editor.h"
namespace kone::grid {
void Editor::runRecordingChecks(const juce::File &folder,
                                std::function<void()> done) {
  folder.createDirectory();
  for (auto control : controls)
    processor.set(control.id, control.initial);
  processor.setAssetProperty("imagePath", "");
  processor.setAssetProperty("fontPath", "");
  processor.setAssetProperty("presetFile", "");
  processor.setAssetProperty("presetName", "");
  processor.setAssetProperty("lyrics", "[\"RECORD TEST\"]");
  refreshAssets();
  isHome = false;
  uiHidden = false;
  updatePresentation();
  recordingCheck = true;
  // Output resolution is independent of the preview window. Keep the test
  // window within monitor 2 when the user is working on the other display.
  if(!quietDesktopChecks)setSize(1920, 1080);
  processor.set("renderResolution",encodeChoice(3,7));
  processor.set("frameCap", 56);    // 60 FPS target.
  processor.set("renderScale", 76); // Approximately 100%.
  processor.set("renderDetail", 127);
  processor.set("bloomQuality", 127);
  processor.set("adaptive", 0);
  processor.set("glow", 90);
  for (auto fx : {"texture", "dither", "track", "glass"})
    processor.set(fx, 80);
  processor.set("textureType", encodeChoice(2, 4));
  processor.set("ditherType", encodeChoice(2, 4));
  processor.set("trackType", encodeChoice(4, 5));
  processor.set("glassType", encodeChoice(1, 6));
  processor.set("category", 0);
  processor.set("variant", encodeChoice(9, 16));
  resized();
  RecordSettings captureSettings;
  captureSettings.encoder = findEncoder(
      juce::SystemStats::getEnvironmentVariable("GRID_TEST_FFMPEG", ""));
  captureSettings.output = folder.getChildFile("gpu-recording.mp4");
  captureSettings.width = 1920;
  captureSettings.height = 1080;
  captureSettings.fps = 60;
  captureSettings.bitrate = 20000;
  captureSettings.seconds = 12;
  captureSettings.nvidia =
      juce::SystemStats::getEnvironmentVariable("GRID_TEST_NVENC", "") == "1";
  const auto started = startRecording(captureSettings);
  if (started.failed()) {
    folder.getChildFile("recording-checks.txt")
        .replaceWithText("FAIL " + started.getErrorMessage());
    recordingCheck = false;
    done();
    return;
  }
  struct Progress {
    int ticks = 0;
    double last = 0, maximumGap = 0;
  };
  auto progress = std::make_shared<Progress>();
  progress->last = juce::Time::getMillisecondCounterHiRes();
  recordingCheckTick = [this, folder, captureSettings, progress, done] {
    const auto now = juce::Time::getMillisecondCounterHiRes();
    progress->maximumGap =
        juce::jmax(progress->maximumGap, now - progress->last);
    progress->last = now;
    ++progress->ticks;
    const int segment = juce::jmin(5, (int)(recorder.elapsed() / 2));
    const int modes[] = {0, 3, 6, 7, 8, 0};
    const int variants[] = {9, 0, 8, 3, 3, 1};
    processor.set("category", encodeChoice(modes[segment], 9));
    processor.set("variant", encodeChoice(variants[segment], 16));
    if (recorder.active())
      return;
    const bool passed =
        recorder.error().isEmpty() && canvas.error().isEmpty() &&
        recorder.encoded == captureSettings.fps * captureSettings.seconds &&
        captureSettings.output.getSize() > 1024 && progress->ticks > 12;
    auto report =
        juce::String(passed ? "PASS " : "FAIL ") +
        "Actual GPU recording: 1920x1080, 60 FPS, 12 seconds, four FX and "
        "bloom\n" +
        "Encoder: " +
        juce::String(captureSettings.nvidia ? "NVIDIA H.264"
                                            : "Software MPEG-4") +
        "\nEncoded frames: " + juce::String(recorder.encoded.load()) +
        "\nQueue drops: " + juce::String(recorder.dropped.load()) +
        "\nPreview ticks: " + juce::String(progress->ticks) +
        "\nMaximum timer gap (ms): " + juce::String(progress->maximumGap, 1) +
        "\nEncoder error: " + recorder.error() +
        "\nShader error: " + canvas.error();
    folder.getChildFile("recording-checks.txt").replaceWithText(report);
    recordingCheck = false;
    // Do not destroy a std::function while its own callback is executing.
    juce::Component::SafePointer<Editor> safe(this);
    juce::MessageManager::callAsync([safe, done] {
      if (safe)
        safe->recordingCheckTick = {};
      done();
    });
  };
}
} // namespace kone::grid
