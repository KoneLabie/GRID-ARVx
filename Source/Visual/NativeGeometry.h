#pragma once
#include "Scenes.h"

namespace kone::grid {
// Spatial equations recovered from the isolated Visual Lab Pro renderer.
// Coordinates are in render pixels, before the shared image FX pipeline.
bool drawNativeGeometry(juce::Graphics&, const SceneState&);
}
