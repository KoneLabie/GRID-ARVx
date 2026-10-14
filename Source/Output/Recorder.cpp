#include "Recorder.h"
#include <cstring>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
extern char **environ;
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace kone::grid {
namespace {
class Pipe {
public:
#if JUCE_WINDOWS
  HANDLE input = nullptr, process = nullptr;
  static juce::String quote(const juce::String &s) {
    juce::String out = "\"";
    int slashes = 0;
    for (auto ch : s) {
      if (ch == '\\') {
        ++slashes;
        continue;
      }
      if (ch == '\"')
        out += juce::String::repeatedString("\\", slashes * 2 + 1);
      else
        out += juce::String::repeatedString("\\", slashes);
      out += juce::String::charToString(ch);
      slashes = 0;
    }
    return out + juce::String::repeatedString("\\", slashes * 2) + "\"";
  }
#else
  int input = -1;
  pid_t process = -1;
#endif
  bool start(const juce::StringArray &args, const juce::File &log) {
#if JUCE_WINDOWS
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE readPipe = nullptr;
    if (!CreatePipe(&readPipe, &input, &security, 0))
      return false;
    SetHandleInformation(input, HANDLE_FLAG_INHERIT, 0);
    auto logPath = log.getFullPathName().toWideCharPointer();
    HANDLE errors =
        CreateFileW(logPath, GENERIC_WRITE, FILE_SHARE_READ, &security,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = readPipe;
    startup.hStdOutput = errors;
    startup.hStdError = errors;
    PROCESS_INFORMATION info{};
    juce::String command;
    for (auto a : args)
      command += quote(a) + " ";
    std::wstring buffer(command.toWideCharPointer());
    bool ok = CreateProcessW(args[0].toWideCharPointer(), buffer.data(),
                             nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                             nullptr, &startup, &info) != 0;
    CloseHandle(readPipe);
    if (errors != INVALID_HANDLE_VALUE)
      CloseHandle(errors);
    if (!ok) {
      CloseHandle(input);
      input = nullptr;
      return false;
    }
    CloseHandle(info.hThread);
    process = info.hProcess;
    return true;
#else
    int pipes[2];
    if (pipe(pipes) != 0)
      return false;
    int logFd = open(log.getFullPathName().toRawUTF8(),
                     O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (logFd < 0) {
      close(pipes[0]);
      close(pipes[1]);
      return false;
    }
    std::vector<std::string> storage;
    for (auto a : args)
      storage.emplace_back(a.toStdString());
    std::vector<char *> argv;
    for (auto &arg : storage)
      argv.push_back(arg.data());
    argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipes[0], 0);
    posix_spawn_file_actions_adddup2(&actions, logFd, 1);
    posix_spawn_file_actions_adddup2(&actions, logFd, 2);
    posix_spawn_file_actions_addclose(&actions, pipes[1]);
    posix_spawn_file_actions_addclose(&actions, pipes[0]);
    posix_spawn_file_actions_addclose(&actions, logFd);
    int result =
        posix_spawn(&process, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(logFd);
    close(pipes[0]);
    input = pipes[1];
    if (result != 0) {
      close(input);
      input = -1;
      process = -1;
      return false;
    }
    return true;
#endif
  }
  bool write(const unsigned char *data, size_t size) {
#if JUCE_WINDOWS
    while (size) {
      DWORD count = 0;
      if (!WriteFile(input, data, (DWORD)std::min<size_t>(size, 1048576),
                     &count, nullptr) ||
          !count)
        return false;
      size -= count;
      data += count;
    }
    return true;
#else
    // Mask SIGPIPE only on this encoder worker, leaving the DAW's signal policy
    // untouched.
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &blocked, nullptr);
    while (size) {
      auto count = ::write(input, data, size);
      if (count < 0 && errno == EINTR)
        continue;
      if (count <= 0)
        return false;
      size -= (size_t)count;
      data += count;
    }
    return true;
#endif
  }
  int finish() {
#if JUCE_WINDOWS
    if (input) {
      CloseHandle(input);
      input = nullptr;
    }
    if (!process)
      return -1;
    if (WaitForSingleObject(process, 30000) != WAIT_OBJECT_0) {
      TerminateProcess(process, 1);
      WaitForSingleObject(process, 1000);
    }
    DWORD code = 1;
    GetExitCodeProcess(process, &code);
    CloseHandle(process);
    process = nullptr;
    return (int)code;
#else
    if (input >= 0) {
      close(input);
      input = -1;
    }
    int status = 0;
    bool reaped = false;
    if (process > 0) {
      for (int i = 0; i < 300; ++i) {
        auto result = waitpid(process, &status, WNOHANG);
        if (result == process) {
          reaped = true;
          break;
        }
        if (result < 0 && errno != EINTR)
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      if (!reaped) {
        kill(process, SIGKILL);
        while (waitpid(process, &status, 0) < 0 && errno == EINTR) {
        }
      }
      process = -1;
    }
    return reaped && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
  }
  ~Pipe() { finish(); }
};
} // namespace
juce::File findEncoder(const juce::String &preferred) {
  if (preferred.isNotEmpty() && juce::File(preferred).existsAsFile())
    return juce::File(preferred);
  auto app = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
  const juce::String binary = JUCE_WINDOWS ? "ffmpeg.exe" : "ffmpeg";
  juce::Array<juce::File> candidates;
  candidates.add(app.getParentDirectory().getChildFile("tools/" + binary));
#if JUCE_WINDOWS
  auto common = juce::File(juce::SystemStats::getEnvironmentVariable(
      "CommonProgramFiles", "C:/Program Files/Common Files"));
  candidates.add(common.getChildFile("Cenaceae/GRID-ARVx/tools/ffmpeg.exe"));
  candidates.add(juce::File("C:/ffmpeg/bin/ffmpeg.exe"));
#elif JUCE_MAC
  candidates.add(app.getChildFile("Contents/Resources/tools/ffmpeg"));
  candidates.add(juce::File(
      "/Library/Application Support/Cenaceae/GRID-ARVx/tools/ffmpeg"));
  candidates.add(juce::File("/opt/homebrew/bin/ffmpeg"));
  candidates.add(juce::File("/usr/local/bin/ffmpeg"));
#else
  candidates.add(
      juce::File::getSpecialLocation(juce::File::userHomeDirectory)
          .getChildFile(".local/share/Cenaceae/GRID-ARVx/tools/ffmpeg"));
  candidates.add(juce::File("/usr/bin/ffmpeg"));
#endif
  for (auto candidate : candidates)
    if (candidate.existsAsFile())
      return candidate;
  return {};
}
juce::Image recordingFrame(const juce::Image &source, int width, int height) {
  juce::Image output(juce::Image::RGB, width, height, true,
                     juce::SoftwareImageType());
  if (!source.isValid())
    return output;
  const float scale = std::min((float)width / source.getWidth(),
                               (float)height / source.getHeight());
  const float x = (width - source.getWidth() * scale) * .5f;
  const float y = (height - source.getHeight() * scale) * .5f;
  juce::Graphics g(output);
  g.setImageResamplingQuality(juce::Graphics::mediumResamplingQuality);
  g.drawImageTransformed(source,
                         juce::AffineTransform::scale(scale, -scale)
                             .translated(x, y + source.getHeight() * scale));
  return output;
}
juce::Result Recorder::start(const RecordSettings &s) {
  if (active())
    return juce::Result::fail("Already recording.");
  stop();
  if (!s.encoder.existsAsFile())
    return juce::Result::fail(
        "Choose an FFmpeg executable in Settings before recording.");
  if (s.output.exists())
    return juce::Result::fail(
        "The output file exists. Choose another filename.");
  if (s.width < 320 || s.width > 3840 || s.height < 180 || s.height > 2160 ||
      s.fps < 12 || s.fps > 120 || s.width % 2 || s.height % 2 ||
      s.bitrate < 500 || s.bitrate > 100000 || s.seconds < 1 ||
      s.seconds > 7200)
    return juce::Result::fail("Invalid capture settings.");
  if (!s.output.getParentDirectory().createDirectory())
    return juce::Result::fail("Output folder is not writable.");
  settings = s;
  failure.clear();
  queue.clear();
  finishing = false;
  stopping = false;
  stopFrame = 0;
  dropped = 0;
  encoded = 0;
  epoch = juce::Time::getMillisecondCounterHiRes();
  running = true;
  worker = std::thread([this] { run(); });
  return juce::Result::ok();
}
void Recorder::submit(juce::Image image) {
  if (!active())
    return;
  std::unique_lock<std::mutex> l(mutex, std::try_to_lock);
  if (!l.owns_lock()) {
    ++dropped;
    return;
  }
  if (finishing || !image.isValid())
    return;
  if (queue.size() >= 4) {
    ++dropped;
    return;
  }
  int index = (int)(elapsed() * settings.fps);
  if (!queue.empty() && queue.back().index == index)
    return;
  queue.push_back({std::move(image), index});
  ready.notify_one();
}
void Recorder::requestStop() {
  {
    std::lock_guard<std::mutex> l(mutex);
    if (finishing)
      return;
    stopFrame =
        active()
            ? std::max(1, (int)std::ceil(std::clamp(elapsed(), 0.0,
                                                    (double)settings.seconds) *
                                         settings.fps))
            : 0;
    finishing = true;
    stopping = active();
  }
  ready.notify_one();
}
void Recorder::stop() {
  requestStop();
  if (worker.joinable())
    worker.join();
}
void Recorder::run() {
  juce::StringArray args{settings.encoder.getFullPathName(),
                         "-hide_banner",
                         "-loglevel",
                         "error",
                         "-n",
                         "-f",
                         "rawvideo",
                         "-pix_fmt",
                         juce::PixelRGB::indexR == 0 ? "rgb24" : "bgr24",
                         "-s",
                         juce::String(settings.width) + "x" +
                             juce::String(settings.height),
                         "-r",
                         juce::String(settings.fps),
                         "-i",
                         "pipe:0",
                         "-an",
                         "-c:v",
                         settings.nvidia ? "h264_nvenc" : "mpeg4",
                         "-b:v",
                         juce::String(settings.bitrate) + "k",
                         "-pix_fmt",
                         "yuv420p"};
  if (settings.output.hasFileExtension("mp4"))
    args.addArray({"-movflags", "+faststart"});
  args.add(settings.output.getFullPathName());
  auto log = settings.output.withFileExtension("encoder.log");
  Pipe pipe;
  if (!pipe.start(args, log)) {
    std::lock_guard<std::mutex> l(mutex);
    failure = "Unable to start the encoder.";
    running = false;
    return;
  }
  int last = -1;
  bool ok = true;
  std::vector<unsigned char> rgb((size_t)settings.width * settings.height * 3);
  std::vector<unsigned char> previous;
  while (ok) {
    Frame frame;
    {
      std::unique_lock<std::mutex> l(mutex);
      ready.wait_for(l, std::chrono::milliseconds(100),
                     [&] { return finishing || !queue.empty(); });
      if (!finishing && elapsed() >= settings.seconds) {
        finishing = true;
        stopping = true;
        stopFrame = settings.seconds * settings.fps;
      }
      if (queue.empty() && finishing)
        break;
      if (queue.empty())
        continue;
      frame = std::move(queue.front());
      queue.pop_front();
    }
    if (frame.index <= last)
      continue;
    auto image = recordingFrame(frame.image, settings.width, settings.height);
    juce::Image::BitmapData b(image, juce::Image::BitmapData::readOnly);
    // Feed the native RGB byte order directly; avoid millions of Colour
    // conversions per second. Row copies preserve padding and platform layout.
    for (int y = 0; y < settings.height; ++y)
      std::memcpy(rgb.data() + (size_t)y * settings.width * 3,
                  b.getLinePointer(y), (size_t)settings.width * 3);
    if (previous.empty())
      previous = rgb;
    int target = std::min(frame.index, settings.seconds * settings.fps - 1);
    while (last + 1 < target && ok) {
      ok = pipe.write(previous.data(), previous.size());
      ++last;
      ++encoded;
    }
    if (ok) {
      ok = pipe.write(rgb.data(), rgb.size());
      last = target;
      ++encoded;
    }
    previous = rgb;
    if (last >= settings.seconds * settings.fps - 1) {
      stopping = true;
      break;
    }
  }
  // Preserve the actual timeline when the preview produces frames below the
  // recording rate.
  int end = 0;
  {
    std::lock_guard<std::mutex> l(mutex);
    end = stopFrame;
  }
  while (ok && !previous.empty() && last + 1 < end) {
    ok = pipe.write(previous.data(), previous.size());
    ++last;
    ++encoded;
  }
  if (previous.empty())
    ok = false;
  int code = pipe.finish();
  if (!ok || code != 0) {
    std::lock_guard<std::mutex> l(mutex);
    failure = "Encoder failed. See " + log.getFullPathName();
  }
  running = false;
}
} // namespace kone::grid
