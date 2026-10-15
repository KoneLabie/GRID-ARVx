#include "Plugin/Processor.h"
#include "Plugin/EmbeddedFrame.h"
#include <iostream>
using namespace kone::grid;
juce::AudioProcessorEditor *Processor::createEditor(){return nullptr;}
int main() {
  juce::ScopedJuceInitialiser_GUI initialise;
  int failures=0;
  const auto check=[&](bool value,const char *name){std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n';if(!value)++failures;};
  {
    juce::Component window,behind;
    EmbeddedFrame dialog;
    window.addAndMakeVisible(behind);window.addAndMakeVisible(dialog);
    dialog.enterModalState(false,nullptr,false);
    check(!window.isCurrentlyBlockedByAnotherModalComponent(),"embedded frame leaves window Close available");
    juce::DocumentWindow output("Output",juce::Colours::black,0,false);
    check(!output.isCurrentlyBlockedByAnotherModalComponent(),"embedded frame permits other app window Close");
    check(behind.isCurrentlyBlockedByAnotherModalComponent(),"embedded frame still blocks controls behind it");
    dialog.exitModalState(0);
    check(!behind.isCurrentlyBlockedByAnotherModalComponent(),"closing frame restores underlying controls");
  }
  Processor processor;
  juce::MemoryBlock autoSaved;
  processor.setMappingChangedCallback([&] { processor.getStateInformation(autoSaved); });
  const auto remapped=juce::KeyPress('K',juce::ModifierKeys::shiftModifier,0);
  processor.setPerformanceKey(11,remapped);
  check(samePerformanceKey(processor.performanceKey(11),remapped),"keyboard remapping");
  processor.setPerformanceKey(10,remapped);
  check(!processor.performanceKey(11).isValid(),"duplicate shortcut unbinds its previous owner");
  check(samePerformanceKey(processor.performanceKey(0),defaultPerformanceKey(0)),"unmodified shortcut retained");
  check(samePerformanceKey(processor.performanceKey(32),juce::KeyPress('1',juce::ModifierKeys::shiftModifier,0)),"original Shift track shortcuts retained");
  processor.setPerformanceKey(11,defaultPerformanceKey(11));
  processor.bind(2,19,{indexOf("int"),-1,Hardware::knob}); // Lab bank.
  processor.selectWorkspace(Workspace::multiple);
  processor.bind(2,19,{indexOf("glow"),-1,Hardware::knob});
  processor.selectWorkspace(Workspace::performance);
  processor.bindPerformanceMidi(11,2,19,false);
  MidiEvent event;
  processor.receiveMidi(juce::MidiMessage::controllerEvent(2,19,64));
  check(!processor.performanceEvents.pop(event),"MIDI knob values rejected for actions");
  processor.receiveMidi(juce::MidiMessage::controllerEvent(2,19,127));
  check(processor.performanceEvents.pop(event) && event.cc==11 && event.value==127,"MIDI press invokes correct action");
  processor.receiveMidi(juce::MidiMessage::controllerEvent(2,19,127));
  check(!processor.performanceEvents.pop(event),"held button does not repeat");
  processor.receiveMidi(juce::MidiMessage::controllerEvent(2,19,0));
  check(!processor.performanceEvents.pop(event),"tap action ignores release");
  processor.bindPerformanceMidi(24,2,19,false);
  check(processor.performanceMidiLabel(11)=="Unbound","physical control reassignment clears previous action");
  processor.receiveMidi(juce::MidiMessage::controllerEvent(2,19,127));
  check(processor.performanceEvents.pop(event) && event.cc==24 && event.value==127,"held action press delivered");
  processor.receiveMidi(juce::MidiMessage::controllerEvent(2,19,0));
  check(processor.performanceEvents.pop(event) && event.cc==24 && event.value==0,"held action release delivered");
  const auto labels=processor.performanceBindingLabels();
  bool labelsMatch=true;
  for(size_t i=0;i<performanceActions.size();++i)
    labelsMatch &= labels.keyboard[i]==performanceKeyLabel(processor.performanceKey(i)) && labels.midi[i]==processor.performanceMidiLabel(i);
  check(labelsMatch,"bulk binding snapshot matches every remapped keyboard and MIDI cell");
  auto automaticallyRestored = std::make_unique<Processor>();
  check(automaticallyRestored->restoreState(autoSaved.getData(),(int)autoSaved.getSize()) &&
            automaticallyRestored->performanceMidiLabel(24)=="Ch 2 CC 19",
        "MIDI mapping changes immediately produce restorable state");
  juce::MemoryBlock state;processor.getStateInformation(state);
  Processor restored;
  check(restored.restoreState(state.getData(),(int)state.getSize()),"state restored");
  check(samePerformanceKey(restored.performanceKey(10),remapped) && restored.performanceMidiLabel(24)=="Ch 2 CC 19","keyboard and MIDI bindings persist");
  restored.selectWorkspace(Workspace::lab);
  restored.receiveMidi(juce::MidiMessage::controllerEvent(2,19,37));
  check(restored.value("int")==37 && !restored.performanceEvents.pop(event),"Lab binding remains independent");
  restored.selectWorkspace(Workspace::multiple);
  restored.receiveMidi(juce::MidiMessage::controllerEvent(2,19,52));
  check(restored.value("glow")==52 && !restored.performanceEvents.pop(event),"Multiple binding remains independent");
  restored.selectWorkspace(Workspace::performance);restored.clearBindings();
  check(restored.performanceMidiLabel(24)=="Unbound","clearing Performance bindings clears actions");
  restored.selectWorkspace(Workspace::lab);
  restored.receiveMidi(juce::MidiMessage::controllerEvent(2,19,61));
  check(restored.value("int")==61,"clearing Performance preserves Lab bindings");
  return failures==0?0:1;
}
