#pragma once
#include <cstdint>
#include <juce_graphics/juce_graphics.h>
namespace kone::grid {
// Theme native UI drawing only; scene rendering and native interaction stay intact.
void paintEnginePanel(void *editor,void *graphics,std::uintptr_t module,
                      void (*paint)(void *,void *),bool menu);
void paintEngineKeyboardGraph(void *editor,void *graphics,std::uintptr_t module);
void prepareEngineTextFrame(void *editor,std::uintptr_t module,void (*paint)(void *,void *));
juce::Image renderKeyboardGraphImage(void *editor,std::uintptr_t module,int width,int height);
}
