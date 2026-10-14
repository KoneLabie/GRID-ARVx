#pragma once
#include <algorithm>
#include <cstdint>
namespace kone::grid {
// Counts from enabling a cycle, independent of the other controls' phases.
class BeatCycle {
public:
  unsigned tick(bool enabled, int ccInterval, unsigned beats) {
    const auto period = (unsigned)std::clamp(ccInterval, 0, 127) + 1;
    if (!enabled) {
      reset();
      return 0;
    }
    if (!active || period != previousPeriod)
      progress = 0;
    active = true;
    previousPeriod = period;
    const auto total = progress + (std::uint64_t)beats;
    progress = total % period;
    return (unsigned)(total / period);
  }
  void reset() {
    active = false;
    progress = 0;
  }

private:
  bool active = false;
  unsigned previousPeriod = 1;
  std::uint64_t progress = 0;
};
} // namespace kone::grid
