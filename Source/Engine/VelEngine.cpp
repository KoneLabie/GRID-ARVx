#include "VelEngine.h"
#include "EngineFiles.h"
#include "PanelTheme.h"
#include "Core/Graphics.h"
#include "Core/PerformanceMappings.h"
#include <juce_audio_processors/format_types/VST3_SDK/pluginterfaces/vst/ivstcomponent.h>
#include <juce_audio_processors/format_types/VST3_SDK/pluginterfaces/vst/ivstunits.h>
#if JUCE_WINDOWS
#include <windows.h>
#endif
#include <map>
#include <mutex>

namespace kone::grid {
namespace {
std::mutex registryLock;
std::map<void *, VelEngine *> renderers;
std::map<void *, VelEngine *> editors;
std::mutex paletteLock;
#if JUCE_WINDOWS
// The GL child is inside a native plugin container. Hiding just the GL child
// leaves that container above GRID's lightweight dialogs, painting solid black.
// Only return a descendant of the known host peer, never the host itself.
HWND nativePanelContainer(HWND window,HWND host) {
  if(!window || !host || window==host)return nullptr;
  DWORD process=0;GetWindowThreadProcessId(window,&process);
  if(process!=GetCurrentProcessId())return nullptr;
  for(auto current=window;current && current!=host;) {
    auto parent=GetParent(current);
    if(parent==host)return (GetWindowLongPtrW(current,GWL_STYLE)&WS_CHILD)?current:nullptr;
    current=parent;
  }
  return nullptr;
}
bool readable(const void *p, size_t bytes) {
  if (!p || reinterpret_cast<uintptr_t>(p) < 65536)
    return false;
  MEMORY_BASIC_INFORMATION info{};
  if (!VirtualQuery(p, &info, sizeof(info)) || info.State != MEM_COMMIT ||
      (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
    return false;
  const auto address = reinterpret_cast<uintptr_t>(p);
  const auto end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
  return address <= end && bytes <= end - address;
}
template <typename T> T field(void *p, size_t offset) {
  T result{};
  if (!p)
    return result;
  auto *address = static_cast<char *>(p) + offset;
  if (readable(address, sizeof(T)))
    std::memcpy(&result, address, sizeof(T));
  return result;
}
template <typename T> void store(void *p, size_t offset, T value) {
  std::memcpy(static_cast<char *>(p) + offset, &value, sizeof(value));
}
void *findObject(void *parent, size_t bytes, uintptr_t vtable, bool indirect) {
  for (size_t offset = 0; offset < bytes; offset += sizeof(void *)) {
    auto *p = field<void *>(parent, offset);
    if (field<uintptr_t>(p, 0) == vtable)
      return p;
    if (indirect && readable(p, 32))
      for (size_t child = 0; child < 32; child += sizeof(void *)) {
        auto *q = field<void *>(p, child);
        if (field<uintptr_t>(q, 0) == vtable)
          return q;
      }
  }
  return nullptr;
}
struct ComponentVisitor final : juce::ExtensionsVisitor {
  Steinberg::Vst::IComponent *component = nullptr;
  void visitVST3Client(const VST3Client &client) override {
    component = client.getIComponentPtr();
  }
};
#endif
struct RenderWindow final : juce::Component {
  RenderWindow() {
    setOpaque(true);
    setInterceptsMouseClicks(false,false);
    setWantsKeyboardFocus(false);
    setBounds(-30000,-30000,600,600);
    addToDesktop(juce::ComponentPeer::windowIsTemporary |
                 juce::ComponentPeer::windowIgnoresMouseClicks |
                 juce::ComponentPeer::windowIgnoresKeyPresses);
  }
};
} // namespace

VelEngine::VelEngine() : Thread("Vel Engine audio") {
  previousParameters.fill(-1);
}
VelEngine::~VelEngine() {
  shutdown();
}
void VelEngine::shutdown() {
  setEngineHeldKeys(false,false,false);
  acceptingAudio.store(false);
  signalThreadShouldExit();
  audioAvailable.signal();
  stopThread(5000);
  // Destroying the plugin editor stops its OpenGL thread before the registry
  // or this bridge's copied per-instance vtable can be released.
  if(engineEditor)engineEditor->removeComponentListener(this);
  // The editor stops its GL thread and releases child HWNDs before either
  // parent peer is destroyed. Windows otherwise destroys children first.
  engineEditor.reset();
  panelViewport.reset();panelViewportOwner=nullptr;
  renderWindow.reset();
  {
    std::lock_guard guard(registryLock);
    renderers.erase(renderer);
    editors.erase(editorObject);
  }
  if (plugin)
    plugin->releaseResources();
  plugin.reset();
#if JUCE_WINDOWS
  if(moduleReference)FreeLibrary(static_cast<HMODULE>(moduleReference));
  moduleReference=nullptr;
#endif
  renderer = nullptr;
  editorObject = nullptr;
}
juce::File VelEngine::adjacentBundle() {
#if JUCE_WINDOWS
  HMODULE owner = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(&VelEngine::adjacentBundle), &owner);
  wchar_t path[32768]{};
  if (owner && GetModuleFileNameW(owner, path, 32768))
    return juce::File(juce::String(path)).getSiblingFile("Engine")
        .getChildFile("Vel Engine.vst3");
#endif
  return {};
}
juce::Result VelEngine::open(const juce::File &bundle,juce::File isolatedProfile) {
#if !JUCE_WINDOWS
  juce::ignoreUnused(bundle,isolatedProfile);
  return juce::Result::fail("The Vel Engine is currently Windows x64 only.");
#else
  if (plugin)
    return juce::Result::fail("Native engine already opened.");
  const auto runtime = bundle.getChildFile("Contents/x86_64-win/Vel Engine.vst3");
  // Reject installed locations even when passed explicitly by a caller.
  const auto path = runtime.getFullPathName().replaceCharacter('\\', '/');
  if (path.containsIgnoreCase("/Greyland Audio/Visual Lab Pro/") ||
      path.containsIgnoreCase("/Common Files/VST3/Visual Lab Pro.vst3/"))
    return juce::Result::fail("Refusing to load the installed Visual Lab Pro engine.");
  if (!runtime.existsAsFile() || juce::SHA256(runtime).toHexString() !=
      "110757113effef4c55d033904b9819a856a6c57f0749dc7b4809740e6c0a6b59")
    return juce::Result::fail("Missing or incompatible Vel Engine module.");
  loadedBundle = bundle;
  moduleReference=LoadLibraryW(runtime.getFullPathName().toWideCharPointer());
  const auto profile=isolatedProfile==juce::File{}?
    juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
      .getChildFile("KoneLabie/GRID-ARVx/VelEngine"):isolatedProfile;
  if(!moduleReference || !connectEngineFiles(moduleReference,profile))
    return juce::Result::fail("Vel Engine settings integration could not be connected.");
  formats.addFormat(new juce::VST3PluginFormat());
  juce::OwnedArray<juce::PluginDescription> descriptions;
  formats.getFormat(0)->findAllTypesForFile(descriptions, bundle.getFullPathName());
  if (descriptions.isEmpty())
    return juce::Result::fail("The Vel Engine module could not be scanned.");
  juce::String error;
  plugin = formats.createPluginInstance(*descriptions[0], 48000, 512, error);
  if (!plugin)
    return juce::Result::fail(error);
  if (plugin->getParameters().size() != 31)
    return juce::Result::fail("Unexpected Vel Engine parameter layout.");
  std::copy(plugin->getParameters().begin(), plugin->getParameters().end(), parameters.begin());
  // Enter Lab before constructing its editor. Keep all native state in memory;
  // no original settings/activation file is imported or written by this bridge.
  juce::MemoryBlock state;
  plugin->getStateInformation(state);
  if (auto wrapper = juce::AudioProcessor::getXmlFromBinary(state.getData(), (int)state.getSize()))
    if (auto *component = wrapper->getChildByName("IComponent")) {
      juce::MemoryBlock bytes;
      if (bytes.fromBase64Encoding(component->getAllSubText()))
        if (auto xml = juce::AudioProcessor::getXmlFromBinary(bytes.getData(), (int)bytes.getSize())) {
          xml->setAttribute("showMainMenu", 0);
          xml->setAttribute("isPerformanceMode", 0);
          xml->setAttribute("category", 10);
          juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
          component->deleteAllChildElements();
          component->addTextElement(bytes.toBase64Encoding());
          juce::AudioProcessor::copyXmlToBinary(*wrapper, state);
          plugin->setStateInformation(state.getData(), (int)state.getSize());
        }
    }
  state.reset();
  auto *editor = plugin->createEditorIfNeeded();
  if (!editor)
    return juce::Result::fail("The Vel Engine did not create a render view.");
  renderWindow = std::make_unique<RenderWindow>();
  engineEditor.reset(editor);
  renderWindow->addAndMakeVisible(editor);
  editor->setBounds(0,0,600,600);
  if (!connectRenderer()) {
    if (failure.isEmpty()) failure = "Vel Engine render interface did not match the verified 1.2.0 layout.";
    return juce::Result::fail(failure);
  }
  prepareReviewUi();
  engineEditor->addComponentListener(this);
  configureComponentPainting(false);
  renderWindow->setBounds(-30000, -30000, 600, 600);
  renderWindow->setVisible(true);
  plugin->setRateAndBufferSizeDetails(48000, 512);
  plugin->prepareToPlay(48000, 512);
  acceptingAudio.store(true);
  startThread();
  return juce::Result::ok();
#endif
}
bool VelEngine::connectRenderer() {
#if JUCE_WINDOWS
  ComponentVisitor visitor;
  plugin->getExtensions(visitor);
  if (!visitor.component) {
    failure = "VST3 component extension unavailable.";
    return false;
  }
  Steinberg::Vst::IUnitInfo *unit = nullptr;
  if (visitor.component->queryInterface(Steinberg::Vst::IUnitInfo::iid,
                                        reinterpret_cast<void **>(&unit)) != 0 || !unit) {
    failure = "VST3 unit/processor interface unavailable.";
    return false;
  }
  const auto table = field<void *>(unit, 0);
  HMODULE module = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(table), &module);
  moduleBase = reinterpret_cast<uintptr_t>(module);
  wchar_t path[32768]{};
  const auto expected = loadedBundle.getChildFile("Contents/x86_64-win/Vel Engine.vst3");
  if (!module || !GetModuleFileNameW(module, path, 32768) ||
      juce::File(juce::String(path)) != expected ||
      reinterpret_cast<uintptr_t>(table) != moduleBase + 0x8b2e98) {
    failure = "Processor interface fingerprint mismatch: table RVA " +
      juce::String::toHexString((juce::int64)(reinterpret_cast<uintptr_t>(table) - moduleBase)) +
      " / module " + juce::String(path);
    unit->release();
    return false;
  }
  // The pinned VST3 component implements IUnitInfo at its +0x10 subobject.
  // Follow its actual owning JuceAudioProcessor, rather than guessing a cast.
  auto *componentRoot = reinterpret_cast<char *>(unit) - 0x10;
  auto *owner = findObject(componentRoot, 512, moduleBase + 0x8b29b0, false);
  auto *processor = findObject(owner, 256, moduleBase + 0x8b3cb8, false);
  editorObject = findObject(processor, 1024, moduleBase + 0x8d7a58, true);
  unit->release();
  if (!editorObject || !readable(static_cast<char *>(editorObject) + 0x168, 8)) {
    failure = processor ? "Native editor reference was not found in the processor." :
                          "Native processor reference was not found in the unit interface.";
    return false;
  }
  renderer = static_cast<char *>(editorObject) + 0x168;
  originalEditorVtable=field<void **>(editorObject,0);
  if(!readable(originalEditorVtable-1,sizeof(void *)*57) ||
      reinterpret_cast<uintptr_t>(originalEditorVtable[4])!=moduleBase+0x498780 ||
      reinterpret_cast<uintptr_t>(originalEditorVtable[30])!=moduleBase+0x5137d0 ||
      reinterpret_cast<uintptr_t>(originalEditorVtable[32])!=moduleBase+0x416970 ||
      reinterpret_cast<uintptr_t>(originalEditorVtable[34])!=moduleBase+0x416800 ||
      reinterpret_cast<uintptr_t>(originalEditorVtable[39])!=moduleBase+0x411680) {
    failure="Native UI interface did not match the verified panel layout.";return false;
  }
  originalVtable = field<void **>(renderer, 0);
  if (reinterpret_cast<uintptr_t>(originalVtable) != moduleBase + 0x8d7040 ||
      !readable(originalVtable, sizeof(void *) * 4) ||
      reinterpret_cast<uintptr_t>(originalVtable[2]) != moduleBase + 0x486e60 ||
      reinterpret_cast<uintptr_t>(originalVtable[3]) != moduleBase + 0x486920) {
    failure = "Native renderer vtable fingerprint mismatch.";
    return false;
  }
  std::copy_n(originalVtable, 4, renderVtable.begin());
  renderVtable[2] = reinterpret_cast<void *>(&VelEngine::renderThunk);
  renderVtable[3] = reinterpret_cast<void *>(&VelEngine::closeThunk);
  {
    std::lock_guard guard(registryLock);
    renderers.emplace(renderer, this);
    editors.emplace(editorObject,this);
  }
  std::copy_n(originalEditorVtable-1,editorVtable.size(),editorVtable.begin());
  editorVtable[5]=reinterpret_cast<void *>(&VelEngine::editorMouseDownThunk);
  editorVtable[31]=reinterpret_cast<void *>(&VelEngine::editorPaintThunk);
  editorVtable[33]=reinterpret_cast<void *>(&VelEngine::editorKeyThunk);
  editorVtable[40]=reinterpret_cast<void *>(&VelEngine::editorResizedThunk);
  editorVtable[35]=reinterpret_cast<void *>(&VelEngine::editorModifiersThunk);
  store(editorObject,0,editorVtable.data()+1);
  store(renderer, 0, renderVtable.data());
  return true;
#else
  return false;
#endif
}
void VelEngine::setHomeAction(std::function<void()> action) {
  juce::ScopedLock guard(homeLock);homeAction=std::move(action);
}
#ifdef GRID_ENGINE_PROBE
float VelEngine::checkPerformanceHold(bool strobes) const {
#if JUCE_WINDOWS
  auto *processor=field<void *>(editorObject,0x1a8);
  auto *state=reinterpret_cast<void *(*)(void *,const char *)>(moduleBase+0x3a1f00)(
      static_cast<char *>(processor)+0x2578,strobes?"perfStrobe":"perfBlackout");
  return field<float>(state,0x98);
#else
  juce::ignoreUnused(strobes);return 0;
#endif
}
void VelEngine::checkMultipleDone() {if(multipleLayoutVisible())finishMultipleLayout();}
void VelEngine::checkPerformancePlay() {startPerformance();}
void VelEngine::checkSaveMultiple(const juce::String &name) {
#if JUCE_WINDOWS
  void *text=reinterpret_cast<void *>(moduleBase+0x982d18);
  reinterpret_cast<void (*)(void *,const char *)>(moduleBase+0x267e0)(&text,name.toRawUTF8());
  reinterpret_cast<void (*)(void *,const void *)>(moduleBase+0x481340)(editorObject,&text);
  reinterpret_cast<void (*)(void *)>(moduleBase+0x1ff8e0)(&text);
#endif
}
void VelEngine::checkCancelMultipleSave() {
  clickPanelControl({viewportBounds.getWidth()*.5f-70,viewportBounds.getHeight()*.5f+45});
}
juce::var VelEngine::viewportState() const {
  // The render thread temporarily installs scene dimensions in native fields.
  // Observe viewport geometry only after that frame's restoration completes.
  const std::lock_guard geometryGuard(geometryLock);
  auto *state=new juce::DynamicObject();
#if JUCE_WINDOWS
  if(engineEditor && editorObject) {
    state->setProperty("adapterBounds",engineEditor->getBounds().toString());
    state->setProperty("nativeWidth",field<int>(editorObject,0x40));
    state->setProperty("nativeHeight",field<int>(editorObject,0x44));
    state->setProperty("headerCrop",hiddenPanelHeader);
    state->setProperty("multiplePresetBrowserOpen",field<unsigned char>(editorObject,0x2d65)!=0);
    state->setProperty("multipleSaveOpen",field<unsigned char>(editorObject,0x2dbc)!=0);
    state->setProperty("multiplePresetIndex",field<int>(editorObject,0x35c0));
    state->setProperty("multipleRows",field<int>(editorObject,0x2c20));
    state->setProperty("multipleColumns",field<int>(editorObject,0x2c1c));
    auto region=CreateRectRgn(0,0,0,0);RECT actual{};
    auto containerWindow=nativePanelContainer(static_cast<HWND>(panelWindow.load()),static_cast<HWND>(hostPeerWindow.load()));
    if(containerWindow && GetWindowRgn(containerWindow,region)!=ERROR)GetRgnBox(region,&actual);
    DeleteObject(region);state->setProperty("nativeRegionTop",(int)actual.top);
    RECT containerRect{},hostRect{};
    if(containerWindow && GetWindowRect(containerWindow,&containerRect) &&
        GetWindowRect(static_cast<HWND>(hostPeerWindow.load()),&hostRect)) {
      state->setProperty("nativeContainerTop",(int)(containerRect.top-hostRect.top));
      state->setProperty("nativeContainerHeight",(int)(containerRect.bottom-containerRect.top));
      const auto scale=engineEditor->getPeer()->getPlatformScaleFactor();
      state->setProperty("nativeViewportActive",panelViewport && engineEditor->getParentComponent()==panelViewport.get());
      state->setProperty("nativeViewportBounds",panelViewport?panelViewport->getBounds().toString():juce::String{});
      RECT viewportRect{};
      auto viewportWindow=panelViewport && panelViewport->getPeer()?static_cast<HWND>(panelViewport->getPeer()->getNativeHandle()):nullptr;
      if(viewportWindow && GetWindowRect(viewportWindow,&viewportRect)) {
        MapWindowPoints(HWND_DESKTOP,GetParent(viewportWindow),reinterpret_cast<POINT *>(&viewportRect),2);
        state->setProperty("nativeViewportTop",(int)viewportRect.top);
        state->setProperty("nativeViewportHeight",(int)(viewportRect.bottom-viewportRect.top));
        state->setProperty("nativeViewportBelowToolbar",viewportRect.top>=juce::roundToInt(82*scale));
      }
      state->setProperty("nativeContentCropped",hiddenPanelHeader && containerRect.top-hostRect.top==-juce::roundToInt(hiddenPanelHeader*scale));
    }
    state->setProperty("nativeContentY",field<int>(editorObject,0x3c));
    state->setProperty("multipleScreen",field<int>(editorObject,0x2c14));
    state->setProperty("performanceScreen",field<int>(editorObject,0x2c30));
    state->setProperty("contextRestarts",(int)contextRestarts.load());
    state->setProperty("pendingPresentation",(juce::int64)revealAfterPaint.load());
    state->setProperty("surfaceVisible",panelWindow.load() && IsWindowVisible(static_cast<HWND>(panelWindow.load())));
    const auto container=nativePanelContainer(static_cast<HWND>(panelWindow.load()),static_cast<HWND>(hostPeerWindow.load()));
    state->setProperty("nativeContainerFound",container!=nullptr);
    auto host=static_cast<HWND>(panelViewport && engineEditor->getParentComponent()==panelViewport.get()?panelViewportOwner:hostPeerWindow.load());
    state->setProperty("hostPeerVisible",host && IsWindowVisible(host));
    state->setProperty("nativeContainerVisible",container && IsWindowVisible(container));
    state->setProperty("fitsViewport",engineEditor->getBounds()==viewportBounds &&
        field<int>(editorObject,0x40)==viewportBounds.getWidth() &&
        field<int>(editorObject,0x44)==viewportBounds.getHeight());
  }
#endif
  return juce::var(state);
}
void VelEngine::checkPanelMenuClick() {checkPanelClick({30,30});}
void VelEngine::checkPanelMappingClick() {checkPanelClick({150,30});}
void VelEngine::checkPanelClick(juce::Point<float> position) {clickPanelControl(position);}
#endif
void VelEngine::clickPanelControl(juce::Point<float> position) {
#if JUCE_WINDOWS
  if(!ready())return;
  auto *target=reinterpret_cast<juce::Component *>(editorObject);
  const auto now=juce::Time::getCurrentTime();
  auto event=juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),position,
      juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,
      target,target,now,position,now,1,false);
  // Exercise the actual native handler and its GRID return callback. This
  // never sends OS input or moves the user's pointer/controls.
  editorMouseDownThunk(editorObject,&event);
#endif
}
void VelEngine::startPerformance() {
#if JUCE_WINDOWS
  if(ready() && workspace.load()==Workspace::performance)
    reinterpret_cast<void (*)(void *)>(moduleBase+0x5256d0)(editorObject);
#endif
}
bool VelEngine::multipleLayoutVisible() const {
#if JUCE_WINDOWS
  return ready() && workspace.load()==Workspace::multiple && field<int>(editorObject,0x2c14)==0;
#else
  return false;
#endif
}
void VelEngine::finishMultipleLayout() {
#if JUCE_WINDOWS
  if(!ready() || workspace.load()!=Workspace::multiple)return;
  if(multipleLayoutVisible()) {
    alignas(16) std::array<unsigned char,512> layout{};
    reinterpret_cast<void (*)(void *,void *)>(moduleBase+0x469ef0)(editorObject,layout.data());
    reinterpret_cast<bool (*)(void *,int,int)>(moduleBase+0x4565a0)(editorObject,
      field<int>(layout.data(),16)+field<int>(layout.data(),24)/2,
      field<int>(layout.data(),20)+field<int>(layout.data(),28)/2);
  } else {
    reinterpret_cast<void (*)(void *)>(moduleBase+0x480490)(editorObject);
    store(editorObject,0x2c14,0);
    reinterpret_cast<void (*)(void *)>(moduleBase+0x39b520)(editorObject);
  }
#endif
}
void VelEngine::openMultiplePresets() {
#if JUCE_WINDOWS
  if(!ready() || workspace.load()!=Workspace::multiple)return;
  if(!multipleLayoutVisible())finishMultipleLayout();
  alignas(16) std::array<unsigned char,512> layout{};
  reinterpret_cast<void (*)(void *,void *)>(moduleBase+0x469ef0)(editorObject,layout.data());
  reinterpret_cast<bool (*)(void *,int,int)>(moduleBase+0x4565a0)(editorObject,
    field<int>(layout.data(),32)+field<int>(layout.data(),40)/2,
    field<int>(layout.data(),36)+field<int>(layout.data(),44)/2);
#endif
}
void VelEngine::stepMultiplePreset(int direction) {
#if JUCE_WINDOWS
  if(!ready() || workspace.load()!=Workspace::multiple)return;
  if(!multipleLayoutVisible())finishMultipleLayout();
  alignas(16) std::array<unsigned char,512> layout{};
  reinterpret_cast<void (*)(void *,void *)>(moduleBase+0x469ef0)(editorObject,layout.data());
  const size_t offset=direction<0?48:64;
  // Invoke the original arrow handler: it wraps the native saved-layout list
  // and restores every tile's serialized visual state through its own loader.
  reinterpret_cast<bool (*)(void *,int,int)>(moduleBase+0x4565a0)(editorObject,
    field<int>(layout.data(),offset)+field<int>(layout.data(),offset+8)/2,
    field<int>(layout.data(),offset+4)+field<int>(layout.data(),offset+12)/2);
#endif
}
void VelEngine::saveMultiplePreset() {
#if JUCE_WINDOWS
  if(!ready() || workspace.load()!=Workspace::multiple)return;
  // Reuse the native Multiple Save handler in layout and live views; this
  // preserves its file format, overwrite confirmation and existing callbacks.
  std::array<float,4> rectangle{};
  reinterpret_cast<void (*)(void *,void *)>(moduleBase+0x4669d0)(editorObject,rectangle.data());
  const int x=juce::roundToInt(rectangle[0]+rectangle[2]*.5f),y=juce::roundToInt(rectangle[1]+rectangle[3]*.5f);
  const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),{(float)x,(float)y},
    juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,
    engineEditor.get(),engineEditor.get(),juce::Time::getCurrentTime(),{(float)x,(float)y},juce::Time::getCurrentTime(),1,false);
  const auto overlay=field<unsigned char>(editorObject,0x2c2c);
  store(editorObject,0x2c2c,(unsigned char)1);
  reinterpret_cast<bool (*)(void *,int,int,const void *)>(moduleBase+0x456e20)(editorObject,x,y,&event);
  store(editorObject,0x2c2c,overlay);
#endif
}
void VelEngine::returnHome() {
#if JUCE_WINDOWS
  if(workspace.load()==Workspace::lab || homePending.exchange(true))return;
  store(editorObject,0x2b54,(unsigned char)0);
  std::function<void()> action;
  {juce::ScopedLock guard(homeLock);action=homeAction;}
  if(action) {
    if(juce::MessageManager::getInstance()->isThisTheMessageThread())action();
    else juce::MessageManager::callAsync(std::move(action));
  }
#endif
}
void VelEngine::editorMouseDownThunk(void *object,void *event) {
#if JUCE_WINDOWS
  VelEngine *engine=nullptr;
  {std::lock_guard guard(registryLock);auto found=editors.find(object);if(found!=editors.end())engine=found->second;}
  if(!engine)return;
  reinterpret_cast<void (*)(void *,void *)>(engine->originalEditorVtable[4])(object,event);
  engine->fitViewport();
  engine->checkMappingRequest();
  if(field<unsigned char>(object,0x2b54)!=0)engine->returnHome();
#else
  juce::ignoreUnused(object,event);
#endif
}
bool VelEngine::editorKeyThunk(void *object,const void *key) {
#if JUCE_WINDOWS
  VelEngine *engine=nullptr;
  {std::lock_guard guard(registryLock);auto found=editors.find(object);if(found!=editors.end())engine=found->second;}
  if(!engine)return false;
  if(engine->performancePlaying() && engine->performanceKeyAction)
    return engine->performanceKeyAction(*static_cast<const juce::KeyPress *>(key));
  const bool handled=reinterpret_cast<bool (*)(void *,const void *)>(engine->originalEditorVtable[32])(object,key);
  engine->fitViewport();
  engine->checkMappingRequest();
  if(field<unsigned char>(object,0x2b54)!=0)engine->returnHome();
  return handled;
#else
  juce::ignoreUnused(object,key);return false;
#endif
}
void VelEngine::editorModifiersThunk(void *object,const void *modifiers) {
#if JUCE_WINDOWS
  VelEngine *engine=nullptr;
  {std::lock_guard guard(registryLock);auto found=editors.find(object);if(found!=editors.end())engine=found->second;}
  if(!engine || (engine->performancePlaying() && engine->performanceKeyAction))return;
  reinterpret_cast<void (*)(void *,const void *)>(engine->originalEditorVtable[34])(object,modifiers);
#else
  juce::ignoreUnused(object,modifiers);
#endif
}
bool VelEngine::performancePlaying() const {
#if JUCE_WINDOWS
  return ready() && workspace.load()==Workspace::performance && field<int>(editorObject,0x2c30)==5;
#else
  return false;
#endif
}
void VelEngine::checkMappingRequest() {
#if JUCE_WINDOWS
  if(workspace.load()!=Workspace::performance || field<int>(editorObject,0x2c30)!=4 || !mappingAction)return;
  store(editorObject,0x2c30,0);fitViewport();mappingAction();
#endif
}
void VelEngine::setPerformanceHeld(bool active,bool strobes,bool blackout) {
  setEngineHeldKeys(active,strobes,blackout);
}
void VelEngine::triggerPerformanceAction(size_t action) {
#if JUCE_WINDOWS
  if(!performancePlaying() || action>=performanceActions.size())return;
  if(juce::String(performanceActions[action].id)=="autoQueue") {applyMappedControl("perfAuto",127,true);return;}
  if(performanceActions[action].held)return;
  const auto key=defaultPerformanceKey(action);
  reinterpret_cast<bool (*)(void *,const void *)>(originalEditorVtable[32])(editorObject,&key);
  fitViewport();
#else
  juce::ignoreUnused(action);
#endif
}
void VelEngine::editorResizedThunk(void *object) {
#if JUCE_WINDOWS
  VelEngine *engine=nullptr;
  {std::lock_guard guard(registryLock);auto found=editors.find(object);if(found!=editors.end())engine=found->second;}
  if(!engine)return;
  // Native Play/Back/fullscreen requests a square editor. Restore the host's
  // dimensions before the VST3 component listener negotiates its window size.
  if(!engine->fittingViewport && !engine->viewportBounds.isEmpty() &&
      (field<int>(object,0x40)!=engine->viewportBounds.getWidth() ||
       field<int>(object,0x44)!=engine->viewportBounds.getHeight())) {
    engine->fitViewport();return;
  }
  reinterpret_cast<void (*)(void *)>(engine->originalEditorVtable[39])(object);
#else
  juce::ignoreUnused(object);
#endif
}
void VelEngine::componentMovedOrResized(juce::Component &component,bool,bool) {
  if(&component==engineEditor.get())fitViewport();
}
void VelEngine::fitViewport() {
#if JUCE_WINDOWS
  if(fittingViewport || !engineEditor || !editorObject || viewportBounds.isEmpty())return;
  const std::lock_guard geometryGuard(geometryLock);
  const juce::ScopedValueSetter<bool> fitting(fittingViewport,true);
  engineEditor->setBounds(viewportBounds);
  hostPeerWindow.store(engineEditor->getPeer()?engineEditor->getPeer()->getNativeHandle():nullptr);
  // The native viewport peer owns clipping. Leave both JUCE implementations'
  // editor sizes identical, so VST3 resize negotiation cannot expand the host.
  reinterpret_cast<void (*)(void *,int,int,int,int)>(moduleBase+0x3852a0)
      (editorObject,0,0,viewportBounds.getWidth(),viewportBounds.getHeight());
#endif
}
void VelEngine::editorPaintThunk(void *object,void *graphics) {
#if JUCE_WINDOWS
  VelEngine *engine=nullptr;
  {std::lock_guard guard(registryLock);auto found=editors.find(object);if(found!=editors.end())engine=found->second;}
  if(!engine)return;
  const auto paint=reinterpret_cast<void (*)(void *,void *)>(engine->originalEditorVtable[30]);
  if(engine->presentationChanging.load())return;
  if(engine->workspace.load()==Workspace::lab) {paint(object,graphics);return;}
  // Suppress the provider home paint before an asynchronous fallback can run.
  // Mouse/keyboard returns normally reach GRID synchronously instead.
  if(engine->homePending.load())return;
  if(field<unsigned char>(object,0x2b54)!=0){engine->returnHome();return;}
  const bool menu=engine->workspace.load()==Workspace::multiple?
      field<int>(object,0x2c14)==0:field<int>(object,0x2c30)<5;
  paintEnginePanel(object,graphics,engine->moduleBase,paint,menu);
  ++engine->panelPaintCount;
#else
  juce::ignoreUnused(object,graphics);
#endif
}
void VelEngine::renderThunk(void *object) {
  VelEngine *engine = nullptr;
  {
    std::lock_guard guard(registryLock);
    auto found = renderers.find(object);
    if (found != renderers.end())
      engine = found->second;
  }
  if (engine)
    engine->render(object);
}
juce::var VelEngine::frameTimings() const {
  auto *stats=new juce::DynamicObject();stats->setProperty("frameIntervalMs",frameInterval.load());
  stats->setProperty("nativeSubmitMs",sceneCpuMs.load());stats->setProperty("gpuReadbackMs",readbackCpuMs.load());
  stats->setProperty("pixelCopyMs",copyCpuMs.load());return juce::var(stats);
}
void VelEngine::render(void *object) {
#if JUCE_WINDOWS
  auto window=WindowFromDC(wglGetCurrentDC());
  const bool changedWindow=panelWindow.exchange(window)!=window;
  if(panelCovered.load())
    if(auto container=nativePanelContainer(window,static_cast<HWND>(hostPeerWindow.load()));container && IsWindowVisible(container))
      ShowWindowAsync(container,SW_HIDE);
  const auto reveal=revealAfterPaint.load();
  if(window && (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD)) {
    if(panelCovered.load() || (reveal && (presentationChanging.load() || panelPaintCount.load()<reveal))) {
      if(changedWindow || IsWindowVisible(window))ShowWindowAsync(window,SW_HIDE);
    } else if(reveal && !presentationChanging.load()) {
      // Publish the completed native control buffer, never the blank or stale
      // surface produced while the child context is being moved/recreated.
      ShowWindowAsync(window,SW_SHOWNOACTIVATE);revealAfterPaint.store(0);
    }
  }
  if(panelCovered.load() || paused.load() || homePending.load() || presentationChanging.load()) {
    lastFrameTime=0;
    if(!componentPainting.load())juce::Thread::sleep(4);
    return;
  }
  SceneState settings;
  {
    juce::ScopedLock guard(controlLock);
    settings = controls;
  }
  const double now = juce::Time::getMillisecondCounterHiRes();
  if (lastFrameTime > 0 && now - lastFrameTime < 1000.0 / frameRate(settings.get("frameCap"))) {
    // Visible native panels still need their last scene under the UI. The
    // offscreen host has no display to present and must not busy-spin between
    // captures. Never wait here while native UI painting holds its message lock.
    if(componentPainting.load()) {
      // Native menus are painted by JUCE after this callback. Reusing a scene
      // framebuffer here overwrites their background with stale/clipped pixels.
      if(workspace.load()==Workspace::lab || performancePlaying() ||
          (workspace.load()==Workspace::multiple && field<int>(editorObject,0x2c14)!=0))
        if(surface.width()>0)surface.present(settings.get("antialias")>=64);
    }
    else juce::Thread::sleep(juce::jlimit(1,4,(int)(1000.0/frameRate(settings.get("frameCap"))-(now-lastFrameTime))));
    return;
  }
  if(lastFrameTime>0)frameInterval.store(now-lastFrameTime);
  lastFrameTime = now;
  bool scaledSurface=false;
  const std::lock_guard geometryGuard(geometryLock);
  const bool labFrame=workspace.load()==Workspace::lab;
  const bool sceneFrame=labFrame || performancePlaying() ||
    (workspace.load()==Workspace::multiple && field<int>(editorObject,0x2c14)!=0);
#ifdef GRID_ENGINE_PROBE
  if(!sceneFrame && panelPaintCount.load()>0) {
    const auto image=EngineSurface::frontImage();
    juce::ScopedLock lock(frameLock);presentedPanel=image;
  }
#endif
  RECT client{};
  if(GetClientRect(WindowFromDC(wglGetCurrentDC()),&client)) {
    if(sceneFrame) {
      const double scale=juce::jmin(1.0,juce::jmin(3840.0/juce::jmax(1,settings.width),2160.0/juce::jmax(1,settings.height)));
      const int width=juce::jmax(320,juce::roundToInt(settings.width*scale));
      const int height=juce::jmax(180,juce::roundToInt(settings.height*scale));
      scaledSurface=surface.begin(width,height,client.right,client.bottom,width,height);
    }
    // Native menu composition keeps the actual client viewport. Resolution
    // quality applies to visual scenes, not to JUCE control rendering.
  }
  if(scaledSurface)renderDimensions.store(((juce::uint64)surface.width()<<32)|(juce::uint64)surface.height());
  // Keep the native scene's pixel geometry at the requested aspect without
  // invoking Component::setBounds (which negotiates a VST3 resize and repaints UI).
  // Lab's native UI is hidden and component painting is disabled. These bounds
  // are used only for this render callback and restored before returning.
  struct FrameGeometry {
    void *editor=nullptr;int width=0,height=0;
    ~FrameGeometry(){if(editor){store(editor,0x40,width);store(editor,0x44,height);}}
  } geometry;
  if(sceneFrame && scaledSurface) {
    geometry.editor=editorObject;geometry.width=field<int>(editorObject,0x40);geometry.height=field<int>(editorObject,0x44);
    const double dpi=juce::jmax(1.0,field<double>(editorObject,0x1c8));
    store(editorObject,0x40,juce::roundToInt(surface.width()/dpi));
    store(editorObject,0x44,juce::roundToInt(surface.height()/dpi));
  }
  struct Presentation {
    EngineSurface *surface;bool filtered;
    ~Presentation(){if(surface)surface->present(filtered);}
  } presentation{scaledSurface?&surface:nullptr,settings.get("antialias")>=64};
  const double submitStart=juce::Time::getMillisecondCounterHiRes();
  {
    std::lock_guard paletteGuard(paletteLock);
    if(workspace.load()==Workspace::lab) {
      applyNativeFields(editorObject, settings);
    }
    if(sceneFrame && !componentPainting.load() && field<int>(editorObject,0x2cd8)==13)
      prepareEngineTextFrame(editorObject,moduleBase,reinterpret_cast<void (*)(void *,void *)>(originalEditorVtable[30]));
    auto *palette = reinterpret_cast<juce::uint8 *>(moduleBase + 0x8d71b0);
    auto *pair = palette;
    constexpr int basicDefaults[]{0,11,3,15,8,17,1,11,16,10,12};
    const int index = settings.nativeDefaultColour ?
      (settings.category() == 4 ? basicDefaults[juce::jlimit(0,10,settings.variant())] : 9) :
      decodeChoice(settings.get("color"),20);
    pair += index * 6;
    std::array<juce::uint8,6> previous{};
    DWORD protection = 0;
    constexpr juce::uint8 signature[]{0xe8,0x11,0x2d,0xfc,0xe3,0x00};
    const bool paired = workspace.load()==Workspace::lab && (settings.category()==4 || settings.category()==8) &&
      std::memcmp(palette,signature,sizeof(signature))==0 &&
      VirtualProtect(pair,6,PAGE_READWRITE,&protection);
    if(paired) {
      std::copy_n(pair,6,previous.begin());
      for(int channel=0;channel<2;++channel) {
        auto colour=settings.colour(channel==0?"color":"color2");
        pair[channel*3]=colour.getRed(); pair[channel*3+1]=colour.getGreen(); pair[channel*3+2]=colour.getBlue();
      }
    }
    reinterpret_cast<void (*)(void *)>(originalVtable[2])(object);
    if(paired) {
      std::copy(previous.begin(),previous.end(),pair);
      DWORD unused=0;VirtualProtect(pair,6,protection,&unused);
    }
  }
  sceneCpuMs.store(juce::Time::getMillisecondCounterHiRes()-submitStart);
  // Capture this engine's own framebuffer on its rendering thread, before its
  // original JUCE controls are composited. No screen/window capture is involved.
  const auto library = GetModuleHandleW(L"opengl32.dll");
  auto get = reinterpret_cast<void (WINAPI *)(unsigned, int *)>(GetProcAddress(library, "glGetIntegerv"));
  auto read = reinterpret_cast<void (WINAPI *)(int, int, int, int, unsigned, unsigned, void *)>(GetProcAddress(library, "glReadPixels"));
  auto pack = reinterpret_cast<void (WINAPI *)(unsigned, int)>(GetProcAddress(library, "glPixelStorei"));
  auto getProc = reinterpret_cast<PROC (WINAPI *)(LPCSTR)>(GetProcAddress(library, "wglGetProcAddress"));
  if (!get || !read || !pack)
    return;
  const int swap = settings.get("vsync") >= 64 ? 1 : 0;
  if (swap != lastSwapInterval && getProc)
    if (auto swapInterval = reinterpret_cast<BOOL (WINAPI *)(int)>(getProc("wglSwapIntervalEXT"))) {
      swapInterval(swap);
      lastSwapInterval = swap;
    }
  // Visible native panels present their framebuffer directly. CPU readback is
  // needed only for recording; Lab always needs it to feed the custom viewport.
  if(!labFrame && !performancePlaying() && !captureFrames.load()) {++frameCount;return;}
  int viewport[4]{};
  get(0x0ba2, viewport);
  int width = viewport[2], height = viewport[3];
  int captureX=viewport[0],captureY=viewport[1];
  // Multiple uses a viewport per tile. Read the complete render surface, not
  // just the final tile's viewport, before native component UI composition.
  if(workspace.load()!=Workspace::lab) {
    auto dc=wglGetCurrentDC();
    RECT captureClient{};
    if(dc && GetClientRect(WindowFromDC(dc),&captureClient)) {
      width=captureClient.right-captureClient.left;height=captureClient.bottom-captureClient.top;
      captureX=0;captureY=0;
    }
  }
  if(scaledSurface) {
    surface.capture();width=surface.width();height=surface.height();captureX=captureY=0;
  }
  if (width < 1 || height < 1 || width > 4096 || height > 4096)
    return;
  readbackBytes.resize((size_t)width*height*4);
  auto &bytes=readbackBytes;
  int alignment = 4, rowLength = 0, skipRows = 0, skipPixels = 0;
  get(0x0d05, &alignment);
  get(0x0d02, &rowLength); get(0x0d03, &skipRows); get(0x0d04, &skipPixels);
  pack(0x0d05, 1);
  pack(0x0d02, 0); pack(0x0d03, 0); pack(0x0d04, 0);
  const double readStart=juce::Time::getMillisecondCounterHiRes();
  read(captureX, captureY, width, height, 0x80e1, 0x1401, bytes.data());
  const double copyStart=juce::Time::getMillisecondCounterHiRes();readbackCpuMs.store(copyStart-readStart);
  pack(0x0d05, alignment);
  pack(0x0d02, rowLength); pack(0x0d03, skipRows); pack(0x0d04, skipPixels);
  juce::Image captured(juce::Image::ARGB, width, height, false, juce::SoftwareImageType());
  juce::Image::BitmapData pixels(captured, juce::Image::BitmapData::writeOnly);
  for (int y = 0; y < height; ++y) {
    auto *row = bytes.data() + (size_t)(height - 1 - y) * width * 4;
    const auto *source = reinterpret_cast<const juce::uint32 *>(row);
    auto *destination = reinterpret_cast<juce::uint32 *>(pixels.getLinePointer(y));
    constexpr juce::uint32 alpha=0xffu<<(juce::PixelARGB::indexA*8);
    // Fuse the full-frame copy and opaque-alpha pass. This preserves every RGB
    // byte and permits vectorization instead of touching the image twice.
    for(int x=0;x<width;++x)destination[x]=source[x]|alpha;
  }
  copyCpuMs.store(juce::Time::getMillisecondCounterHiRes()-copyStart);
#ifdef GRID_ENGINE_PROBE
  if(frameCount.load()==0) {
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
      juce::PixelARGB expected;
      std::memcpy(&expected,bytes.data()+((size_t)(height-1-y)*width+x)*4,4);
      expected.setAlpha(255);
      juce::PixelARGB actual;
      std::memcpy(&actual,pixels.getPixelPointer(x,y),4);
      pixelCopyVerified=pixelCopyVerified && std::memcmp(&expected,&actual,4)==0;
    }
  }
#endif
  {
    juce::ScopedLock guard(frameLock);
    frame = captured;
    auto *data=new juce::DynamicObject();
#ifdef GRID_ENGINE_PROBE
    data->setProperty("pixelCopyEquivalent",pixelCopyVerified);
    auto gpuString=reinterpret_cast<const unsigned char *(WINAPI *)(unsigned)>(GetProcAddress(library,"glGetString"));
    if(gpuString){data->setProperty("gpuVendor",juce::String((const char *)gpuString(0x1f00)));
      data->setProperty("gpuRenderer",juce::String((const char *)gpuString(0x1f01)));}
#endif
    data->setProperty("renderWidth",width);data->setProperty("renderHeight",height);
    data->setProperty("category",field<int>(editorObject,0x2cd8));
    data->setProperty("basicColour",field<int>(editorObject,0x1254));
    data->setProperty("lyricCount",field<int>(editorObject,0x1ac4));
    data->setProperty("activeLyric",field<int>(editorObject,0x1d20));
    data->setProperty("customTypefacePresent",field<void *>(editorObject,0x1e80)!=nullptr);
    data->setProperty("glyphImagePresent",field<void *>(editorObject,0x1fa8)!=nullptr);
    data->setProperty("textStyle",field<int>(editorObject,0x1a3c));
    data->setProperty("textColour",field<int>(editorObject,0x1a60));
    data->setProperty("scopeStyle",field<int>(editorObject,0x42a0));
    data->setProperty("scopeTraceCount",(juce::int64)((field<uintptr_t>(editorObject,0x2c78)-field<uintptr_t>(editorObject,0x2c70))/40));
    data->setProperty("customColour",field<unsigned char>(editorObject,0x20aa)!=0);
    data->setProperty("requestedDefaultColour",settings.nativeDefaultColour);
    for(auto pair:{std::pair{0x3d4,"texture"},std::pair{0x480,"dither"},std::pair{0x514,"track"},std::pair{0x518,"glass"}})
      data->setProperty(pair.second,field<float>(editorObject,(size_t)pair.first));
    observed=juce::var(data);
  }
  ++frameCount;
#endif
}
void VelEngine::closeThunk(void *object) {
  VelEngine *engine=nullptr;
  {std::lock_guard guard(registryLock);auto found=renderers.find(object);if(found!=renderers.end())engine=found->second;}
  if(engine) {
    engine->panelWindow.store(nullptr);
    engine->surface.close();
    reinterpret_cast<void (*)(void *)>(engine->originalVtable[3])(object);
  }
}
void VelEngine::applyNativeFields(void *editor, const SceneState &s) {
#if JUCE_WINDOWS
  // These offsets are verified from the pinned renderer's state mirroring;
  // writes occur on its render thread. Public parameters are sent separately.
  const auto speed = s.get("speed") / 127.0f;
  auto exact=[&](const char* id,float fallback) {
    juce::ScopedLock guard(controlLock);
    auto entry=exactValues[id];
    return entry.isObject() && (int)entry["cc"]==s.get(id)?(float)entry["value"]:fallback;
  };
  const auto nativeSpeed=exact("speed",speed);
  for (size_t offset : {0xb94u, 0xe18u, 0xec0u, 0x1250u, 0x1650u, 0x1798u, 0x1a4cu})
    store(editor, offset, nativeSpeed);
  store(editor, 0x1e78, s.get("imageScale") / 127.0f);
  for (size_t offset : {0xec4u, 0x1a50u, 0x2094u})
    store(editor, offset, exact("glow",s.get("glow") / 127.0f));
  // Preserve the native Default's palette entry, rather than interpreting a
  // legacy GRID HSV swatch value as an unrelated native palette index.
  static constexpr int defaultIndices[]{6, 12, 9, 7, 0, 17, 0, 11, 9};
  static constexpr int basicIndices[]{0, 11, 3, 15, 8, 17, 1, 11, 16, 10, 12};
  const int colour = s.nativeDefaultColour ?
    (s.category() == 4 ? basicIndices[juce::jlimit(0, 10, s.variant())] : defaultIndices[s.category()]) :
    s.get("color") == 127 ? 18 : s.get("color") == 126 ? 19 : decodeChoice(s.get("color"), 20);
  // Synchronize the palette selector first, so the native parameter dispatch
  // does not clear a manual RGB override later in this same frame.
  reinterpret_cast<void (*)(void *, int)>(moduleBase + 0x50df90)(editor, colour);
  if (s.category() != 4 && s.category() != 8 && s.category() != 7 && !s.nativeDefaultColour) {
    store(editor,0x20aa,static_cast<unsigned char>(1));
    store(editor,0x20ab,s.colour("color").getARGB());
  }
  if(s.category()==4 || s.category()==7 || s.category()==8 || s.nativeDefaultColour) store(editor,0x20aa,static_cast<unsigned char>(0));
  if (s.category() == 7) {
    store(editor, 0x1a54, colour);
    store(editor, 0x1a60, s.get("textColor") == 127 ? 18 : s.get("textColor") == 126 ? 19 : decodeChoice(s.get("textColor"), 20));
  }
  store(editor, 0x1258, static_cast<unsigned char>(s.secondaryPalette));
  store(editor, 0x20a8, static_cast<unsigned char>(s.secondaryPalette));
  // The native stock-preset browser clamps its remembered selection to the
  // stock count after a category switch. Its file-drop path uses these reserved
  // entries, which must remain authoritative while the imported image is active.
  if(s.hasImage && s.category()==0)store(editor,0x42a0,15);
  if(s.hasImage && s.category()==2)store(editor,0xe10,8);
  juce::StringArray lines;
  juce::String signature;
  int active=0;
  {
    juce::ScopedLock guard(controlLock);
    lines=pendingLyrics; signature=lyricsSignature; active=activeLyric;
  }
  if(signature.isNotEmpty() && signature!=appliedLyrics) {
    // Use this module's String allocator/copy/release routines. Its empty
    // String singleton differs from the host's and must never be host-freed.
    using Construct=void (*)(void *,const char *);
    using Copy=void (*)(void *,const void *);
    using Destroy=void (*)(void *);
    auto construct=reinterpret_cast<Construct>(moduleBase+0x0267e0);
    auto copy=reinterpret_cast<Copy>(moduleBase+0x1fee60);
    auto destroy=reinterpret_cast<Destroy>(moduleBase+0x1ff8e0);
    for(int i=0;i<75;++i) {
      void *text=reinterpret_cast<void *>(moduleBase+0x982d18);
      construct(&text,i<lines.size()?lines[i].toRawUTF8():"");
      copy(static_cast<char *>(editor)+0x1ac8+i*8,&text);
      destroy(&text);
    }
    store(editor,0x1ac4,lines.size());
    store(editor,0x1d20,juce::jlimit(0,lines.size()-1,active));
    // Cache the same active index sent through TEXT BOX. Invalidating this
    // mirror lets the native timer restore its old parameter (usually line 0).
    store(editor,0x1d34,juce::jlimit(0,lines.size()-1,active));
    appliedLyrics=signature;
  }
#else
  juce::ignoreUnused(editor, s);
#endif
}
void VelEngine::updateGraphics(int width,int height,int frameCap,int filtering,int vsync) {
  juce::ScopedLock guard(controlLock);
  controls.width=width;controls.height=height;
  controls.values[(size_t)indexOf("frameCap")]=frameCap;
  controls.values[(size_t)indexOf("antialias")]=filtering;
  controls.values[(size_t)indexOf("vsync")]=vsync;
}
void VelEngine::update(const SceneState &s) {
  {
    juce::ScopedLock guard(controlLock);
    controls = s;
  }
  if (!plugin)
    return;
  if(workspace.load()!=Workspace::lab)return;
  auto send = [&](int index, float value) {
    value = juce::jlimit(0.0f, 1.0f, value);
    if (previousParameters[(size_t)index] == value &&
        std::abs(parameters[(size_t)index]->getValue()-value)<0.00001f)
      return;
    previousParameters[(size_t)index] = value;
    parameters[(size_t)index]->setValueNotifyingHost(value);
  };
  constexpr int nativeCategories[]{0, 1, 3, 4, 5, 6, 7, 8, 9};
  constexpr int textStyles[]{0, 0, 4, 9, 3, 5, 1, 10, 8, 6, 2, 7};
  constexpr int textAnimations[]{6, 0, 1, 2, 4, 5, 3};
  constexpr int textures[]{2, 1, 0, 3};
  send(19, nativeCategories[s.category()] / 9.0f);
  // Native file drops select the reserved image entries (Scope 15 and
  // Interstellar 8). Keep those entries while the imported asset is present;
  // otherwise the next host update replaces the trace with a stock preset.
  const int variant = s.hasImage && s.category() == 0 ? 15
                    : s.hasImage && s.category() == 2 ? 8
                    : s.category() == 7 ? textStyles[juce::jlimit(0, 11, s.variant())]
                    : s.variant();
  send(17, variant / 15.0f);
  auto exact=[&](const char* id,float fallback) {
    juce::ScopedLock guard(controlLock);
    auto entry=exactValues[id];
    return entry.isObject() && (int)entry["cc"]==s.get(id)?(float)entry["value"]:fallback;
  };
  for (auto pair : {std::pair{0, "in"}, {1, "int"}, {2, "glow"}, {3, "texture"},
                    {4, "dither"}, {5, "track"}, {6, "glass"}})
    send(pair.first, exact(pair.second,s.get(pair.second) / 127.0f));
  for (auto pair : {std::pair{7, "textureReact"}, {8, "ditherReact"},
                    {9, "trackReact"}, {10, "glassReact"}})
    send(pair.first, s.get(pair.second) >= 64 ? 1.0f : 0.0f);
  send(11, textures[decodeChoice(s.get("textureType"), 4)] / 3.0f);
  send(12, decodeChoice(s.get("ditherType"), 4) / 3.0f);
  send(13, decodeChoice(s.get("trackType"), 5) / 4.0f);
  send(14, decodeChoice(s.get("glassType"), 6) / 5.0f);
  int selectedLyric=0;
  {juce::ScopedLock guard(controlLock);selectedLyric=activeLyric;}
  send(20, selectedLyric/74.0f); // Native TEXT BOX is 0–74, independent of line count.
  send(15, 0); send(16, 0); send(21, 0); send(24, 0); // GRID owns interval clocks.
  constexpr int defaultIndices[]{6, 12, 9, 7, 0, 17, 0, 11, 9};
  constexpr int basicIndices[]{0, 11, 3, 15, 8, 17, 1, 11, 16, 10, 12};
  send(18, s.category()==7 ? (s.get("textColor")==127?18:s.get("textColor")==126?19:decodeChoice(s.get("textColor"),20))/19.0f : s.nativeDefaultColour ?
    (s.category() == 4 ? basicIndices[juce::jlimit(0, 10, s.variant())] : defaultIndices[s.category()]) / 19.0f :
    s.get("color") / 127.0f);
  if (s.category() == 7) send(23, s.nativeDefaultColour ? 11/19.0f : s.get("color") / 127.0f);
  send(22, textAnimations[decodeChoice(s.get("textEffect"), 7)] / 6.0f);
  // Lab's hidden host has stable dimensions. Preview size and adaptive quality
  // affect only the scene framebuffer, never VST3 window-size negotiation.
  dispatchImport();
}
void VelEngine::coverPanel(bool covered) {
#if JUCE_WINDOWS
  const bool changed=panelCovered.exchange(covered)!=covered;
  if(panelViewport)panelViewport->setVisible(!covered && engineEditor && engineEditor->getParentComponent()==panelViewport.get());
  if(!changed && !covered)return;
  // Keep the engine/context alive, but hide its complete native child tree.
  // GRID's modal frame is drawn by the host peer outside this child tree.
  auto window=static_cast<HWND>(panelWindow.load());
  if(auto container=nativePanelContainer(window,static_cast<HWND>(hostPeerWindow.load())))
    ShowWindow(container,covered?SW_HIDE:SW_SHOWNOACTIVATE);
  if(window && IsWindow(window) && (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD))
    ShowWindow(window,covered?SW_HIDE:SW_SHOWNOACTIVATE);
  if(!covered && editorObject)reinterpret_cast<void (*)(void *)>(moduleBase+0x39b520)(editorObject);
#else
  juce::ignoreUnused(covered);
#endif
}
void VelEngine::showWorkspace(Workspace mode,juce::Component *owner,juce::Rectangle<int> bounds,bool compactHeader) {
#if JUCE_WINDOWS
  if(!ready() || !engineEditor || !renderWindow)return;
  if(owner && mode!=Workspace::lab && bounds.isEmpty())return;
  const int header=compactHeader && mode!=Workspace::lab && owner?60:0;
  const bool cropChanged=header!=hiddenPanelHeader;
  hiddenPanelHeader=header;
  const bool visiblePanel=mode!=Workspace::lab && owner;
  if(visiblePanel) {
    auto *peer=owner->getPeer();if(!peer)return;
    if(!panelViewport) {
      panelViewport=std::make_unique<juce::Component>();
      panelViewport->setName("GRID native workspace viewport");
      panelViewport->setOpaque(true);
    }
    if(panelViewportOwner!=peer->getNativeHandle()) {
      // Detach the plugin before recreating a viewport for a different controls
      // window. Destroying a parent HWND must never destroy its plugin children.
      if(engineEditor->getParentComponent()==panelViewport.get())renderWindow->addAndMakeVisible(engineEditor.get());
      panelViewport->removeFromDesktop();
      panelViewport->addToDesktop(juce::ComponentPeer::windowIsTemporary,peer->getNativeHandle());
      panelViewportOwner=peer->getNativeHandle();
    }
    // This is a real child HWND, confined to the body. Both native GL and
    // JUCE's software wrapper paint are clipped by its client bounds. A region
    // on a full-window VST3 wrapper cannot protect the lightweight toolbar.
    panelViewport->setBounds(bounds.translated(peer->getAreaCoveredBy(*owner).getX(),peer->getAreaCoveredBy(*owner).getY()));
    panelViewport->setVisible(!panelCovered.load());
  } else if(panelViewport)panelViewport->setVisible(false);
  const auto desiredBounds=visiblePanel?juce::Rectangle<int>{0,-header,bounds.getWidth(),bounds.getHeight()+header}:juce::Rectangle<int>{0,0,600,600};
  auto *desiredParent=visiblePanel?panelViewport.get():renderWindow.get();
  if(!cropChanged && mode==workspace.load() && !homePending.load() &&
      engineEditor->getParentComponent()==desiredParent && engineEditor->getBounds()==desiredBounds &&
      viewportBounds==desiredBounds)return;
  struct Transition {
    std::atomic<bool> &changing;
    explicit Transition(std::atomic<bool> &value):changing(value){changing.store(true);}
    ~Transition(){changing.store(false);}
  } transition(presentationChanging);
  if(mode!=Workspace::lab && owner) {
    revealAfterPaint.store(panelPaintCount.load()+1);
    if(auto window=static_cast<HWND>(panelWindow.load());window && IsWindow(window) && (GetWindowLongPtrW(window,GWL_STYLE)&WS_CHILD))
      ShowWindow(window,SW_HIDE);
  } else revealAfterPaint.store(0);
  viewportBounds=desiredBounds;
  homePending.store(false);
  prepareReviewUi();
  const bool wantsPainting=mode!=Workspace::lab && owner!=nullptr;
  if(wantsPainting!=componentPainting)configureComponentPainting(wantsPainting);
  const bool changing=mode!=workspace.load();
  if(changing) {
    pendingImports.clear();
    // These are embedded TextEditor objects, not TextEditor pointers.
    // Match the native entry routine's Component subobject and virtual
    // setVisible call. Dereferencing the object's first word reads its vtable.
    for(size_t offset:{0x22e0u,0x2dc0u,0x3628u,0x3e10u}) {
      auto *component=static_cast<char *>(editorObject)+offset+8;
      auto **table=field<void **>(component,0);
      if(readable(component,16) && readable(table,12*sizeof(void *)))
        reinterpret_cast<void (*)(void *,bool)>(table[11])(component,false);
    }
    for(size_t offset:{0x2bf6u,0x2bf8u,0x2bfcu,0x2dbcu,0x2dbdu,0x2d65u,0x2d66u,0x2d4cu,0x2c2cu})store(editorObject,offset,(unsigned char)0);
    store(editorObject,0x2b54,(unsigned char)0);
    store(editorObject,0x2c13,(unsigned char)(mode==Workspace::multiple));
    store(editorObject,0x2c2f,(unsigned char)(mode==Workspace::performance));
    if(mode==Workspace::multiple) {
      store(editorObject,0x2c14,0);
      reinterpret_cast<void (*)(void *)>(moduleBase+0x480490)(editorObject);
    } else if(mode==Workspace::performance) {
      store(editorObject,0x2c30,0);
      store(editorObject,0x361c,-1);
      store(editorObject,0x3dcc,-1);
      reinterpret_cast<void (*)(void *)>(moduleBase+0x47fe70)(editorObject);
    }
  }
  workspace.store(mode);
  if(mode==Workspace::performance && !owner && field<int>(editorObject,0x2c30)==5)
    store(editorObject,0x2bf5,(unsigned char)1); // Native scene-only layout; GRID owns its window.
  if(mode==Workspace::lab || !owner) {
    if(engineEditor->getParentComponent()!=renderWindow.get()) {
      renderWindow->addAndMakeVisible(engineEditor.get());
      engineEditor->setBounds(0,0,600,600);
      renderWindow->setBounds(-30000,-30000,600,600);
      renderWindow->setVisible(true);
    }
    if(panelViewport) {panelViewport->removeFromDesktop();panelViewportOwner=nullptr;}
  } else {
    if(bounds.isEmpty())return;
    if(engineEditor->getParentComponent()!=panelViewport.get()) {
      renderWindow->setVisible(false);
      panelViewport->addAndMakeVisible(engineEditor.get());
    }
  }
  fitViewport();
  presentationChanging.store(false); // Publish complete layout before its first repaint.
  reinterpret_cast<void (*)(void *)>(moduleBase+0x39b520)(editorObject);
#else
  juce::ignoreUnused(mode,owner,bounds);
#endif
}
juce::Image VelEngine::keyboardGraphImage(int width,int height) {
#if JUCE_WINDOWS
  if(!ready() || width<1 || height<1)return {};
  if(cachedKeyboardGraph.getWidth()==width && cachedKeyboardGraph.getHeight()==height)return cachedKeyboardGraph;
  const std::lock_guard geometryGuard(geometryLock);
  const int previousWidth=field<int>(editorObject,0x40),previousHeight=field<int>(editorObject,0x44);
  store(editorObject,0x40,width);store(editorObject,0x44,height);
  cachedKeyboardGraph=renderKeyboardGraphImage(editorObject,moduleBase,width,height);
  store(editorObject,0x40,previousWidth);store(editorObject,0x44,previousHeight);
  return cachedKeyboardGraph;
#else
  juce::ignoreUnused(width,height);return {};
#endif
}
void VelEngine::prepareReviewUi() {
#if JUCE_WINDOWS
  if(!ready() || !editorObject || !moduleBase)return;
  // Private creator-review integration, authorized by the owner as reported
  // by the user. Keep the native activation card and input gate out of this UI.
  // Only this instance of the adjacent, fingerprint-checked module is changed;
  // no binary bytes, installed files, activation keys or preferences are copied.
  store(editorObject,0x3190,(unsigned char)0);
  auto *input=static_cast<char *>(editorObject)+0x31a0;
  auto **table=field<void **>(input,0);
  if(readable(input,16) && readable(table,12*sizeof(void *)))
    reinterpret_cast<void (*)(void *,bool)>(table[11])(input,false);
#endif
}
void VelEngine::configureComponentPainting(bool enabled) {
#if JUCE_WINDOWS
  if(!editorObject || !moduleBase)return;
  componentPainting=enabled;
  ++contextRestarts;
  auto *context=static_cast<char *>(editorObject)+0x1b0;
  // Verified 1.2.0 OpenGLContext constructor: renderComponents is +0x70.
  // Detach before changing it, as required by JUCE's context API.
  reinterpret_cast<void (*)(void *)>(moduleBase+0x5b65a0)(context);
  store(context,0x70,(unsigned char)enabled);
  reinterpret_cast<void (*)(void *,void *)>(moduleBase+0x5b6630)(context,editorObject);
#else
  juce::ignoreUnused(enabled);
#endif
}
bool VelEngine::requestedHome() const {
#if JUCE_WINDOWS
  return ready() && workspace.load()!=Workspace::lab && (homePending.load() || field<unsigned char>(editorObject,0x2b54)!=0);
#else
  return false;
#endif
}
bool VelEngine::forwardKey(const juce::KeyPress &key) {
#if JUCE_WINDOWS
  return ready() && workspace.load()!=Workspace::lab &&
    reinterpret_cast<juce::Component *>(editorObject)->keyPressed(key);
#else
  juce::ignoreUnused(key);return false;
#endif
}
int VelEngine::activeCategory() const {
#if JUCE_WINDOWS
  constexpr int nativeModes[]{10,15,17,14,12,18,19,13,11};
  const int native=field<int>(editorObject,0x2cd8);
  for(int i=0;i<9;++i)if(nativeModes[i]==native)return i;
#endif
  return -1;
}
void VelEngine::applyMappedControl(const juce::String &id,int cc,bool button) {
#if JUCE_WINDOWS
  if(!ready() || workspace.load()==Workspace::lab)return;
  cc=juce::jlimit(0,127,cc);
  if(id=="multiTile") {midiTile=decodeChoice(cc,36);return;}
  auto nativeParameter=[&](const char *name,float normalised,bool counter,int direction=1) {
    auto *processor=field<void *>(editorObject,0x1a8);
    auto *state=reinterpret_cast<void *(*)(void *,const char *)>(moduleBase+0x3a1f00)(
      static_cast<char *>(processor)+0x2578,name);
    if(!state)return;
    auto *parameter=field<void *>(state,0x38);
    if(!parameter)return;
    if(counter) {
      const int next=(juce::roundToInt(field<float>(state,0x98))+direction+500)%500;
      normalised=reinterpret_cast<float (*)(void *,float)>(moduleBase+0x2d44c0)(parameter,(float)next);
    }
    reinterpret_cast<void (*)(void *,float)>(moduleBase+0x2d66d0)(parameter,normalised);
  };
  if(button && workspace.load()==Workspace::performance && id.startsWith("perf")) {
    const auto name=id=="perfNext"||id=="perfPrevious"?"perfPresetNav":id.toRawUTF8();
    nativeParameter(name,0,true,id=="perfPrevious"?-1:1);
    return;
  }
  void *target=editorObject;
  bool tile=false;
  if(workspace.load()==Workspace::multiple) {
    auto *begin=field<char *>(editorObject,0x2d18),*end=field<char *>(editorObject,0x2d20);
    const auto count=begin && end>=begin?(end-begin)/0x168:0;
    if(count<=0 || count>36)return;
    target=begin+juce::jlimit(0,(int)count-1,midiTile)*0x168;
    tile=true;
  }
  const float value=cc/127.0f;
  const auto category=field<int>(target,tile?0:0x2cd8);
  auto modeOffset=[&](const char *id,size_t fallback) {
    if(!tile)return fallback;
    struct Slots {int category;size_t intensity,speed,glow,colour;};
    const Slots slots[]{
      {10,0x10,0,0x14,0x48},{15,0x70,0x74,0x14,0x78},
      {17,0xa0,0xa4,0x14,0xa8},{14,0xd4,0xd8,0xe0,0xdc},
      {12,0xf8,0xfc,0x14,0x100},{18,0x10c,0x110,0x14,0x114},
      {19,0x128,0x12c,0x14,0x130},{13,0x10,0,0x14,0x48},
      {11,0x148,0,0x14c,0x150}};
    for(auto slot:slots)if(slot.category==category)
      return juce::String(id)=="int"?slot.intensity:juce::String(id)=="glow"?slot.glow:
        juce::String(id)=="speed"?slot.speed:slot.colour;
    return fallback;
  };
  const struct { const char *id;size_t live,snapshot; } amounts[]{
    {"in",0x294,0xc},{"int",0x290,0x10},{"glow",0x280,0x14},
    {"texture",0x3d4,0x18},{"dither",0x480,0x1c},{"track",0x514,0x20},{"glass",0x518,0x24}};
  for(auto entry:amounts)if(id==entry.id) {
    auto offset=tile?entry.snapshot:entry.live;
    if(tile && (id=="int" || id=="glow"))offset=modeOffset(entry.id,offset);
    store(target,offset,value);
    if(!tile && id=="int")for(size_t at:{0xb90u,0xe14u,0xeb8u,0x124cu,0x164cu,0x1794u,0x1a48u,0x2090u})store(target,at,value);
    if(!tile && id=="glow")for(size_t at:{0xec4u,0x1a50u,0x2094u})store(target,at,value);
    if(!tile)nativeParameter(id=="in"?"inputGain":id=="int"?"intensity":entry.id,value,false);
    return;
  }
  const struct {const char *id;size_t live,snapshot;int count;} profiles[]{
    {"textureType",0x378,0x34,4},{"ditherType",0x3d0,0x30,4},
    {"trackType",0x3b4,0x2c,5},{"glassType",0x444,0x38,6}};
  for(auto entry:profiles)if(id==entry.id) {
    int choice=decodeChoice(cc,entry.count);
    if(id=="textureType") {constexpr int map[]{2,1,0,3};choice=map[choice];}
    store(target,tile?entry.snapshot:entry.live,choice);
    return;
  }
  const struct {const char *id;size_t live,snapshot;} reactive[]{
    {"textureReact",0x4c9,0x28},{"ditherReact",0x484,0x29},
    {"trackReact",0x4e4,0x2a},{"glassReact",0x4fc,0x2b}};
  for(auto entry:reactive)if(id==entry.id) {
    const auto offset=tile?entry.snapshot:entry.live;
    store(target,offset,(unsigned char)(button?!field<unsigned char>(target,offset):cc>=64));return;
  }
  if(id=="color") {
    if(tile)store(target,modeOffset("color",0x48),decodeChoice(cc,20));
    else reinterpret_cast<void (*)(void *,int)>(moduleBase+0x50df90)(editorObject,decodeChoice(cc,20));
  }
  if(id=="speed") {
    if(tile) {const auto at=modeOffset("speed",0);if(at)store(target,at,value);}
    else for(size_t at:{0xb94u,0xe18u,0xec0u,0x1250u,0x1650u,0x1798u,0x1a4cu})store(target,at,value);
  }
#else
  juce::ignoreUnused(id,cc,button);
#endif
}
juce::Result VelEngine::setLyrics(const juce::StringArray &lines, int active) {
  if(lines.isEmpty() || lines.size()>75)
    return juce::Result::fail("The native engine supports 1–75 lyric lines.");
  for(const auto& line:lines) if(line.length()>120)
    return juce::Result::fail("Native lyric lines are limited to 120 characters.");
  juce::ScopedLock guard(controlLock);
  pendingLyrics=lines;activeLyric=juce::jlimit(0,lines.size()-1,active);
  lyricsSignature=lines.joinIntoString("\n")+"\nactive="+juce::String(activeLyric);
  return juce::Result::ok();
}
void VelEngine::clearTextImage() {
#if JUCE_WINDOWS
  const std::lock_guard geometryGuard(geometryLock);
  pendingImports.clear();
  if(ready()) {
    void *empty=nullptr;
    reinterpret_cast<void (*)(void *,const void *)>(moduleBase+0x29f600)(
      static_cast<char *>(editorObject)+0x1e70,&empty);
  }
#endif
}
void VelEngine::pushAudio(const float *left, const float *right, int count, double rate) noexcept {
  if (!left || count <= 0 || !acceptingAudio.load())
    return;
  const juce::SpinLock::ScopedTryLockType guard(audioProducer);
  if (!guard.isLocked())
    return;
  inputRate.store(rate);
  int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
  audioFifo.prepareToWrite(count, start1, size1, start2, size2);
  for (int channel = 0; channel < 2; ++channel) {
    const auto *source = channel == 0 || !right ? left : right;
    std::copy_n(source, size1, audioRing[(size_t)channel].data() + start1);
    std::copy_n(source + size1, size2, audioRing[(size_t)channel].data() + start2);
  }
  audioFifo.finishedWrite(size1 + size2);
  audioAvailable.signal();
}
void VelEngine::clearTextFont() {
#if JUCE_WINDOWS
  const std::lock_guard geometryGuard(geometryLock);
  if(ready()) {
    void *empty=nullptr;
    reinterpret_cast<void (*)(void *,const void *)>(moduleBase+0x04c6b0)(
      static_cast<char *>(editorObject)+0x1e80,&empty);
    reinterpret_cast<void (*)(void *,const void *)>(moduleBase+0x29f600)(
      static_cast<char *>(editorObject)+0x1d78,&empty);
  }
#endif
}
void VelEngine::run() {
  juce::AudioBuffer<float> buffer(2, 512);
  juce::MidiBuffer midi;
  double rate = 48000;
  double lastProcessed = juce::Time::getMillisecondCounterHiRes();
  while (!threadShouldExit()) {
    bool silence = false;
    if (audioFifo.getNumReady() < 512) {
      audioAvailable.wait(5);
      if (audioFifo.getNumReady() < 512) {
        if (juce::Time::getMillisecondCounterHiRes() - lastProcessed < 1024.0 * 1000 / rate)
          continue;
        silence = true;
      }
    }
    const auto requested = inputRate.load();
    if (requested != rate) {
      plugin->releaseResources();
      plugin->setRateAndBufferSizeDetails(requested, 512);
      plugin->prepareToPlay(requested, 512);
      rate = requested;
    }
    if (silence) buffer.clear(); else {
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    audioFifo.prepareToRead(512, start1, size1, start2, size2);
    for (int channel = 0; channel < 2; ++channel) {
      std::copy_n(audioRing[(size_t)channel].data() + start1, size1, buffer.getWritePointer(channel));
      std::copy_n(audioRing[(size_t)channel].data() + start2, size2, buffer.getWritePointer(channel) + size1);
    }
    audioFifo.finishedRead(size1 + size2);
    }
    midi.clear();
    plugin->processBlock(buffer, midi);
    lastProcessed = juce::Time::getMillisecondCounterHiRes();
    audioFrames.fetch_add(512);
  }
}
juce::Result VelEngine::importFile(const juce::File &file) {
#if JUCE_WINDOWS
  const std::lock_guard geometryGuard(geometryLock);
  if (!ready() || !file.existsAsFile())
    return juce::Result::fail("Native import requires a ready Vel Engine and an existing file.");
  if(workspace.load()!=Workspace::lab) {
    auto *target=static_cast<char *>(editorObject)+0x170;
    auto **table=field<void **>(target,0);
    if(reinterpret_cast<uintptr_t>(table)!=moduleBase+0x8d8508)
      return juce::Result::fail("Vel Engine import interface is unavailable.");
    juce::StringArray files(file.getFullPathName());
    reinterpret_cast<void (*)(void *,const juce::StringArray &,int,int)>(table[5])(target,files,0,0);
  } else pendingImports.emplace_back(file,requestedScene().category());
  return juce::Result::ok();
#else
  juce::ignoreUnused(file);
  return juce::Result::fail("Vel Engine imports are Windows-only.");
#endif
}
void VelEngine::dispatchImport() {
#if JUCE_WINDOWS
  // Imports are queued and dispatched on the message thread. Normal control
  // updates must not wait for a full native GL/CPU FX frame with no import work.
  if(pendingImports.empty())return;
  // A real import keeps the original serialized path, so it cannot starve
  // behind continuously rendered heavy FX frames.
  const std::lock_guard geometryGuard(geometryLock);
  constexpr int nativeCategories[]{10,15,17,14,12,18,19,13,11};
  if(!ready())return;
  while(!pendingImports.empty()) {
    auto [file,category]=pendingImports.front();
    if(requestedScene().category()!=category) {
      pendingImports.pop_front();
      continue;
    }
    if(field<int>(editorObject,0x2cd8)!=nativeCategories[category])return;
  auto *target = static_cast<char *>(editorObject) + 0x170;
  auto **table = field<void **>(target, 0);
  if (reinterpret_cast<uintptr_t>(table) != moduleBase + 0x8d8508)
    return;
  // Forward product file imports to the original JUCE FileDragAndDropTarget;
  // this invokes its actual contour/font/image routines, with no synthetic input.
  juce::StringArray files(file.getFullPathName());
  reinterpret_cast<void (*)(void *, const juce::StringArray &, int, int)>(table[5])(target, files, 0, 0);
  pendingImports.pop_front();
  }
#endif
}
juce::Image VelEngine::latestFrame() const {
  juce::ScopedLock guard(frameLock);
  return frame;
}
juce::var VelEngine::observedState() const {
  juce::ScopedLock guard(frameLock);
  return observed;
}
void VelEngine::setExactValues(const juce::var &values) {
  juce::ScopedLock guard(controlLock);
  exactValues=values;
}
juce::var VelEngine::defaultProfile(int category,int variant) const {
#if JUCE_WINDOWS
  if(!ready() || category<0 || category>=9)return {};
  constexpr int nativeCategories[]{10,15,17,14,12,18,19,13,11};
  std::array<unsigned char,512> snapshot{};
  using Defaults=void (*)(void*,void*,int,int,bool);
  reinterpret_cast<Defaults>(moduleBase+0x465c70)(nullptr,snapshot.data(),nativeCategories[category],variant,false);
  auto number=[&](size_t offset){float value=0;std::memcpy(&value,snapshot.data()+offset,4);return value;};
  auto integer=[&](size_t offset){int value=0;std::memcpy(&value,snapshot.data()+offset,4);return value;};
  constexpr size_t intensity[]{0x10,0x70,0xa0,0xd4,0xf8,0x10c,0x128,0,0x148};
  constexpr size_t speed[]{0,0x74,0xa4,0xd8,0xfc,0x110,0x12c,0,0};
  constexpr size_t glow[]{0x14,0,0,0xe0,0,0,0,0,0x14c};
  constexpr size_t colour[]{0x48,0x78,0xa8,0xdc,0x100,0x114,0x130,0,0x150};
  auto *result=new juce::DynamicObject();
  auto add=[&](const char* id,float value){auto* entry=new juce::DynamicObject();entry->setProperty("value",value);entry->setProperty("cc",juce::roundToInt(value*127));result->setProperty(id,juce::var(entry));};
  add("in",number(0xc));
  add("int",category==7?.4f:number(intensity[category]));
  add("speed",speed[category]?number(speed[category]):.5f);
  add("glow",category==7?.5f:glow[category]?number(glow[category]):0.0f);
  result->setProperty("nativePalette",category==7?11:integer(colour[category]));
  result->setProperty("secondaryPalette",category==4?snapshot[0x104]!=0:false);
  result->setProperty("colourCycle",category==4?snapshot[0x105]!=0:false);
  result->setProperty("engineVersion","1.2.0");
  return juce::var(result);
#else
  juce::ignoreUnused(category,variant);return {};
#endif
}
juce::String VelEngine::diagnostic() const {
  if (failure.isNotEmpty())
    return failure;
  return "Vel Engine 1.2.0 / native frames " + juce::String(frames()) +
         " / audio frames " + juce::String(audioFrames.load())
#if JUCE_WINDOWS
         + " / native size " + juce::String(field<int>(editorObject, 0x40)) + "x" + juce::String(field<int>(editorObject, 0x44))
#endif
         ;
}
} // namespace kone::grid
