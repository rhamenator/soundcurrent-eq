// SPDX-License-Identifier: GPL-3.0-only
#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <devguid.h>
#include <sddl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wintrust.h>
#include <softpub.h>
#include <tlhelp32.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cwctype>
#include "../../../src/windows_audio_session_gate.h"
namespace fs=std::filesystem;
constexpr wchar_t hardware[]=L"Root\\SOUNDCURRENTVAD";
constexpr wchar_t ownersKey[]=L"Software\\SoundCurrent\\AudioDriver\\Owners";
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
// Serialize installation/removal across apps and interactive sessions. A bounded
// wait lets an interrupted installer release its abandoned mutex automatically.
struct DriverTransaction {
 HANDLE mutex=nullptr;
 DriverTransaction(){
  mutex=CreateMutexW(nullptr,FALSE,L"Global\\SoundCurrent.AudioDriver.Transaction.v1");
  require(mutex!=nullptr,"Cannot open shared driver transaction lock");
  DWORD wait=WaitForSingleObject(mutex,15000);
  if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){CloseHandle(mutex);mutex=nullptr;throw std::runtime_error("Another driver setup is busy; try again after it finishes");}
 }
 ~DriverTransaction(){if(mutex){ReleaseMutex(mutex);CloseHandle(mutex);}}
};
struct Devices {
 HDEVINFO handle=SetupDiGetClassDevsW(&GUID_DEVCLASS_MEDIA,nullptr,nullptr,0);
 ~Devices(){if(handle!=INVALID_HANDLE_VALUE)SetupDiDestroyDeviceInfoList(handle);}
 std::vector<SP_DEVINFO_DATA> own(){
  require(handle!=INVALID_HANDLE_VALUE,"Cannot enumerate audio devices");
  std::vector<SP_DEVINFO_DATA> result;
  for(DWORD i=0;;++i){
   SP_DEVINFO_DATA d{};d.cbSize=sizeof(d);
   if(!SetupDiEnumDeviceInfo(handle,i,&d)){require(GetLastError()==ERROR_NO_MORE_ITEMS,"Audio enumeration failed");break;}
   wchar_t ids[4096]{};DWORD type=0,bytes=0;
   if(!SetupDiGetDeviceRegistryPropertyW(handle,&d,SPDRP_HARDWAREID,&type,reinterpret_cast<BYTE*>(ids),sizeof(ids),&bytes))continue;
   if(type!=REG_MULTI_SZ||bytes<2*sizeof(wchar_t)||bytes>sizeof(ids)||bytes%sizeof(wchar_t)||
      ids[bytes/sizeof(wchar_t)-1]!=0||ids[bytes/sizeof(wchar_t)-2]!=0)continue;
   for(const wchar_t* id=ids;*id;id+=wcslen(id)+1)
    if(!_wcsicmp(id,hardware)){result.push_back(d);break;}
  }
  return result;
 }
};
void checkNoApps(){
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
 require(snapshot!=INVALID_HANDLE_VALUE,"Cannot check running apps");
 PROCESSENTRY32W p{};p.dwSize=sizeof(p);bool active=false;
 const BOOL first=Process32FirstW(snapshot,&p);
 if(first)do{
  if(!_wcsicmp(p.szExeFile,L"soundcurrent-eq.exe")||!_wcsicmp(p.szExeFile,L"soundcurrent-studio.exe")||!_wcsicmp(p.szExeFile,L"soundcurrent-route-guardian.exe")){active=true;break;}
 }while(Process32NextW(snapshot,&p));
 const DWORD enumerationError=GetLastError();
 CloseHandle(snapshot);
 require(active || enumerationError==ERROR_NO_MORE_FILES,"Running app enumeration failed; driver setup stopped");
 require(!active,"Quit EQ and Studio before changing the shared audio driver");
}
std::wstring userSid(){
 HANDLE token=nullptr;require(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token),"Cannot read owner identity");
 DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<BYTE> bytes(size);
 bool ok=GetTokenInformation(token,TokenUser,bytes.data(),size,&size);CloseHandle(token);require(ok,"Cannot read owner SID");
 LPWSTR text=nullptr;require(ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(bytes.data())->User.Sid,&text),"Invalid owner SID");
 std::wstring result=text;LocalFree(text);return result;
}
// Per-user installers resolve the original user's SID before UAC. An alternate
// administrator token must not silently acquire that user's driver ownership.
std::wstring validatedSid(const wchar_t* text){
 require(text&&wcslen(text)<=184,"Invalid installer owner SID");
 PSID sid=nullptr;require(ConvertStringSidToSidW(text,&sid),"Invalid installer owner SID");
 LPWSTR normalized=nullptr;bool ok=IsValidSid(sid)&&ConvertSidToStringSidW(sid,&normalized);LocalFree(sid);
 require(ok,"Invalid installer owner SID");std::wstring result=normalized;LocalFree(normalized);return result;
}
struct Owners{
 HKEY key=nullptr;
 Owners(){require(RegCreateKeyExW(HKEY_LOCAL_MACHINE,ownersKey,0,nullptr,0,KEY_READ|KEY_WRITE|KEY_WOW64_64KEY,nullptr,&key,nullptr)==ERROR_SUCCESS,"Administrator approval is required for driver setup");}
 ~Owners(){if(key)RegCloseKey(key);}
 void add(const std::wstring& name){DWORD one=1;require(RegSetValueExW(key,name.c_str(),0,REG_DWORD,reinterpret_cast<BYTE*>(&one),sizeof(one))==ERROR_SUCCESS,"Cannot register app ownership");require(RegFlushKey(key)==ERROR_SUCCESS,"Cannot persist app ownership before driver changes");}
 bool has(const std::wstring& name){return RegQueryValueExW(key,name.c_str(),nullptr,nullptr,nullptr,nullptr)==ERROR_SUCCESS;}
 void remove(const std::wstring& name){auto status=RegDeleteValueW(key,name.c_str());require(status==ERROR_SUCCESS||status==ERROR_FILE_NOT_FOUND,"Cannot release app ownership");require(RegFlushKey(key)==ERROR_SUCCESS,"Cannot persist app ownership release; retry removal");}
 DWORD count(){DWORD result=0;require(RegQueryInfoKeyW(key,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&result,nullptr,nullptr,nullptr,nullptr)==ERROR_SUCCESS,"Cannot inspect shared driver owners");return result;}
};
// The protected journal includes every imported version, so an interrupted
// update cannot orphan an old package. Cleanup refuses names outside OEM INF.
struct Packages {
 HKEY key=nullptr;
 Packages(){require(RegCreateKeyExW(HKEY_LOCAL_MACHINE,L"Software\\SoundCurrent\\AudioDriver\\Packages",0,nullptr,0,KEY_READ|KEY_WRITE|KEY_WOW64_64KEY,nullptr,&key,nullptr)==ERROR_SUCCESS,"Cannot open driver package journal");}
 ~Packages(){if(key)RegCloseKey(key);}
 static bool validName(const std::wstring& name){
  if(name.size()<8||name.size()>40||name.substr(0,3)!=L"oem"||name.substr(name.size()-4)!=L".inf")return false;
  for(size_t i=3;i<name.size()-4;++i)if(name[i]<L'0'||name[i]>L'9')return false;
  return true;
 }
 void add(const std::wstring& name){require(validName(name),"Windows returned an invalid OEM INF name");DWORD one=1;require(RegSetValueExW(key,name.c_str(),0,REG_DWORD,reinterpret_cast<BYTE*>(&one),sizeof(one))==ERROR_SUCCESS,"Cannot journal driver package");require(RegFlushKey(key)==ERROR_SUCCESS,"Cannot persist driver package journal before binding");}
 std::vector<std::wstring> list(){
  std::vector<std::wstring> names;
  for(DWORD i=0;;++i){wchar_t name[64]{};DWORD length=64;auto result=RegEnumValueW(key,i,name,&length,nullptr,nullptr,nullptr,nullptr);if(result==ERROR_NO_MORE_ITEMS)break;require(result==ERROR_SUCCESS,"Invalid driver package journal");require(validName(name),"Unsafe OEM INF journal entry");names.emplace_back(name,length);}
  return names;
 }
 void cleanup(){
  wchar_t windows[MAX_PATH]{};require(GetWindowsDirectoryW(windows,MAX_PATH)!=0,"Windows directory unavailable");
  for(const auto& name:list()){
   const auto path=fs::path(windows)/L"INF"/name;
   if(fs::exists(path)){
    HINF inf=SetupOpenInfFileW(path.c_str(),L"MEDIA",INF_STYLE_WIN4,nullptr);
    require(inf!=INVALID_HANDLE_VALUE,"Cannot inspect journaled INF");
    INFCONTEXT line{};wchar_t id[128]{},catalog[128]{};
    bool ours=SetupFindFirstLineW(inf,L"SoundCurrent.NTamd64.10.0...22621",nullptr,&line)&&
      SetupGetStringFieldW(&line,2,id,128,nullptr)&&!_wcsicmp(id,hardware)&&
      SetupFindFirstLineW(inf,L"Version",L"CatalogFile",&line)&&
      SetupGetStringFieldW(&line,1,catalog,128,nullptr)&&!_wcsicmp(catalog,L"soundcurrentvad.cat");
    SetupCloseInfFile(inf);require(ours,"Journaled INF does not belong to SoundCurrent; cleanup stopped");
    // Flags zero: Windows refuses removal while any device still uses it.
    require(SetupUninstallOEMInfW(name.c_str(),0,nullptr),"Driver Store cleanup failed; retry removal after restart");
   }
   require(RegDeleteValueW(key,name.c_str())==ERROR_SUCCESS,"Cannot retire package journal entry");
   require(RegFlushKey(key)==ERROR_SUCCESS,"Cannot persist package journal retirement; retry removal");
  }
 }
};
std::wstring importPackage(const fs::path& package) {
 wchar_t imported[MAX_PATH]{};wchar_t* filename=nullptr;
 const BOOL copied=SetupCopyOEMInfW((package/L"soundcurrentvad.inf").c_str(),nullptr,
     SPOST_PATH,SP_COPY_NOOVERWRITE,imported,MAX_PATH,nullptr,&filename);
 const DWORD error=copied?ERROR_SUCCESS:GetLastError();
 require(copied||error==ERROR_FILE_EXISTS,"Cannot import trusted driver package");
 require(filename!=nullptr,"Imported driver package name unavailable");
 require(Packages::validName(filename),"Invalid published driver package name");
 return filename;
}
fs::path stagingBase() {
 PWSTR programFiles=nullptr;require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles,KF_FLAG_DEFAULT,nullptr,&programFiles)),"Program Files unavailable");
 fs::path base=fs::path(programFiles)/L"SoundCurrent"/L"AudioDriver";CoTaskMemFree(programFiles);return base;
}
void verifyPackageIdentity(const fs::path& directory) {
 HINF inf=SetupOpenInfFileW((directory/L"soundcurrentvad.inf").c_str(),L"MEDIA",INF_STYLE_WIN4,nullptr);
 require(inf!=INVALID_HANDLE_VALUE,"Driver INF is not a valid audio package");
 auto field=[&](const wchar_t* section,const wchar_t* key,DWORD index,const wchar_t* expected){
  INFCONTEXT line{};wchar_t value[256]{};
  return SetupFindFirstLineW(inf,section,key,&line)&&SetupGetStringFieldW(&line,index,value,256,nullptr)&&!_wcsicmp(value,expected);
 };
 const bool ours=
  field(L"Version",L"ClassGUID",1,L"{4d36e96c-e325-11ce-bfc1-08002be10318}")&&
  field(L"Version",L"CatalogFile",1,L"soundcurrentvad.cat")&&
  SetupGetLineCountW(inf,L"Manufacturer")==1&&
  field(L"Manufacturer",nullptr,1,L"SoundCurrent")&&
  field(L"Manufacturer",nullptr,2,L"NTamd64.10.0...22621")&&
  SetupGetLineCountW(inf,L"SoundCurrent.NTamd64.10.0...22621")==1&&
  field(L"SoundCurrent.NTamd64.10.0...22621",nullptr,1,L"SoundCurrent")&&
  field(L"SoundCurrent.NTamd64.10.0...22621",nullptr,2,hardware)&&
  SetupGetLineCountW(inf,L"SoundCurrent.NT.Services")==1&&
  field(L"SoundCurrent.NT.Services",L"AddService",1,L"SoundCurrentVAD")&&
  field(L"SoundCurrent.NT.Services",L"AddService",3,L"DriverService");
 SetupCloseInfFile(inf);
 require(ours,"Driver package identity does not match SoundCurrent Audio");
}
void verify(const fs::path& directory){
 for(auto file:{L"soundcurrentvad.inf",L"soundcurrentvad.sys",L"soundcurrentvad.cat"})require(fs::is_regular_file(directory/file),"Signed driver package is incomplete");
 verifyPackageIdentity(directory);
 auto catalog=(directory/L"soundcurrentvad.cat").wstring();
 WINTRUST_FILE_INFO file{};file.cbStruct=sizeof(file);file.pcwszFilePath=catalog.c_str();
 WINTRUST_DATA data{};data.cbStruct=sizeof(data);data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_WHOLECHAIN;data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;
 GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;LONG status=WinVerifyTrust(nullptr,&action,&data);data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&action,&data);
 require(status==ERROR_SUCCESS,"Driver catalog is unsigned or Windows cannot verify its trust");
 SP_INF_SIGNER_INFO_W signer{};signer.cbSize=sizeof(signer);
 require(SetupVerifyInfFileW((directory/L"soundcurrentvad.inf").c_str(),nullptr,&signer),"INF catalog binding/signature validation failed");
}
// One import intent suffices because all setup actions hold the global mutex.
// Its files are retained until the imported OEM identity has been durably saved.
struct PendingImport {
 HKEY key=nullptr;
 PendingImport(){require(RegCreateKeyExW(HKEY_LOCAL_MACHINE,L"Software\\SoundCurrent\\AudioDriver",0,nullptr,0,KEY_READ|KEY_WRITE|KEY_WOW64_64KEY,nullptr,&key,nullptr)==ERROR_SUCCESS,"Cannot open import recovery record");}
 ~PendingImport(){if(key)RegCloseKey(key);}
 struct Intent { fs::path path;bool cleanup=false; };
 Intent load(){
  wchar_t value[MAX_PATH+16]{};DWORD type=0,size=sizeof(value);
  const auto status=RegQueryValueExW(key,L"PendingImport",nullptr,&type,reinterpret_cast<BYTE*>(value),&size);
  if(status==ERROR_FILE_NOT_FOUND)return {};
  require(status==ERROR_SUCCESS&&type==REG_SZ&&size>=sizeof(wchar_t)&&size<=sizeof(value)&&size%sizeof(wchar_t)==0,"Invalid import recovery record");
  require(value[size/sizeof(wchar_t)-1]==0&&wcslen(value)+1==size/sizeof(wchar_t),"Invalid import recovery path");
  std::wstring record(value);bool cleanup=false;
  if(record.rfind(L"cleanup|",0)==0){cleanup=true;record.erase(0,8);}
  else if(record.rfind(L"import|",0)==0)record.erase(0,7);
  // An unprefixed path is a legacy import intent from the earlier prototype.
  fs::path path(record);const auto base=stagingBase();
  require(path.parent_path()==base,"Import recovery path outside protected staging");
  GUID id{};require(SUCCEEDED(CLSIDFromString(path.filename().c_str(),&id)),"Invalid import staging ID");
  // Refuse reparse redirects before inspecting or deleting privileged files.
  for(auto check=path;!check.empty();check=check.parent_path()) {
   const DWORD attributes=GetFileAttributesW(check.c_str());
   if(attributes==INVALID_FILE_ATTRIBUTES){
    const DWORD error=GetLastError();
    require(cleanup&&(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND),"Import staging missing or inaccessible");
   }else require(!(attributes&FILE_ATTRIBUTE_REPARSE_POINT),"Import staging redirected");
   if(check==check.root_path())break;
  }
  return {path,cleanup};
 }
 void set(const fs::path& path,bool cleanup=false){
  require(path.wstring().size()<MAX_PATH,"Import staging path too long");
  auto value=std::wstring(cleanup?L"cleanup|":L"import|")+path.wstring();
  require(RegSetValueExW(key,L"PendingImport",0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS,"Cannot record pending driver import");
  require(RegFlushKey(key)==ERROR_SUCCESS,"Cannot persist pending driver import");
 }
 void clear(){auto status=RegDeleteValueW(key,L"PendingImport");require(status==ERROR_SUCCESS||status==ERROR_FILE_NOT_FOUND,"Cannot retire pending import");require(RegFlushKey(key)==ERROR_SUCCESS,"Cannot persist import completion");}
 void finishCleanup(){
  auto intent=load();require(!intent.path.empty()&&intent.cleanup,"Missing durable staging cleanup intent");
  // Delete only our three package files. Unknown files are preserved and make
  // directory removal fail, retaining the intent for diagnosis/retry.
  for(auto name:{L"soundcurrentvad.inf",L"soundcurrentvad.sys",L"soundcurrentvad.cat"})
   fs::remove(intent.path/name);
  fs::remove(intent.path);
  clear();
 }
 void recover(Packages& packages){
  auto intent=load();if(intent.path.empty())return;
  if(!intent.cleanup){
   verify(intent.path); // Enforce signature/trust equally during recovery.
   packages.add(importPackage(intent.path));
   set(intent.path,true); // Persist cleanup before deleting any staging file.
  }
  finishCleanup();
 }
};
fs::path stage(const fs::path& from){
 fs::path base=stagingBase();
 GUID guid{};require(SUCCEEDED(CoCreateGuid(&guid)),"Cannot create package staging ID");wchar_t id[40]{};StringFromGUID2(guid,id,40);
 auto destination=base/id;fs::create_directories(destination);
 try{for(auto f:{L"soundcurrentvad.inf",L"soundcurrentvad.sys",L"soundcurrentvad.cat"})fs::copy_file(from/f,destination/f);verify(destination);}
 catch(...){fs::remove_all(destination);throw;}
 return destination;
}
int run(int argc,wchar_t** argv){
 require(argc>=2,"Choose --status, --verify, --install or --remove");
 std::wstring command=argv[1];
 if(command==L"--status"){require(argc==2,"Unexpected status arguments");Devices d;return d.own().empty()?10:0;}
 if(command==L"--verify"){require(argc==3,"Supply a driver package directory");verify(fs::absolute(argv[2]));return 0;}
 require(command==L"--install"||command==L"--remove","Unknown driver action");
 const int baseArgs=command==L"--install"?4:3;
 require(argc==baseArgs||argc==baseArgs+1,"Invalid driver setup arguments");
 std::wstring app=argv[2];require(app==L"eq"||app==L"studio","Unknown SoundCurrent app");
 const auto sid=argc==baseArgs+1?validatedSid(argv[baseArgs]):userSid();
 DriverTransaction transaction;
 soundcurrent::WindowsAudioSessionGate sessionGate;
 require(sessionGate.acquire(),"Quit SoundCurrent apps or wait for audio setup before changing the driver");
 checkNoApps();Owners owners;Packages packages;PendingImport pending;auto owner=sid+L"."+app;Devices devices;auto existing=devices.own();
 if(command==L"--remove"){
  if(!owners.has(owner))return 0;
  pending.recover(packages);
  if(owners.count()>1){owners.remove(owner);return 0;}
  // Remove only our Root hardware ID; never touch a physical driver.
  bool reboot=false;
  for(auto& d:existing){SP_REMOVEDEVICE_PARAMS remove{};remove.ClassInstallHeader.cbSize=sizeof(SP_CLASSINSTALL_HEADER);remove.ClassInstallHeader.InstallFunction=DIF_REMOVE;remove.Scope=DI_REMOVEDEVICE_GLOBAL;
   require(SetupDiSetClassInstallParamsW(devices.handle,&d,&remove.ClassInstallHeader,sizeof(remove))&&SetupDiCallClassInstaller(DIF_REMOVE,devices.handle,&d),"Virtual device removal failed; ownership was retained");
   SP_DEVINSTALL_PARAMS_W params{};params.cbSize=sizeof(params);
   require(SetupDiGetDeviceInstallParamsW(devices.handle,&d,&params),"Cannot inspect removal restart requirement; retry removal");
   reboot=reboot||(params.Flags&(DI_NEEDREBOOT|DI_NEEDRESTART))!=0;
  }
  packages.cleanup();owners.remove(owner);return reboot?3010:0;
 }
 pending.recover(packages);
 auto package=stage(fs::absolute(argv[3]));bool intentRecorded=false;bool created=false;SP_DEVINFO_DATA device{};device.cbSize=sizeof(device);
 try{
  // Record and flush ownership before any Driver Store/device mutation. If the
  // installer exits during import or binding, this app's next removal can
  // still clean up its own registered device and journaled packages. Failed
  // installation retains this recovery ownership intentionally.
  owners.add(owner);
  intentRecorded=true;pending.set(package);
  packages.add(importPackage(package));
  pending.set(package,true);
  if(existing.empty()){
   require(SetupDiCreateDeviceInfoW(devices.handle,L"SoundCurrentVAD",&GUID_DEVCLASS_MEDIA,L"SoundCurrent Audio",nullptr,DICD_GENERATE_ID,&device),"Cannot create virtual audio device");
   const wchar_t id[]=L"Root\\SOUNDCURRENTVAD\0";
   require(SetupDiSetDeviceRegistryPropertyW(devices.handle,&device,SPDRP_HARDWAREID,reinterpret_cast<const BYTE*>(id),sizeof(id))&&SetupDiCallClassInstaller(DIF_REGISTERDEVICE,devices.handle,&device),"Cannot register virtual audio device");created=true;
  }
  BOOL reboot=FALSE;
  // Windows enforces kernel signing here. No test mode or force-install flag.
  require(UpdateDriverForPlugAndPlayDevicesW(nullptr,hardware,(package/L"soundcurrentvad.inf").c_str(),0,&reboot),"Windows rejected the driver package or its kernel signature");
  pending.finishCleanup();intentRecorded=false;return reboot?3010:0;
 }catch(...){if(created)SetupDiCallClassInstaller(DIF_REMOVE,devices.handle,&device);if(!intentRecorded)fs::remove_all(package);throw;}
}
int main(){int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);try{require(args!=nullptr,"Invalid command line");int result=run(count,args);LocalFree(args);return result;}catch(const std::exception& e){if(args)LocalFree(args);std::cerr<<e.what()<<'\n';return 30;}}
