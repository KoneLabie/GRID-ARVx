#pragma once
#include <juce_core/juce_core.h>
namespace kone::grid {
inline bool validLyrics(const juce::var &value) {
  auto *lines = value.getArray();
  constexpr int maximumLines =
#if GRID_USE_VEL_ENGINE
      75;
#else
      128;
#endif
  if (!lines || lines->isEmpty() || lines->size() > maximumLines)
    return false;
  for (const auto &line : *lines)
    if (!line.isString() || line.toString().length() > 120)
      return false;
  return true;
}
} // namespace kone::grid
