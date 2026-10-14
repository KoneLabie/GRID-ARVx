#pragma once
#include <juce_core/juce_core.h>
namespace kone::grid {
void setEngineHeldKeys(bool active,bool strobes,bool blackout);
// Stores backend preferences and native preset/setlist folders in GRID's profile.
// Activation and other engine behaviour are unchanged.
bool connectEngineFiles(void *module, const juce::File &profile);
}
