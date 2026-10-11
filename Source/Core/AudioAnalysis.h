#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace kone::grid
{
struct AudioFrame
{
    float rms = 0, peak = 0, bass = 0, mid = 0, high = 0;
    std::array<float, 256> waveform {};
};

// One audio producer, one visualization consumer. The consumer owns each occupied
// slot until read_ is published. A full queue drops new analysis samples, never audio.
class AudioAnalysis
{
public:
    static constexpr std::size_t capacity = 32768;
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    void setSampleRate(double value) noexcept
    {
        if (std::isfinite(value) && value >= 8000.0 && value <= 384000.0)
            sampleRate_.store(value, std::memory_order_relaxed);
    }
    void push(const float* left, const float* right, int samples) noexcept
    {
        if (left == nullptr || samples <= 0) return;
        if (right == nullptr) right = left;
        const auto start = write_.load(std::memory_order_relaxed);
        const auto available = capacity - (start - read_.load(std::memory_order_acquire));
        const auto count = std::min<std::uint64_t>(available, static_cast<std::uint64_t>(samples));
        for (std::uint64_t i = 0; i < count; ++i)
        {
            const float l = std::isfinite(left[i]) ? left[i] : 0.0f;
            const float r = std::isfinite(right[i]) ? right[i] : 0.0f;
            queue_[(start + i) & (capacity - 1)] = std::clamp(l * 0.5f + r * 0.5f, -8.0f, 8.0f);
        }
        write_.store(start + count, std::memory_order_release);
        dropped_.fetch_add(static_cast<std::uint64_t>(samples) - count, std::memory_order_relaxed);
    }
    AudioFrame consume(float elapsedSeconds = 1.0f / 60.0f) noexcept
    {
        auto head = read_.load(std::memory_order_relaxed);
        const auto end = write_.load(std::memory_order_acquire);
        const auto count = end - head;
        const double rate = sampleRate_.load(std::memory_order_relaxed);
        const float bassAlpha = static_cast<float>(1.0 - std::exp(-6.283185307179586 * 150.0 / rate));
        const float midAlpha = static_cast<float>(1.0 - std::exp(-6.283185307179586 * 2500.0 / rate));
        double energy = 0, bassEnergy = 0, midEnergy = 0, highEnergy = 0;
        float peak = 0;
        while (head < end)
        {
            const float value = queue_[head & (capacity - 1)];
            bassFilter_ += bassAlpha * (value - bassFilter_);
            midFilter_ += midAlpha * (value - midFilter_);
            const float middle = midFilter_ - bassFilter_;
            const float high = value - midFilter_;
            energy += value * value;
            bassEnergy += bassFilter_ * bassFilter_;
            midEnergy += middle * middle;
            highEnergy += high * high;
            peak = std::max(peak, std::abs(value));
            history_[historyIndex_++ % history_.size()] = value;
            ++head;
        }
        read_.store(end, std::memory_order_release);
        const float dt = std::isfinite(elapsedSeconds) ? std::clamp(elapsedSeconds, 0.001f, 1.0f) : 0.016f;
        auto follow = [dt](float current, float target)
        {
            const float coefficient = 1.0f - std::exp(-dt * (target > current ? 45.0f : 9.0f));
            return current + coefficient * (target - current);
        };
        auto magnitude = [count](double value)
        { return count == 0 ? 0.0f : static_cast<float>(std::sqrt(value / static_cast<double>(count))); };
        frame_.rms = follow(frame_.rms, magnitude(energy));
        frame_.peak = follow(frame_.peak, peak);
        frame_.bass = follow(frame_.bass, magnitude(bassEnergy));
        frame_.mid = follow(frame_.mid, magnitude(midEnergy));
        frame_.high = follow(frame_.high, magnitude(highEnergy));
        for (std::size_t i = 0; i < frame_.waveform.size(); ++i)
            frame_.waveform[i] = count == 0 ? 0.0f : history_[(historyIndex_ + i * 4) % history_.size()];
        return frame_;
    }
    std::uint64_t droppedSamples() const noexcept { return dropped_.load(std::memory_order_relaxed); }
private:
    std::array<float, capacity> queue_ {};
    alignas(64) std::atomic<std::uint64_t> write_ {0};
    alignas(64) std::atomic<std::uint64_t> read_ {0};
    std::atomic<std::uint64_t> dropped_ {0};
    std::atomic<double> sampleRate_ {48000.0};
    std::array<float, 1024> history_ {};
    std::size_t historyIndex_ = 0;
    float bassFilter_ = 0, midFilter_ = 0;
    AudioFrame frame_;
};
}
