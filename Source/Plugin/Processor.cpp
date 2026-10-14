#include "Processor.h"
#include "Core/Lyrics.h"
#include "Visual/Scenes.h"
#if GRID_USE_VEL_ENGINE
#include "Engine/VelEngine.h"
#endif
namespace kone::grid {
static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters() {
  juce::AudioProcessorValueTreeState::ParameterLayout p;
  for (auto c : controls)
    p.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID(c.id, 2),
                                                    c.name, 0, 127, c.initial));
  return p;
}
Processor::Processor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Audio", juce::AudioChannelSet::stereo(), true)
              .withOutput("Audio", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "GRID_ARVX_STATE", makeParameters()) {
  for (size_t i = 0; i < controls.size(); ++i)
    parameters[i] = state.getParameter(controls[i].id);
  for(auto &values:workspaceValues)
    for(size_t i=0;i<controls.size();++i)values[i]=controls[i].initial;
}
bool Processor::isBusesLayoutSupported(const BusesLayout &l) const {
  auto c = l.getMainInputChannelSet();
  return (c == juce::AudioChannelSet::mono() ||
          c == juce::AudioChannelSet::stereo()) &&
         c == l.getMainOutputChannelSet();
}
int Processor::value(const char *id) const {
  auto *p = state.getRawParameterValue(id);
  return p ? juce::roundToInt(p->load()) : 0;
}
void Processor::set(const char *id, int v) {
  if (auto *p = state.getParameter(id)) {
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1((float)juce::jlimit(0, 127, v)));
    p->endChangeGesture();
  }
}
void Processor::processBlock(juce::AudioBuffer<float> &a, juce::MidiBuffer &m) {
  juce::ScopedNoDenormals g;
  for (auto e : m)
    receiveMidi(e.getMessage());
  double bpm = 0;
  if (auto *ph = getPlayHead())
    if (auto pos = ph->getPosition(); pos && pos->getIsPlaying())
      bpm = pos->getBpm().orFallback(0.0);
  hostBpm.store(std::isfinite(bpm) && bpm > 0 ? bpm : 0);
  if (a.getNumChannels() > 0 && value("muteInput") < 64)
    analyzeInput(a.getReadPointer(0),
                  a.getNumChannels() > 1 ? a.getReadPointer(1) : nullptr,
                  a.getNumSamples());
}
void Processor::setInputSampleRate(double rate) {
  inputSampleRate.store(rate);
  analysis.setSampleRate(rate);
}
void Processor::analyzeInput(const float *left, const float *right, int count) {
  analysis.push(left, right, count);
#if GRID_USE_VEL_ENGINE
  if (auto engine = velEngine.load())
    engine->pushAudio(left, right, count, inputSampleRate.load());
#endif
}
void Processor::receiveMidi(const juce::MidiMessage &m) noexcept {
  if(switchingWorkspace.load())return;
  if (!m.isController() && !m.isNoteOnOrOff())
    return;
  bool note = m.isNoteOnOrOff();
  MidiEvent e{m.getChannel(),
              note ? m.getNoteNumber() : m.getControllerNumber(),
              note ? (m.isNoteOn() ? 127 : 0) : m.getControllerValue(), note};
  if (learning.load()) {
    midiEvents.push(e);
    return;
  }
  const auto context=activeWorkspace.load();
  const auto bank=(size_t)context;
  if (performance.load() && context==Workspace::lab)
    return;
  int s = (e.channel - 1) * 256 + e.cc + (e.note ? 128 : 0);
  if(context==Workspace::performance) {
    const int action=performanceMidi[(size_t)s].load()-1;
    if(action>=0 && action<(int)performanceActions.size()) {
      if(e.value!=0 && e.value!=127)return;
      const bool down=e.value==127,old=performanceMidiDown[(size_t)s].exchange(down);
      if(down!=old && (down || performanceActions[(size_t)action].held))
        performanceEvents.push({e.channel,action,e.value,e.note});
      return;
    }
  }
  auto b = unpack(bindings[bank][(size_t)s].load());
  if (!compatible(b))
    return;
  if (b.hardware == Hardware::button) {
    if (e.value != 0 && e.value != 127)
      return;
    bool down = e.value == 127, old = held[bank][(size_t)s].exchange(down);
    if (!down || old)
      return;
  }
  auto *p = parameters[(size_t)b.target];
  float n = (float)e.value;
  if (b.fixed >= 0)
    n = (float)b.fixed;
  else if (b.hardware == Hardware::button)
    n = p->getValue() >= 0.5f ? 0.0f : 127.0f;
  p->setValueNotifyingHost(p->convertTo0to1(n));
  appliedMidi.push({e.channel,b.target,(int)n,e.note});
  if(context!=Workspace::lab)
    mappedEvents.push({(int)context+1,b.target,(int)n,b.hardware==Hardware::button});
}
void Processor::selectWorkspace(Workspace context) {
  if(context==activeWorkspace.load())return;
  switchingWorkspace.store(true);
  const auto old=(size_t)activeWorkspace.load();
  for(size_t i=0;i<controls.size();++i)
    if(!globalControl(controls[i].id)) {
      workspaceValues[old][i]=value(controls[i].id);
      set(controls[i].id,workspaceValues[(size_t)context][i]);
    }
  learning.store(false);
  activeWorkspace.store(context);
  performance.store(context==Workspace::performance);
  for(auto &button:held[(size_t)context])button.store(false);
  for(auto &button:performanceMidiDown)button.store(false);
  MidiEvent discarded;while(performanceEvents.pop(discarded)){}
  switchingWorkspace.store(false);
}
bool Processor::bind(int ch, int cc, Binding b, bool note) {
  if (ch < 1 || ch > 16 || cc < 0 || cc > 127 || !compatible(b) ||
      (note && b.hardware != Hardware::button))
    return false;
  bindings[(size_t)activeWorkspace.load()][(size_t)((ch - 1) * 256 + cc + (note ? 128 : 0))].store(pack(b));
  if(workspace()==Workspace::performance)performanceMidi[(size_t)((ch-1)*256+cc+(note?128:0))].store(0);
  return true;
}
void Processor::clearBindings() {
  const auto bank=(size_t)activeWorkspace.load();
  for (auto &b : bindings[bank])
    b.store(0);
  for (auto &h : held[bank])
    h.store(false);
  if(workspace()==Workspace::performance) {
    for(auto &entry:performanceMidi)entry.store(0);
    for(auto &entry:performanceMidiDown)entry.store(false);
  }
}
Processor::PerformanceBindingLabels Processor::performanceBindingLabels() const {
  PerformanceBindingLabels labels;
  const auto data=juce::JSON::parse(getAssetProperty("performanceKeys"));
  for(size_t i=0;i<performanceActions.size();++i) {
    const auto entry=data[performanceActions[i].id];
    const auto key=entry.isObject()?juce::KeyPress((int)entry["key"],(int)entry["modifiers"],0):defaultPerformanceKey(i);
    labels.keyboard[i]=performanceKeyLabel(key);labels.midi[i]="Unbound";
  }
  for(size_t slot=0;slot<performanceMidi.size();++slot) {
    const int action=performanceMidi[slot].load()-1;
    if(juce::isPositiveAndBelow(action,(int)performanceActions.size()) && labels.midi[(size_t)action]=="Unbound")
      labels.midi[(size_t)action]="Ch "+juce::String((int)slot/256+1)+(slot%256>=128?" Note ":" CC ")+juce::String((int)slot%128);
  }
  return labels;
}
juce::KeyPress Processor::performanceKey(size_t action) const {
  if(action>=performanceActions.size())return {};
  const auto data=juce::JSON::parse(getAssetProperty("performanceKeys"));
  const auto entry=data[performanceActions[action].id];
  if(!entry.isObject())return defaultPerformanceKey(action);
  return {(int)entry["key"],(int)entry["modifiers"],0};
}
void Processor::setPerformanceKey(size_t action,const juce::KeyPress &key) {
  if(action>=performanceActions.size())return;
  auto data=juce::JSON::parse(getAssetProperty("performanceKeys"));
  if(!data.isObject())data=juce::var(new juce::DynamicObject());
  // One key resolves to one action. Explicitly unbind the previous owner.
  for(size_t i=0;i<performanceActions.size();++i)
    if(i==action || (key.isValid() && samePerformanceKey(performanceKey(i),key))) {
      auto entry=new juce::DynamicObject();
      entry->setProperty("key",i==action?key.getKeyCode():0);
      entry->setProperty("modifiers",i==action?key.getModifiers().getRawFlags():0);
      data.getDynamicObject()->setProperty(performanceActions[i].id,juce::var(entry));
    }
  setAssetProperty("performanceKeys",juce::JSON::toString(data));
}
void Processor::bindPerformanceMidi(size_t action,int channel,int control,bool note) {
  if(action>=performanceActions.size() || channel<1 || channel>16 || control<0 || control>127)return;
  const int slot=(channel-1)*256+control+(note?128:0);
  for(auto &entry:performanceMidi)if(entry.load()==(int)action+1)entry.store(0);
  // Replace only the Performance assignment for this physical control.
  bindings[(size_t)Workspace::performance][(size_t)slot].store(0);
  performanceMidiDown[(size_t)slot].store(false);
  performanceMidi[(size_t)slot].store((int)action+1);
}
juce::String Processor::performanceMidiLabel(size_t action) const {
  for(size_t slot=0;slot<performanceMidi.size();++slot)
    if(performanceMidi[slot].load()==(int)action+1)
      return "Ch "+juce::String((int)slot/256+1)+(slot%256>=128?" Note ":" CC ")+juce::String((int)slot%128);
  return "Unbound";
}
juce::var Processor::bindingState() const {
  juce::Array<juce::var> a;
  for(size_t bank=0;bank<bindings.size();++bank)
  for (int i = 0; i < 4096; ++i)
    if (int v = bindings[bank][(size_t)i].load()) {
      auto *o = new juce::DynamicObject;
      o->setProperty("channel", i / 256 + 1);
      o->setProperty("workspace",(int)bank);
      o->setProperty("cc", i % 128);
      o->setProperty("note", (i % 256) >= 128);
      o->setProperty("binding", v);
      auto binding = unpack(v);
      o->setProperty("targetId", controls[(size_t)binding.target].id);
      o->setProperty("fixed", binding.fixed);
      o->setProperty("hardware",
                     binding.hardware == Hardware::button ? "button" : "knob");
      a.add(juce::var(o));
    }
  return a;
}
void Processor::restoreBindings(const juce::var &v) {
  for(auto &bank:bindings)for(auto &binding:bank)binding.store(0);
  for(auto &bank:held)for(auto &button:bank)button.store(false);
  if (auto *a = v.getArray())
    for (auto e : *a) {
      auto binding = unpack((int)e["binding"]);
      if (e["targetId"].isString())
        binding = {indexOf(e["targetId"].toString().toStdString()),
                   (int)e["fixed"],
                   e["hardware"].toString() == "button" ? Hardware::button
                                                        : Hardware::knob};
      const int bank=e["workspace"].isVoid()?0:(int)e["workspace"];
      const int channel=e["channel"],cc=e["cc"];
      const bool note=e["note"];
      if(bank>=0 && bank<3 && channel>=1 && channel<=16 && cc>=0 && cc<=127 &&
         compatible(binding) && (!note || binding.hardware==Hardware::button))
        bindings[(size_t)bank][(size_t)((channel-1)*256+cc+(note?128:0))].store(pack(binding));
    }
}
juce::Colour Processor::visualColour(const char* id,bool background) const {
  const auto data=juce::JSON::parse(getAssetProperty("nativePresetColors"));
  const auto entry=data[id];
  if(entry.isObject() && (int)entry["cc"]==value(id)) return juce::Colour((juce::uint32)(juce::int64)entry["argb"]);
  return palette(value(id),background);
}
juce::String Processor::getAssetProperty(const char *n) const {
  juce::ScopedLock l(assetLock);
  return assets[n].toString();
}
void Processor::setAssetProperty(const char *n, const juce::String &v) {
  juce::ScopedLock l(assetLock);
  assets.set(n, v);
}
void Processor::getStateInformation(juce::MemoryBlock &b) {
  auto s = state.copyState();
  {
    juce::ScopedLock l(assetLock);
    for (int i = 0; i < assets.size(); ++i)
      s.setProperty(assets.getName(i), assets.getValueAt(i), nullptr);
  }
  s.setProperty("midiBindings", juce::JSON::toString(bindingState()), nullptr);
  juce::Array<juce::var> performanceBindings;
  for(size_t i=0;i<performanceMidi.size();++i)if(const int value=performanceMidi[i].load()) {
    auto entry=new juce::DynamicObject();entry->setProperty("slot",(int)i);
    entry->setProperty("action",performanceActions[(size_t)value-1].id);performanceBindings.add(juce::var(entry));
  }
  s.setProperty("performanceMidiBindings",juce::JSON::toString(performanceBindings),nullptr);
  juce::Array<juce::var> contexts;
  for(size_t bank=0;bank<3;++bank) {
    auto *object=new juce::DynamicObject();
    for(size_t i=0;i<controls.size();++i)
      object->setProperty(controls[i].id,bank==(size_t)activeWorkspace.load()?value(controls[i].id):workspaceValues[bank][i]);
    contexts.add(juce::var(object));
  }
  s.setProperty("workspaceControls",juce::JSON::toString(contexts),nullptr);
  s.setProperty("schema", 2, nullptr);
  s.setProperty("templateCatalogRevision", templateCatalogRevision, nullptr);
  if (auto x = s.createXml())
    copyXmlToBinary(*x, b);
}
void Processor::setStateInformation(const void *d, int n) {
  restoreState(d, n);
}
bool Processor::restoreState(const void *d, int n) {
  if (!d || n < 8)
    return false;
  if (auto x = getXmlFromBinary(d, n); x && x->hasTagName("GRID_ARVX_STATE")) {
    auto s = juce::ValueTree::fromXml(*x);
    if (s.hasProperty("schema") && (int)s["schema"] != 1 &&
        (int)s["schema"] != 2)
      return false;
    const auto contexts=juce::JSON::parse(s["workspaceControls"].toString());
    if(s.hasProperty("workspaceControls")) {
      auto *banks=contexts.getArray();
      if(!banks || banks->size()!=3)return false;
      for(const auto &bank:*banks) {
        if(!bank.isObject())return false;
        for(const auto &control:controls)if(bank.hasProperty(control.id)) {
          const double value=bank[control.id];
          if(!std::isfinite(value) || value<0 || value>127 || std::floor(value)!=value)return false;
        }
      }
    }
    if ((int)s["schema"] == 2) {
      const auto lyrics = s["lyrics"].toString();
      if (lyrics.isNotEmpty() && !validLyrics(juce::JSON::parse(lyrics)))
        return false;
      for (auto child : s) {
        if (indexOf(child["id"].toString().toStdString()) < 0)
          continue;
        const double value = child["value"];
        if (!std::isfinite(value) || value < 0 || value > 127 ||
            std::floor(value) != value)
          return false;
      }
    }
    if (!s.hasProperty("schema") || (int)s["schema"] == 1) {
      for (auto child : s) {
        auto id = child["id"].toString();
        float v = (float)child["value"];
        if (id == "mode") {
          child.setProperty("id", "category", nullptr);
          child.setProperty("value",
                            encodeChoice(v < 1   ? 0
                                         : v < 2 ? 4
                                                 : 8,
                                         9),
                            nullptr);
        } else if (id == "texture")
          child.setProperty("value", juce::roundToInt(v * 127), nullptr);
        else if (id == "speed")
          child.setProperty("value", juce::roundToInt(v * 64), nullptr);
        else if (id == "sensitivity") {
          child.setProperty("id", "int", nullptr);
          child.setProperty(
              "value", juce::jlimit(0, 127, juce::roundToInt(v * 64)), nullptr);
        }
      }
    }
    const bool changedCatalog=(int)s["templateCatalogRevision"] < templateCatalogRevision;
    if (changedCatalog) {
      int category = 0;
      for (auto child : s) if (child["id"].toString() == "category")
        category = decodeChoice(juce::roundToInt((float)child["value"]), 9);
      for (auto child : s) if ((int)s["templateCatalogRevision"] < 3 && child["id"].toString() == "variant")
        child.setProperty("value", encodeChoice(migrateLegacyVariant(category,
            decodeChoice(juce::roundToInt((float)child["value"]),16)),16), nullptr);
      const auto oldPreset=juce::File(s["presetFile"].toString());
      if(oldPreset.getParentDirectory().getFileName()=="Defaults") {
        for(auto child:s) if(child["id"].toString()=="variant") {
          const auto name=templates(category)[juce::jlimit(0,templates(category).size()-1,
            decodeChoice(juce::roundToInt((float)child["value"]),16))];
          s.setProperty("presetName",name,nullptr);
          s.setProperty("presetFile",oldPreset.getParentDirectory().getChildFile(name+".json").getFullPathName(),nullptr);
        }
      }
      s.setProperty("templateCatalogRevision", templateCatalogRevision, nullptr);
    }
    state.replaceState(s);
    if(auto *banks=contexts.getArray()) {
      for(size_t bank=0;bank<3;++bank)
        for(size_t i=0;i<controls.size();++i)
          workspaceValues[bank][i]=(*banks)[(int)bank].hasProperty(controls[i].id)?(int)(*banks)[(int)bank][controls[i].id]:controls[i].initial;
      for(size_t i=0;i<controls.size();++i)
        if(!globalControl(controls[i].id))set(controls[i].id,workspaceValues[(size_t)activeWorkspace.load()][i]);
    }
    {
      juce::ScopedLock l(assetLock);
      for (const auto *k :
           {"imagePath", "fontPath", "lyrics", "presetName", "presetFile",
            "graphics", "encoderPath", "styleFontRoot", "audioSource", "speakerDevice", "nativePresetColors", "nativeEngineValues", "performanceKeys", "controlsPresentation"})
        assets.set(k, s[k]);
      if (assets["lyrics"].isVoid())
        assets.set("lyrics", s["overlayText"]);
      assets.set("reloadDefaultPreset", changedCatalog &&
        juce::File(s["presetFile"].toString()).getParentDirectory().getFileName()=="Defaults" ? "1" : "0");
    }
    restoreBindings(juce::JSON::parse(s["midiBindings"].toString()));
    for(auto &entry:performanceMidi)entry.store(0);
    for(auto &entry:performanceMidiDown)entry.store(false);
    if(auto mappings=juce::JSON::parse(s["performanceMidiBindings"].toString());mappings.isArray())
      for(const auto &entry:*mappings.getArray()) {
        const int slot=entry["slot"];
        if(slot<0 || slot>=4096)continue;
        for(size_t i=0;i<performanceActions.size();++i)
          if(entry["action"].toString()==performanceActions[i].id)performanceMidi[(size_t)slot].store((int)i+1);
      }
    return true;
  }
  return false;
}
} // namespace kone::grid
#ifndef GRID_STANDALONE
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new kone::grid::Processor;
}
#endif
