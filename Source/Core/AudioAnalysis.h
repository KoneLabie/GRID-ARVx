#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace kone::grid {
struct AudioFrame {
  float rms = 0, peak = 0, bass = 0, mid = 0, high = 0;
  float averageMagnitude = 0;
  unsigned beats = 0; // Audio onsets since the previous consume, using sample time.
  std::array<float, 256> waveform{};
  std::array<float, 256> left{}, right{};
  std::array<float, 2048> scope{};
};

// One audio producer, one visualization consumer. The consumer owns each
// occupied slot until read_ is published. A full queue drops new analysis
// samples, never audio.
class AudioAnalysis {
public:
  static constexpr std::size_t capacity = 32768;
  static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
  void setSampleRate(double value) noexcept {
    if (std::isfinite(value) && value >= 8000.0 && value <= 384000.0)
      sampleRate_.store(value, std::memory_order_relaxed);
  }
  // Call only after stopping the old producer, before starting a new source.
  void reset() noexcept {
    read_.store(0); write_.store(0); dropped_.store(0);
    history_={};historyIndex_=0;bassFilter_={};midFilter_={};frame_={};
    beatSamples_=0;beatEnergy_=0;beatBackground_=0;beatCooldown_=0;beatArmed_=true;
  }
  void push(const float *left, const float *right, int samples) noexcept {
    if (left == nullptr || samples <= 0)
      return;
    if (right == nullptr)
      right = left;
    const auto start = write_.load(std::memory_order_relaxed);
    const auto available =
        capacity - (start - read_.load(std::memory_order_acquire));
    const auto count =
        std::min<std::uint64_t>(available, static_cast<std::uint64_t>(samples));
    for (std::uint64_t i = 0; i < count; ++i) {
      const float l = std::isfinite(left[i]) ? left[i] : 0.0f;
      const float r = std::isfinite(right[i]) ? right[i] : 0.0f;
      queue_[(start + i) & (capacity - 1)] = {std::clamp(l, -8.0f, 8.0f),
                                              std::clamp(r, -8.0f, 8.0f)};
    }
    write_.store(start + count, std::memory_order_release);
    dropped_.fetch_add(static_cast<std::uint64_t>(samples) - count,
                       std::memory_order_relaxed);
  }
  AudioFrame consume(float elapsedSeconds = 1.0f / 60.0f) noexcept {
    auto head = read_.load(std::memory_order_relaxed);
    const auto end = write_.load(std::memory_order_acquire);
    const auto count = end - head;
    const double rate = sampleRate_.load(std::memory_order_relaxed);
    const float bassAlpha =
        static_cast<float>(1.0 - std::exp(-6.283185307179586 * 150.0 / rate));
    const float midAlpha =
        static_cast<float>(1.0 - std::exp(-6.283185307179586 * 2500.0 / rate));
    double energy = 0, bassEnergy = 0, midEnergy = 0, highEnergy = 0, absolute = 0;
    float peak = 0;
    frame_.beats=0;
    const auto beatHop=std::max(1,(int)std::lround(rate*.01));
    while (head < end) {
      const auto sample = queue_[head & (capacity - 1)];
      double onsetEnergy=0;
      const float channels[] = {sample.left, sample.right};
      for (int channel = 0; channel < 2; ++channel) {
        const auto value = channels[channel];
        bassFilter_[channel] += bassAlpha * (value - bassFilter_[channel]);
        midFilter_[channel] += midAlpha * (value - midFilter_[channel]);
        const float middle = midFilter_[channel] - bassFilter_[channel];
        const float high = value - midFilter_[channel];
        onsetEnergy += .4*bassFilter_[channel]*bassFilter_[channel] + .1*value*value;
        energy += value * value * .5;
        absolute += std::abs(value) * .5;
        bassEnergy += bassFilter_[channel] * bassFilter_[channel] * .5;
        midEnergy += middle * middle * .5;
        highEnergy += high * high * .5;
        peak = std::max(peak, std::abs(value));
      }
      beatEnergy_+=onsetEnergy;
      if(++beatSamples_>=beatHop) {
        const double level=std::sqrt(beatEnergy_/beatSamples_);
        const double threshold=std::max(.0005,beatBackground_*1.65);
        beatCooldown_=std::max(0.0,beatCooldown_-beatSamples_/rate);
        if(level<std::max(.0003,beatBackground_*1.1))beatArmed_=true;
        if(beatArmed_ && beatCooldown_==0 && level>threshold) {
          ++frame_.beats;beatCooldown_=.18;beatArmed_=false;
        }
        beatBackground_+=(level-beatBackground_)*(1-std::exp(-beatSamples_/rate/.5));
        beatSamples_=0;beatEnergy_=0;
      }
      history_[historyIndex_++ % history_.size()] = sample;
      ++head;
    }
    read_.store(end, std::memory_order_release);
    const float dt = std::isfinite(elapsedSeconds)
                         ? std::clamp(elapsedSeconds, 0.001f, 1.0f)
                         : 0.016f;
    auto follow = [dt](float current, float target) {
      const float coefficient =
          1.0f - std::exp(-dt * (target > current ? 45.0f : 9.0f));
      return current + coefficient * (target - current);
    };
    auto magnitude = [count](double value) {
      return count == 0 ? 0.0f
                        : static_cast<float>(
                              std::sqrt(value / static_cast<double>(count)));
    };
    frame_.rms = follow(frame_.rms, magnitude(energy));
    frame_.averageMagnitude = follow(frame_.averageMagnitude, count ? (float)(absolute/count) : 0.0f);
    frame_.peak = follow(frame_.peak, peak);
    frame_.bass = follow(frame_.bass, magnitude(bassEnergy));
    frame_.mid = follow(frame_.mid, magnitude(midEnergy));
    frame_.high = follow(frame_.high, magnitude(highEnergy));
    for (std::size_t i = 0; i < frame_.waveform.size(); ++i) {
      const auto sample = history_[(historyIndex_ + history_.size()-1024 + i * 4) % history_.size()];
      frame_.left[i] =
          count == 0 ? frame_.left[i] * std::exp(-dt * 9.0f) : sample.left;
      frame_.right[i] =
          count == 0 ? frame_.right[i] * std::exp(-dt * 9.0f) : sample.right;
      frame_.waveform[i] = (frame_.left[i] + frame_.right[i]) * .5f;
    }
    for (std::size_t i=0;i<frame_.scope.size();++i) {
      const auto sample=history_[(historyIndex_+i)%history_.size()];
      frame_.scope[i]=count==0?frame_.scope[i]*std::exp(-dt*9): (sample.left+sample.right)*.5f;
    }
    return frame_;
  }
  std::uint64_t droppedSamples() const noexcept {
    return dropped_.load(std::memory_order_relaxed);
  }

private:
  struct StereoSample {
    float left = 0, right = 0;
  };
  std::array<StereoSample, capacity> queue_{};
  alignas(64) std::atomic<std::uint64_t> write_{0};
  alignas(64) std::atomic<std::uint64_t> read_{0};
  std::atomic<std::uint64_t> dropped_{0};
  std::atomic<double> sampleRate_{48000.0};
  std::array<StereoSample, 2048> history_{};
  std::size_t historyIndex_ = 0;
  std::array<float, 2> bassFilter_{}, midFilter_{};
  AudioFrame frame_;
  int beatSamples_=0;
  double beatEnergy_=0,beatBackground_=0,beatCooldown_=0;
  bool beatArmed_=true;
};
} // namespace kone::grid
