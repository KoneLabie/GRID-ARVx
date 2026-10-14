#include "PanelTheme.h"
#include <array>
#include <algorithm>
#include <cstring>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace kone::grid {
#if defined(_WIN32)
namespace {
// Verified native Graphics layout: context +8, pending save +16. The native
// LowLevelGraphicsContext interface has 39 slots, including its destructor.
// FillType is 48 bytes: colour +0, gradient +8, image +16, transform +24.
// Never assign host JUCE fonts, Strings, Images or FillTypes to this module.
struct PaintScope;
thread_local PaintScope *currentPaint=nullptr;
template<typename T>T load(const void *p,size_t offset=0) {
  T value{};std::memcpy(&value,static_cast<const char *>(p)+offset,sizeof(value));return value;
}
template<typename T>void write(void *p,size_t offset,T value) {
  std::memcpy(static_cast<char *>(p)+offset,&value,sizeof(value));
}
std::uint32_t panelColour(std::uint32_t colour) {
  const auto alpha=colour&0xff000000u,rgb=colour&0xffffffu;
  if(rgb==0x101011 || rgb==0x2c2c30 || rgb==0x858589 || rgb==0x00dd91 || rgb==0xffffff)return colour;
  const int r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
  if(g>140 && r<70 && b<170)return alpha|0x00dd91;
  if(std::max({r,g,b})-std::min({r,g,b})<12) {
    if(r<=48)return alpha|0x101011;
    if(r<=78)return alpha|0x2c2c30;
    if(r<=170)return alpha|0x858589;
    if(r>=210)return alpha|0xffffff;
  }
  return colour;
}
struct PaintScope {
  void *context=nullptr;
  void **original=nullptr;
  // Retain the RTTI locator at vtable[-1], as native dynamic casts expect it.
  std::array<void *,40> table{};
  alignas(8) std::array<unsigned char,48> fill{};
  bool solid=false;
  PaintScope *previous=nullptr;
  static void setFill(void *context,const void *value) {
    auto &scope=*currentPaint;
    alignas(8) std::array<unsigned char,48> mapped{};
    std::memcpy(mapped.data(),value,mapped.size());
    scope.solid=load<void *>(value,8)==nullptr && load<void *>(value,16)==nullptr;
    if(scope.solid) {
      write(mapped.data(),0,panelColour(load<std::uint32_t>(value)));
      scope.fill=mapped;
    }
    reinterpret_cast<void (*)(void *,const void *)>(scope.original[17])(context,mapped.data());
  }
  static void rounded(void *context,const void *rectangle,float radius,float line) {
    auto &scope=*currentPaint;
    const float width=load<float>(rectangle,8),height=load<float>(rectangle,12);
    // Button outlines use the same filled charcoal surface, 10px corners and
    // 1.2px border as Lab. Preserve large cards, previews and graphical marks.
    if(scope.solid && width>=45 && height>=22 && height<=80 && line>0 && line<=2.1f) {
      auto background=scope.fill;write(background.data(),0,(std::uint32_t)0xff2c2c30);
      auto set=reinterpret_cast<void (*)(void *,const void *)>(scope.original[17]);
      set(context,background.data());
      radius=std::min(10.0f,height*.5f);line=1.2f;
      reinterpret_cast<void (*)(void *,const void *,float)>(scope.original[35])(context,rectangle,radius);
      set(context,scope.fill.data());
    }
    reinterpret_cast<void (*)(void *,const void *,float,float)>(scope.original[34])(context,rectangle,radius,line);
  }
  explicit PaintScope(void *graphics) {
    context=load<void *>(graphics,8);if(!context)return;
    original=load<void **>(context);
    MEMORY_BASIC_INFORMATION info{};
    if(!original || !VirtualQuery(original-1,&info,sizeof(info)) || info.State!=MEM_COMMIT ||
       (info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
       reinterpret_cast<std::uintptr_t>(original+39)>reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize) {
      context=nullptr;return;
    }
    std::copy_n(original-1,table.size(),table.begin());
    table[18]=reinterpret_cast<void *>(&setFill); // interface slot 17 + RTTI
    table[35]=reinterpret_cast<void *>(&rounded); // interface slot 34 + RTTI
    previous=currentPaint;currentPaint=this;write(context,0,table.data()+1);
  }
  ~PaintScope(){if(context){write(context,0,original);currentPaint=previous;}}
};
void drawButton(void *graphics,std::uintptr_t module,const char *text,float x,float width) {
  auto *context=load<void *>(graphics,8);auto **table=load<void **>(context);
  auto colour=reinterpret_cast<void (*)(void *,std::uint32_t)>(module+0x2a6e10);
  auto fill=reinterpret_cast<void (*)(void *,float,float,float,float,float)>(module+0x2a3de0);
  auto outline=reinterpret_cast<void (*)(void *,float,float,float,float,float,float)>(module+0x2a3d70);
  const auto pending=load<unsigned char>(graphics,16);write(graphics,16,(unsigned char)0);
  reinterpret_cast<void (*)(void *)>(table[13])(context);
  colour(graphics,0xff101011);fill(graphics,x-2,12,width+4,38,0);
  colour(graphics,0xff2c2c30);fill(graphics,x,14,width,34,10);
  colour(graphics,0xff858589);outline(graphics,x,14,width,34,10,1.2f);
  // Native constructors/destructors preserve the module's ownership rules.
  void *family=nullptr,*label=nullptr,*font=nullptr;
  // 0x22fff0 constructs FontOptions (104 bytes), not the pointer-sized Font.
  // Font's by-value constructor at 0x22faf0 consumes/destroys those options.
  alignas(8) std::array<unsigned char,104> options{};
  auto string=reinterpret_cast<void (*)(void *,const char *)>(module+0x267e0);
  auto releaseString=reinterpret_cast<void (*)(void *)>(module+0x1ff8e0);
  string(&family,"Arial");string(&label,text);
  reinterpret_cast<void (*)(void *,const void *,float,int)>(module+0x22fff0)(options.data(),&family,12,1);
  reinterpret_cast<void (*)(void *,void *)>(module+0x22faf0)(&font,options.data());
  reinterpret_cast<void (*)(void *,const void *)>(module+0x2a6a10)(graphics,&font);
  reinterpret_cast<void (*)(void *,const void *,int,int,int,int,int,bool)>(module+0x2a53b0)(
      graphics,&label,(int)x,14,(int)width,34,0x24,true);
  reinterpret_cast<void (*)(void *)>(module+0x22f860)(&font);
  releaseString(&label);releaseString(&family);
  reinterpret_cast<void (*)(void *)>(table[14])(context);write(graphics,16,pending);
}
}
#endif
void paintEnginePanel(void *editor,void *graphics,std::uintptr_t module,
                      void (*paint)(void *,void *),bool menu) {
#if defined(_WIN32)
  PaintScope scope(graphics);
  paint(editor,graphics);
  if(menu && scope.context)drawButton(graphics,module,"MENU",14,84);
  if(scope.context && load<unsigned char>(editor,0x2c2f) && load<int>(editor,0x2c30)==0)
    drawButton(graphics,module,"MAPPING",104,128);
#else
  (void)module;(void)menu;paint(editor,graphics);
#endif
}
void paintEngineKeyboardGraph(void *editor,void *graphics,std::uintptr_t module) {
#if defined(_WIN32)
  PaintScope scope(graphics);
  reinterpret_cast<void (*)(void *,std::uint32_t)>(module+0x2b3e90)(graphics,0xff1c1c20);
  // Draw the original keyboard diagram using native Graphics/font ownership.
  // Its standalone paint routine does not change the running setlist/live state.
  const auto width=(float)load<int>(editor,0x40);
  reinterpret_cast<void (*)(void *,void *,float,float,float)>(module+0x472d50)
      (editor,graphics,22.0f,20.0f,width-44.0f);
#else
  (void)editor;(void)graphics;(void)module;
#endif
}
void prepareEngineTextFrame(void *editor,std::uintptr_t module,void (*paint)(void *,void *)) {
#if defined(_WIN32)
  // Text is a hybrid renderer: the native paint pass builds the styled lyric
  // image at editor +0x1fa8, then the GL scene uploads it and applies the FX.
  // Lab disables native UI composition, so run that producer against a clipped
  // software target before GL. Keep native fonts/images in their owning module.
  void *imageType=nullptr,*image=nullptr;
  reinterpret_cast<void (*)(void *)>(module+0x29f980)(&imageType);
  reinterpret_cast<void (*)(void *,int,int,int,bool,void *)>(module+0x29f660)(&image,2,1,1,true,&imageType);
  if(!image)return;
  auto *context=reinterpret_cast<void *(*)(size_t)>(module+0x1a0f40)(40);
  reinterpret_cast<void (*)(void *,const void *)>(module+0x2a2e10)(context,&image);
  alignas(8) std::array<unsigned char,24> graphics{};
  write(graphics.data(),8,context);
  paint(editor,graphics.data());
  auto **table=load<void **>(context);
  reinterpret_cast<void (*)(void *,unsigned)>(table[0])(context,1);
  void *empty=nullptr;
  reinterpret_cast<void (*)(void *,const void *)>(module+0x29f600)(&image,&empty);
#else
  (void)editor;(void)module;(void)paint;
#endif
}
juce::Image renderKeyboardGraphImage(void *editor,std::uintptr_t module,int width,int height) {
#if defined(_WIN32)
  if(!editor || width<1 || height<1 || width>2048 || height>1536)return {};
  // Pinned native SoftwareImageType, Image and software renderer constructors.
  // All native allocations/fonts are destroyed by the module that owns them.
  void *imageType=nullptr,*nativeImage=nullptr;
  reinterpret_cast<void (*)(void *)>(module+0x29f980)(&imageType);
  reinterpret_cast<void (*)(void *,int,int,int,bool,void *)>(module+0x29f660)
      (&nativeImage,2,width,height,true,&imageType);
  if(!nativeImage)return {};
  auto *context=reinterpret_cast<void *(*)(size_t)>(module+0x1a0f40)(40);
  reinterpret_cast<void (*)(void *,const void *)>(module+0x2a2e10)(context,&nativeImage);
  struct NativeLifetime {
    std::uintptr_t module;void *context;void **image;
    ~NativeLifetime() {
      auto **table=load<void **>(context);
      reinterpret_cast<void (*)(void *,unsigned)>(table[0])(context,1);
      void *empty=nullptr;
      reinterpret_cast<void (*)(void *,const void *)>(module+0x29f600)(image,&empty);
    }
  } lifetime{module,context,&nativeImage};
  // Graphics borrowed-context layout: owned context +0 (null), context +8,
  // deferred-save flag +16. This view never takes ownership of native fonts.
  alignas(8) std::array<unsigned char,24> graphics{};
  write(graphics.data(),8,context);
  paintEngineKeyboardGraph(editor,graphics.data(),module);
  alignas(8) std::array<unsigned char,48> bitmap{};
  reinterpret_cast<void (*)(void *,const void *,int,int,int,int,int)>(module+0x29eef0)
      (bitmap.data(),&nativeImage,0,0,width,height,0);
  auto *pixels=load<const unsigned char *>(bitmap.data());
  const auto size=load<size_t>(bitmap.data(),8);
  const int format=load<int>(bitmap.data(),16),stride=load<int>(bitmap.data(),20),pixelStride=load<int>(bitmap.data(),24);
  juce::Image result;
  if(pixels && format==2 && pixelStride==4 && stride>=width*4 &&
      size>=(size_t)(height-1)*stride+(size_t)width*4) {
    result=juce::Image(juce::Image::ARGB,width,height,false,juce::SoftwareImageType());
    juce::Image::BitmapData destination(result,juce::Image::BitmapData::writeOnly);
    for(int y=0;y<height;++y)std::memcpy(destination.getLinePointer(y),pixels+(size_t)y*stride,(size_t)width*4);
  }
  if(auto *releaser=load<void *>(bitmap.data(),40)) {
    auto **table=load<void **>(releaser);
    reinterpret_cast<void (*)(void *,unsigned)>(table[0])(releaser,1);
  }
  return result;
#else
  (void)editor;(void)module;(void)width;(void)height;return {};
#endif
}
}
