#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
namespace kone::grid {
struct PerformanceAction {
  const char *id,*name;
  int key,modifiers=0;
  bool held=false;
};
// Native Performance shortcuts, including the actions shown in its key map.
inline const std::array performanceActions{
  PerformanceAction{"track1","Track 1",'1'},PerformanceAction{"track2","Track 2",'2'},
  PerformanceAction{"track3","Track 3",'3'},PerformanceAction{"track4","Track 4",'4'},
  PerformanceAction{"track5","Track 5",'5'},PerformanceAction{"track6","Track 6",'6'},
  PerformanceAction{"track7","Track 7",'7'},PerformanceAction{"track8","Track 8",'8'},
  PerformanceAction{"track9","Track 9",'9'},PerformanceAction{"track10","Track 10",'0'},
  PerformanceAction{"previous","Previous preset",'Z'},PerformanceAction{"next","Next preset",juce::KeyPress::spaceKey},
  PerformanceAction{"glassToggle","Toggle Glass",'Q'},PerformanceAction{"ditherToggle","Toggle Dither",'W'},
  PerformanceAction{"textureToggle","Toggle Texture",'E'},PerformanceAction{"trackToggle","Toggle Track",'R'},
  PerformanceAction{"glassType","Next Glass type",'A'},PerformanceAction{"ditherType","Next Dither type",'S'},
  PerformanceAction{"textureType","Next Texture type",'D'},PerformanceAction{"trackType","Next Track type",'F'},
  PerformanceAction{"color","Next color",'C'},PerformanceAction{"colorAuto","Auto color",'C',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"mode","Next mode preset",'V'},PerformanceAction{"modeAuto","Auto mode preset",'V',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"strobes","Strobes (hold)",'X',0,true},PerformanceAction{"blackout","Blackout (hold)",'B',0,true},
  PerformanceAction{"textCycle","Cycle text",'T'},PerformanceAction{"textPrevious","Previous text line",juce::KeyPress::upKey},
  PerformanceAction{"textNext","Next text line",juce::KeyPress::downKey},PerformanceAction{"freestyle","Freestyle",juce::KeyPress::tabKey},
  PerformanceAction{"autoQueue","Auto cue queue",0x11},PerformanceAction{"back","Back to setlist",juce::KeyPress::escapeKey},
  PerformanceAction{"track11","Track 11",'1',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"track12","Track 12",'2',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"track13","Track 13",'3',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"track14","Track 14",'4',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"track15","Track 15",'5',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"track16","Track 16",'6',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"track17","Track 17",'7',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"track18","Track 18",'8',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"track19","Track 19",'9',juce::ModifierKeys::shiftModifier},
  PerformanceAction{"track20","Track 20",'0',juce::ModifierKeys::shiftModifier}};
inline juce::KeyPress defaultPerformanceKey(size_t action) {
  const auto &entry=performanceActions[action];return {entry.key,entry.modifiers,0};
}
inline bool samePerformanceKey(const juce::KeyPress &a,const juce::KeyPress &b) {
  const auto normal=[](int code){return code>='a' && code<='z'?code-'a'+'A':code;};
  return normal(a.getKeyCode())==normal(b.getKeyCode()) &&
    (a.getModifiers().getRawFlags()&juce::ModifierKeys::allKeyboardModifiers)==
    (b.getModifiers().getRawFlags()&juce::ModifierKeys::allKeyboardModifiers);
}
inline juce::String performanceKeyLabel(const juce::KeyPress &key) {
  if(!key.isValid())return "Unbound";
  if(key.getKeyCode()==0x11)return "Ctrl";
  return key.getTextDescription();
}
} // namespace kone::grid
