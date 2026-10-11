#include "Core/AudioAnalysis.h"
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using kone::grid::AudioAnalysis;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void feed(AudioAnalysis& analysis, float frequency, double sampleRate, int blocks = 12)
{
    analysis.setSampleRate(sampleRate);
    std::array<float, 1024> signal;
    for (int block = 0; block < blocks; ++block)
    {
        for (std::size_t i = 0; i < signal.size(); ++i)
            signal[i] = 0.5f * std::sin(static_cast<float>((block * 1024 + i) * 6.283185307179586 * frequency / sampleRate));
        analysis.push(signal.data(), nullptr, static_cast<int>(signal.size())); analysis.consume();
    }
}
int main()
{
    try
    {
        // Each fixture contains a 128 KiB queue; keep them off Windows' 1 MiB stack.
        auto analysis = std::make_unique<AudioAnalysis>();
        require(analysis->consume().rms == 0, "empty input must be silent");
        std::cout << "PASS 1: empty input\n";
        feed(*analysis, 1000, 48000); auto frame = analysis->consume();
        require(frame.rms > 0.25f && frame.rms < 0.4f, "sine RMS out of expected range");
        std::cout << "PASS 2: sine amplitude\n";
        std::array<float, 2048> mono; mono.fill(0.25f); auto leftOnly = std::make_unique<AudioAnalysis>();
        auto stereo = std::make_unique<AudioAnalysis>();
        leftOnly->push(mono.data(), nullptr, 2048); stereo->push(mono.data(), mono.data(), 2048);
        require(std::abs(leftOnly->consume().rms - stereo->consume().rms) < 0.00001f, "mono duplication differs");
        std::cout << "PASS 3: mono/stereo consistency\n";
        analysis->push(nullptr, nullptr, 128); analysis->push(mono.data(), nullptr, -1); analysis->push(mono.data(), nullptr, 0);
        require(analysis->droppedSamples() == 0, "invalid input changed queue");
        std::cout << "PASS 4: invalid callback input\n";
        std::vector<float> large(AudioAnalysis::capacity + 100, 0.1f); auto bounded = std::make_unique<AudioAnalysis>();
        bounded->push(large.data(), nullptr, static_cast<int>(large.size()));
        require(bounded->droppedSamples() == 100, "overflow was not bounded"); bounded->consume();
        bounded->push(mono.data(), nullptr, 2048); require(bounded->consume().rms > 0, "queue failed to recover");
        std::cout << "PASS 5: overflow and recovery\n";
        float bad[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
        auto finite = std::make_unique<AudioAnalysis>(); finite->push(bad, nullptr, 2); require(std::isfinite(finite->consume().rms), "nonfinite sample escaped");
        std::cout << "PASS 6: nonfinite input\n";
        for (int i = 0; i < 120; ++i) frame = analysis->consume(); require(frame.rms < 0.0001f, "silence envelope did not decay");
        std::cout << "PASS 7: silence decay\n";
        auto bass = std::make_unique<AudioAnalysis>();
        auto high = std::make_unique<AudioAnalysis>(); feed(*bass, 60, 48000); feed(*high, 10000, 48000);
        auto lowFrame = bass->consume(), highFrame = high->consume();
        require(lowFrame.bass > lowFrame.high && highFrame.high > highFrame.bass, "frequency bands do not separate");
        std::cout << "PASS 8: bass/high band response\n";
        for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        { auto a = std::make_unique<AudioAnalysis>(); feed(*a, 1000, rate); require(std::isfinite(a->consume().rms), "sample rate failure"); }
        std::cout << "PASS 9: common sample rates\n";
        auto concurrent = std::make_unique<AudioAnalysis>(); std::atomic<bool> finished {false};
        std::thread writer([&] { for (int i = 0; i < 3000; ++i) concurrent->push(mono.data(), nullptr, 2048); finished.store(true); });
        while (!finished.load()) require(std::isfinite(concurrent->consume().rms), "concurrent sample failure");
        writer.join(); require(std::isfinite(concurrent->consume().rms), "concurrent final failure");
        std::cout << "PASS 10: concurrent producer/consumer stress\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
