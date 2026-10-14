#include "EngineSurface.h"
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <cstring>

namespace kone::grid {
namespace {
constexpr GLenum framebufferTarget=0x8d40,drawTarget=0x8ca9,readTarget=0x8ca8;
using Bind=void (WINAPI *)(GLenum,GLuint);
using Generate=void (WINAPI *)(GLsizei,GLuint *);
using Destroy=void (WINAPI *)(GLsizei,const GLuint *);
using Attach=void (WINAPI *)(GLenum,GLenum,GLenum,GLuint,GLint);
using Check=GLenum (WINAPI *)(GLenum);
using Storage=void (WINAPI *)(GLenum,GLenum,GLsizei,GLsizei);
using AttachDepth=void (WINAPI *)(GLenum,GLenum,GLenum,GLuint);
using Blit=void (WINAPI *)(GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLint,GLbitfield,GLenum);
template<class T>T procedure(const char *name) {
  auto pointer=wglGetProcAddress(name);
  auto value=reinterpret_cast<uintptr_t>(pointer);
  return value>3 && value!=UINTPTR_MAX?reinterpret_cast<T>(pointer):nullptr;
}
struct Routing {
  GLuint output=0;
  bool active=false,bound=false;
  float x=1,y=1;
  std::array<int,4> viewport{};
};
thread_local Routing routing;
void WINAPI bindFramebuffer(GLenum target,GLuint value) {
  auto bind=procedure<Bind>("glBindFramebuffer");
  if(!bind)return;
  if(routing.active && value==0)value=routing.output;
  if(target==framebufferTarget || target==drawTarget)routing.bound=routing.active && value==routing.output;
  bind(target,value);
}
}
PROC WINAPI engineGlProcedure(LPCSTR name) {
  const auto original=wglGetProcAddress(name);
  if(original && std::strcmp(name,"glBindFramebuffer")==0)
    return reinterpret_cast<PROC>(&bindFramebuffer);
  return original;
}
void WINAPI engineViewport(GLint x,GLint y,GLsizei width,GLsizei height) {
  if(routing.active && routing.bound) {
    routing.viewport={x,y,width,height};
    glViewport(juce::roundToInt(x*routing.x),juce::roundToInt(y*routing.y),
      juce::roundToInt(width*routing.x),juce::roundToInt(height*routing.y));
  } else glViewport(x,y,width,height);
}
void WINAPI engineGlInteger(GLenum name,GLint *result) {
  if(name==GL_VIEWPORT && routing.active && routing.bound)
    std::copy(routing.viewport.begin(),routing.viewport.end(),result);
  else glGetIntegerv(name,result);
}
bool EngineSurface::begin(int width,int height,int targetWidth,int targetHeight,int virtualWidth,int virtualHeight) {
  auto bind=procedure<Bind>("glBindFramebuffer");
  auto generate=procedure<Generate>("glGenFramebuffers");
  auto attach=procedure<Attach>("glFramebufferTexture2D");
  auto check=procedure<Check>("glCheckFramebufferStatus");
  auto generateDepth=procedure<Generate>("glGenRenderbuffers");
  auto bindDepth=procedure<Bind>("glBindRenderbuffer");
  auto storage=procedure<Storage>("glRenderbufferStorage");
  auto attachDepth=procedure<AttachDepth>("glFramebufferRenderbuffer");
  if(!bind || !generate || !attach || !check || !generateDepth || !bindDepth || !storage || !attachDepth || !procedure<Blit>("glBlitFramebuffer"))return false;
  glGetIntegerv(0x8caa,&destinationRead);
  glGetIntegerv(0x8ca6,&destinationDraw);
  glGetIntegerv(GL_VIEWPORT,destinationViewport.data());
  if(!framebuffer)generate(1,&framebuffer);
  if(!texture)glGenTextures(1,&texture);
  if(!depth)generateDepth(1,&depth);
  targetWidth=juce::jmax(1,targetWidth);targetHeight=juce::jmax(1,targetHeight);
  width=juce::jlimit(320,3840,width);height=juce::jlimit(180,2160,height);
  GLint previousTexture=0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D,&previousTexture);
  glBindTexture(GL_TEXTURE_2D,texture);
  if(width!=renderWidth || height!=renderHeight) {
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    GLint previousDepth=0;glGetIntegerv(0x8ca7,&previousDepth);
    bindDepth(0x8d41,depth);storage(0x8d41,0x88f0,width,height);bindDepth(0x8d41,(GLuint)previousDepth);
  }
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  bind(framebufferTarget,framebuffer);
  attach(framebufferTarget,0x8ce0,GL_TEXTURE_2D,texture,0);
  attachDepth(framebufferTarget,0x821a,0x8d41,depth);
  const bool complete=check(framebufferTarget)==0x8cd5;
  glBindTexture(GL_TEXTURE_2D,(GLuint)previousTexture);
  if(!complete) {bind(readTarget,(GLuint)destinationRead);bind(drawTarget,(GLuint)destinationDraw);return false;}
  renderWidth=width;renderHeight=height;displayWidth=targetWidth;displayHeight=targetHeight;
  virtualWidth=virtualWidth>0?virtualWidth:targetWidth;
  virtualHeight=virtualHeight>0?virtualHeight:targetHeight;
  routing={framebuffer,true,true,(float)width/virtualWidth,(float)height/virtualHeight,{0,0,virtualWidth,virtualHeight}};
  glViewport(0,0,width,height);
  return true;
}
void EngineSurface::present(bool filtered) {
  routing.active=false;routing.bound=false;
  auto bind=procedure<Bind>("glBindFramebuffer");
  auto blit=procedure<Blit>("glBlitFramebuffer");
  if(bind && blit) {
    bind(readTarget,framebuffer);bind(drawTarget,(GLuint)destinationDraw);
    const bool scissor=glIsEnabled(GL_SCISSOR_TEST)!=0;
    glDisable(GL_SCISSOR_TEST);
    blit(0,0,renderWidth,renderHeight,0,0,displayWidth,displayHeight,GL_COLOR_BUFFER_BIT,filtered?GL_LINEAR:GL_NEAREST);
    bind(readTarget,(GLuint)destinationRead);bind(drawTarget,(GLuint)destinationDraw);
    glViewport(destinationViewport[0],destinationViewport[1],destinationViewport[2],destinationViewport[3]);
    if(scissor)glEnable(GL_SCISSOR_TEST);
  }
}
void EngineSurface::close() {
  routing={};
  if(framebuffer)if(auto destroy=procedure<Destroy>("glDeleteFramebuffers"))destroy(1,&framebuffer);
  if(texture)glDeleteTextures(1,&texture);
  if(depth)if(auto destroy=procedure<Destroy>("glDeleteRenderbuffers"))destroy(1,&depth);
  framebuffer=texture=depth=0;renderWidth=renderHeight=0;
}
juce::Image EngineSurface::frontImage() {
  RECT client{};auto dc=wglGetCurrentDC();
  if(!dc || !GetClientRect(WindowFromDC(dc),&client) || client.right<1 || client.bottom<1 || client.right>4096 || client.bottom>4096)return {};
  auto bind=procedure<Bind>("glBindFramebuffer");if(!bind)return {};
  GLint readFbo=0,readBuffer=0,alignment=0,rowLength=0,skipRows=0,skipPixels=0;
  glGetIntegerv(0x8caa,&readFbo);bind(readTarget,0);glGetIntegerv(GL_READ_BUFFER,&readBuffer);
  glGetIntegerv(GL_PACK_ALIGNMENT,&alignment);glGetIntegerv(GL_PACK_ROW_LENGTH,&rowLength);
  glGetIntegerv(GL_PACK_SKIP_ROWS,&skipRows);glGetIntegerv(GL_PACK_SKIP_PIXELS,&skipPixels);
  glReadBuffer(GL_FRONT);glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);
  glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);
  juce::Image image(juce::Image::ARGB,client.right,client.bottom,false,juce::SoftwareImageType());
  {juce::Image::BitmapData pixels(image,juce::Image::BitmapData::writeOnly);
    glReadPixels(0,0,client.right,client.bottom,0x80e1,GL_UNSIGNED_BYTE,pixels.data);
    std::vector<unsigned char> row((size_t)pixels.lineStride);
    for(int y=0;y<client.bottom/2;++y) {
      auto *a=pixels.getLinePointer(y),*b=pixels.getLinePointer(client.bottom-1-y);
      std::memcpy(row.data(),a,row.size());std::memcpy(a,b,row.size());std::memcpy(b,row.data(),row.size());
    }
    for(int y=0;y<client.bottom;++y)for(int x=0;x<client.right;++x)
      reinterpret_cast<juce::PixelARGB *>(pixels.getLinePointer(y))[x].setAlpha(255);
  }
  glReadBuffer((GLenum)readBuffer);bind(readTarget,(GLuint)readFbo);
  glPixelStorei(GL_PACK_ALIGNMENT,alignment);glPixelStorei(GL_PACK_ROW_LENGTH,rowLength);
  glPixelStorei(GL_PACK_SKIP_ROWS,skipRows);glPixelStorei(GL_PACK_SKIP_PIXELS,skipPixels);
  return image;
}
void EngineSurface::capture() {
  if(auto bind=procedure<Bind>("glBindFramebuffer"))bind(readTarget,framebuffer);
}
}
#else
namespace kone::grid {
bool EngineSurface::begin(int,int,int,int,int,int) { return false; }
void EngineSurface::present(bool) {}
void EngineSurface::capture() {}
juce::Image EngineSurface::frontImage() { return {}; }
void EngineSurface::close() {}
}
#endif
