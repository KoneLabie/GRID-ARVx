#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace kone::grid {
// Embedded dialogs block controls behind them, but must not block their
// owning window's native Close event (JUCE checks the modal gate for WM_CLOSE).
class EmbeddedFrame : public juce::Component {
public:
  bool canModalEventBeSentToComponent(const juce::Component *component) override {
    return component==getTopLevelComponent() || dynamic_cast<const juce::DocumentWindow *>(component)!=nullptr;
  }
};
}
