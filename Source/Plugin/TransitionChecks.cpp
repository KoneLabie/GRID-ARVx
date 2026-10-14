#include "Editor.h"
#if JUCE_WINDOWS
#include <windows.h>
#undef small
#endif
namespace kone::grid {
void Editor::runTransitionChecks(const juce::File &folder,std::function<void()> done) {
  folder.createDirectory();
  processor.set("renderResolution",0);processor.set("frameCap",frameRateValue(24));
  struct State {juce::String report,bindings;void *engine=nullptr;int failures=0;};
  auto state=std::make_shared<State>();state->bindings=juce::JSON::toString(processor.bindingState());
#if GRID_USE_VEL_ENGINE
  state->engine=nativeEngine.get();
#endif
  auto check=[state](bool passed,const juce::String &label) {
    state->report+=(passed?"PASS ":"FAIL ")+label+"\n";
    if(!passed)++state->failures;
  };
  auto click=[](juce::TextButton &button){if(button.onClick)button.onClick();};
  auto frameButton=[this,click,check](const juce::String &id) {
    auto *button=panel?dynamic_cast<juce::TextButton *>(panel->findChildWithID(id)):nullptr;
    check(button!=nullptr,"Live frame button exists / "+id);if(button)click(*button);
  };
  class Steps final : public juce::Timer {
  public:
    Steps(std::function<void(int)> action,std::function<void()> finish):action(std::move(action)),finish(std::move(finish)){startTimer(150);}
    void timerCallback() override {
      action(index++);
      if(index==170){stopTimer();juce::MessageManager::callAsync(finish);}
    }
  private:int index=0;std::function<void(int)> action;std::function<void()> finish;
  };
  transitionCheckTimer=std::make_unique<Steps>([this,folder,state,check,click,frameButton](int index) {
    const int phase=index%34,cycle=index/34;
    folder.getChildFile("progress.txt").replaceWithText("Cycle "+juce::String(cycle+1)+", phase "+juce::String(phase));
    switch(phase) {
      case 0:click(lab);break;
      case 1:click(popOut);break;
      case 2:click(hide);break;
      case 3:setUiHidden(false);break;
      case 4:click(popOut);break;
      case 5:click(home);break;
      case 6:click(multiple);break;
      case 7:click(workspaceAction);break;
      case 8:click(popOut);break;
      case 9:click(midi);break;
      case 10:click(midi);break;
      case 11:click(popOut);break;
      case 12:click(workspaceAction);break;
      case 13:click(home);break;
      case 14:click(performanceButton);break;
      case 15:click(midi);break;
      case 16:frameButton("performance-bindings");break;
      case 17:case 18:frameButton("performance-mapping-back");break;
      case 19:click(workspaceAction);break;
      case 20:click(popOut);break;
      case 21:case 22:case 23:click(full);break;
      case 24:click(midi);break;
      case 25:frameButton("performance-mapping-back");break;
      case 26:click(popOut);break;
      case 27:keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));break;
      case 28:click(home);break;
      case 29:click(settings);break;
      case 30:
        for(auto &widget:panelWidgets)if(auto *button=dynamic_cast<juce::TextButton *>(widget.get()))
          if(button->getButtonText().equalsIgnoreCase("ADVANCED GRAPHICS")){click(*button);break;}
        break;
      case 31:case 32:click(back);break;
      case 33:setSize(1120,740);break;
    }
    bool valid=canvas.error().isEmpty();
#if GRID_USE_VEL_ENGINE
    valid=valid && nativeEngine && nativeEngine->ready() && nativeEngine.get()==state->engine;
#endif
    valid=valid && state->bindings==juce::JSON::toString(processor.bindingState());
    if(controlsWindow && uiHidden)valid=valid && !controlsWindow->isVisible();
    if(controlsWindow && captureOverlay && !uiHidden) {
      valid=valid && controlsWindow->getBounds()==getScreenBounds();
#if JUCE_WINDOWS
      DWORD affinity=0;
      auto window=static_cast<HWND>(controlsWindow->getPeer()->getNativeHandle());
      valid=valid && GetWindowDisplayAffinity(window,&affinity) && affinity==0x11;
#endif
    }
    check(valid,"Live mode/menu/overlay transition "+juce::String(index+1));
    if(phase==33)check(isHome && !panel && !controlsWindow && windowMode==0,"Cycle returns to a clean Home state");
  },[folder,state,done]{
    folder.getChildFile("transition-checks.txt").replaceWithText(state->report);
    folder.getChildFile("result.txt").replaceWithText(state->failures?"FAIL Live transition checks":"PASS 170 live transitions across five complete mode/menu/overlay/fullscreen cycles");
    done();
  });
}
} // namespace kone::grid
