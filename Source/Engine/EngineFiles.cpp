#include "EngineFiles.h"
#include "EngineSurface.h"
#if JUCE_WINDOWS
#include <windows.h>
#include <GL/gl.h>
#include <string>
#include <cwctype>
#include <cstring>

namespace kone::grid {
PROC WINAPI engineGlProcedure(LPCSTR);
void WINAPI engineViewport(GLint,GLint,GLsizei,GLsizei);
void WINAPI engineGlInteger(GLenum,GLint *);
namespace {
std::atomic<unsigned> performanceKeys{0};
SHORT WINAPI asyncKeyState(int key) {
  const auto state=performanceKeys.load();
  if(state&1u) {
    if(key=='X')return (state&2u)?(SHORT)0x8000:0;
    if(key=='B')return (state&4u)?(SHORT)0x8000:0;
  }
  return ::GetAsyncKeyState(key);
}
struct Paths { std::wstring provider, library, profile; };
// JUCE can retain a module after an editor closes. Paths must remain valid
// through DLL detach, when the native preferences may be finalised.
Paths &paths() { static auto *value = new Paths; return *value; }
struct LastError {
  DWORD value = ERROR_SUCCESS;
  ~LastError() { SetLastError(value); }
};
std::wstring mapped(LPCWSTR input) {
  if(!input)return {};
  std::wstring path(input), comparison(path);
  for(auto &c:comparison)c=c==L'/'?L'\\':(wchar_t)std::towlower(c);
  const auto &source=paths().provider;
  const size_t prefix=comparison.starts_with(L"\\\\?\\")?4:0;
  if(comparison.compare(prefix,source.size(),source)==0 &&
     (comparison.size()==prefix+source.size() || comparison[prefix+source.size()]==L'\\'))
    return paths().profile+path.substr(prefix+source.size());
  const auto &library=paths().library;
  if(comparison.compare(prefix,library.size(),library)==0 &&
     (comparison.size()==prefix+library.size() || comparison[prefix+library.size()]==L'\\'))
    return paths().profile+L"\\Library"+path.substr(prefix+library.size());
  return path;
}
HANDLE WINAPI createFile(LPCWSTR p,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES sa,DWORD d,DWORD f,HANDLE t) {
  LastError error;auto path=mapped(p);
  auto result=::CreateFileW(p?path.c_str():nullptr,a,s,sa,d,f,t);error.value=GetLastError();return result;
}
DWORD WINAPI attributes(LPCWSTR p) {
  LastError error;auto path=mapped(p);auto result=::GetFileAttributesW(p?path.c_str():nullptr);error.value=GetLastError();return result;
}
BOOL WINAPI attributesEx(LPCWSTR p,GET_FILEEX_INFO_LEVELS l,LPVOID data) {
  LastError error;auto path=mapped(p);auto result=::GetFileAttributesExW(p?path.c_str():nullptr,l,data);error.value=GetLastError();return result;
}
BOOL WINAPI createDirectory(LPCWSTR p,LPSECURITY_ATTRIBUTES sa) {
  LastError error;auto path=mapped(p);auto result=::CreateDirectoryW(p?path.c_str():nullptr,sa);error.value=GetLastError();return result;
}
BOOL WINAPI deleteFile(LPCWSTR p) {
  LastError error;auto path=mapped(p);auto result=::DeleteFileW(p?path.c_str():nullptr);error.value=GetLastError();return result;
}
BOOL WINAPI removeDirectory(LPCWSTR p) {
  LastError error;auto path=mapped(p);auto result=::RemoveDirectoryW(p?path.c_str():nullptr);error.value=GetLastError();return result;
}
BOOL WINAPI moveFile(LPCWSTR from,LPCWSTR to) {
  LastError error;auto a=mapped(from),b=mapped(to);auto result=::MoveFileW(from?a.c_str():nullptr,to?b.c_str():nullptr);error.value=GetLastError();return result;
}
BOOL WINAPI copyFile(LPCWSTR from,LPCWSTR to,BOOL fail) {
  LastError error;auto a=mapped(from),b=mapped(to);auto result=::CopyFileW(from?a.c_str():nullptr,to?b.c_str():nullptr,fail);error.value=GetLastError();return result;
}
BOOL WINAPI replaceFile(LPCWSTR old,LPCWSTR replacement,LPCWSTR backup,DWORD flags,LPVOID exclude,LPVOID reserved) {
  LastError error;auto a=mapped(old),b=mapped(replacement),c=mapped(backup);
  auto result=::ReplaceFileW(old?a.c_str():nullptr,replacement?b.c_str():nullptr,backup?c.c_str():nullptr,flags,exclude,reserved);error.value=GetLastError();return result;
}
HANDLE WINAPI findFirst(LPCWSTR p,LPWIN32_FIND_DATAW data) {
  LastError error;auto path=mapped(p);auto result=::FindFirstFileW(p?path.c_str():nullptr,data);error.value=GetLastError();return result;
}
struct Import { const char *name; void *replacement; };
}
bool connectEngineFiles(void *module,const juce::File &profile) {
  if(!module || !profile.createDirectory())return false;
  auto &configuration=paths();
  if(configuration.profile.empty()) {
    auto roaming=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    configuration.provider=roaming.getChildFile("Visual Lab Pro").getFullPathName().toWideCharPointer();
    configuration.library=roaming.getChildFile("Greyland Audio/Visual Lab").getFullPathName().toWideCharPointer();
    for(auto &c:configuration.provider)c=(wchar_t)std::towlower(c);
    for(auto &c:configuration.library)c=(wchar_t)std::towlower(c);
    configuration.profile=profile.getFullPathName().toWideCharPointer();
  } else if(configuration.profile!=profile.getFullPathName().toWideCharPointer())return false;
  auto *base=static_cast<unsigned char *>(module);
  auto *dos=reinterpret_cast<IMAGE_DOS_HEADER *>(base);
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return false;
  auto *pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(base+dos->e_lfanew);
  if(pe->Signature!=IMAGE_NT_SIGNATURE)return false;
  const Import replacements[]{
    {"CreateFileW",reinterpret_cast<void *>(&createFile)},
    {"GetFileAttributesW",reinterpret_cast<void *>(&attributes)},
    {"GetFileAttributesExW",reinterpret_cast<void *>(&attributesEx)},
    {"CreateDirectoryW",reinterpret_cast<void *>(&createDirectory)},
    {"DeleteFileW",reinterpret_cast<void *>(&deleteFile)},
    {"RemoveDirectoryW",reinterpret_cast<void *>(&removeDirectory)},
    {"MoveFileW",reinterpret_cast<void *>(&moveFile)},
    {"CopyFileW",reinterpret_cast<void *>(&copyFile)},
    {"ReplaceFileW",reinterpret_cast<void *>(&replaceFile)},
    {"FindFirstFileW",reinterpret_cast<void *>(&findFirst)},
    {"wglGetProcAddress",reinterpret_cast<void *>(&engineGlProcedure)},
    {"glViewport",reinterpret_cast<void *>(&engineViewport)},
    {"glGetIntegerv",reinterpret_cast<void *>(&engineGlInteger)},
    {"GetAsyncKeyState",reinterpret_cast<void *>(&asyncKeyState)}};
  auto *descriptor=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base+
    pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
  int connected=0;
  for(;descriptor->Name;++descriptor) {
    if(!descriptor->OriginalFirstThunk)continue;
    auto *names=reinterpret_cast<IMAGE_THUNK_DATA64 *>(base+descriptor->OriginalFirstThunk);
    auto *slots=reinterpret_cast<IMAGE_THUNK_DATA64 *>(base+descriptor->FirstThunk);
    for(;names->u1.AddressOfData;++names,++slots) {
      if(IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))continue;
      const auto *name=reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base+names->u1.AddressOfData);
      for(const auto &entry:replacements)if(std::strcmp(reinterpret_cast<const char *>(name->Name),entry.name)==0) {
        DWORD previous=0;
        if(!VirtualProtect(&slots->u1.Function,sizeof(ULONGLONG),PAGE_READWRITE,&previous))return false;
        slots->u1.Function=reinterpret_cast<ULONGLONG>(entry.replacement);
        DWORD ignored=0;VirtualProtect(&slots->u1.Function,sizeof(ULONGLONG),previous,&ignored);
        ++connected;
      }
    }
  }
  return connected==14;
}
void setEngineHeldKeys(bool active,bool strobes,bool blackout) {
  performanceKeys.store((active?1u:0u)|(strobes?2u:0u)|(blackout?4u:0u));
}
}
#else
namespace kone::grid {
bool connectEngineFiles(void *,const juce::File &) { return false; }
void setEngineHeldKeys(bool,bool,bool) {}
}
#endif
