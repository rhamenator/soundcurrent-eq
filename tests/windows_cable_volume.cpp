// SPDX-License-Identifier: GPL-3.0-only
// Inert Win32 API fixture: never reads/writes registry, elevates, or touches devices.
#include <windows.h>
#include <setupapi.h>
#include <shellapi.h>
#include <cassert>
#include <cstring>
#include <iostream>
namespace fixture {
int saved=12, bootSaved=12, pending=0, writes=0, launches=0, live=12, liveWrites=0, childExit=0;
bool admin=true, present=true, control=true, cancelled=false, badReadback=false, liveWriteFails=false;
LONG readError=0, writeError=0, bootWriteError=0, flushError=0;
DWORD type=REG_DWORD, value=1, bootValue=1;
HKEY config=reinterpret_cast<HKEY>(1), marker=reinterpret_cast<HKEY>(2), bootConfig=reinterpret_cast<HKEY>(7);
void reset(){saved=bootSaved=12;bootValue=1;bootWriteError=0;pending=writes=launches=liveWrites=childExit=0;live=12;admin=present=control=true;cancelled=badReadback=liveWriteFails=false;readError=writeError=flushError=0;type=REG_DWORD;value=1;}
LONG open(HKEY,const wchar_t* path,DWORD,REGSAM access,HKEY* out){
 assert(access&KEY_WOW64_64KEY);
 if(wcsstr(path,L"VolumePending")){if(!pending)return ERROR_FILE_NOT_FOUND;*out=marker;return 0;}
 if(readError)return readError;
 const bool boot=wcsstr(path,L"SYSTEM\\CurrentControlSet\\Services\\VB-Cable")!=nullptr;
 if((boot?bootSaved:saved)==12)return ERROR_FILE_NOT_FOUND;
 *out=boot?bootConfig:config;return 0;
}
LONG query(HKEY key,const wchar_t*,DWORD*,DWORD* t,BYTE* data,DWORD* bytes){
 *t=type;*bytes=sizeof(DWORD);const DWORD stored=key==bootConfig?bootValue:value;std::memcpy(data,&stored,sizeof(stored));return 0;
}
LONG close(HKEY){return 0;}
LONG create(HKEY,const wchar_t* path,DWORD,wchar_t*,DWORD options,REGSAM access,SECURITY_ATTRIBUTES*,HKEY* out,DWORD*){
 assert(access&KEY_WOW64_64KEY);
 if(wcsstr(path,L"VolumePending")){assert(options==REG_OPTION_VOLATILE);pending=1;*out=marker;}
 else {*out=wcsstr(path,L"SYSTEM\\CurrentControlSet\\Services\\VB-Cable")?bootConfig:config;}
 return 0;
}
LONG set(HKEY key,const wchar_t* name,DWORD,DWORD t,const BYTE* data,DWORD bytes){
 assert(wcscmp(name,L"VBAudioCableWDM_VolCtl")==0&&t==REG_DWORD&&bytes==sizeof(DWORD));
 ++writes;if(writeError)return writeError;if(key==bootConfig&&bootWriteError)return bootWriteError;
 type=REG_DWORD;if(key==bootConfig){bootSaved=0;std::memcpy(&bootValue,data,bytes);if(badReadback)bootValue=0;}
 else {saved=0;std::memcpy(&value,data,bytes);if(badReadback)value=0;}return 0;
}
LONG flush(HKEY){return flushError;}
HDEVINFO devices(const GUID*,const wchar_t*,HWND,DWORD){return reinterpret_cast<HDEVINFO>(3);}
BOOL enumerate(HDEVINFO,DWORD i,SP_DEVINFO_DATA*){return present&&i==0;}
BOOL enumerateInterface(HDEVINFO,SP_DEVINFO_DATA*,const GUID*,DWORD i,SP_DEVICE_INTERFACE_DATA*){return present&&control&&i==0;}
BOOL detail(HDEVINFO,SP_DEVICE_INTERFACE_DATA*,SP_DEVICE_INTERFACE_DETAIL_DATA_W* data,DWORD bytes,DWORD* needed,SP_DEVINFO_DATA*){
 constexpr wchar_t path[]=L"fixture-cable-control";const DWORD required=DWORD(sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)+sizeof(path));
 if(needed)*needed=required;if(!data||bytes<required)return FALSE;wcscpy(data->DevicePath,path);return TRUE;
}
BOOL property(HDEVINFO,SP_DEVINFO_DATA*,DWORD,DWORD* t,BYTE* data,DWORD bytes,DWORD* needed){
 constexpr wchar_t id[]=L"ROOT\\VBAudioVACWDM\0";if(needed)*needed=sizeof(id);*t=REG_MULTI_SZ;
 if(!data||bytes<sizeof(id))return FALSE;std::memcpy(data,id,sizeof(id));return TRUE;
}
BOOL destroy(HDEVINFO){return TRUE;}
HANDLE file(const wchar_t*,DWORD access,DWORD share,SECURITY_ATTRIBUTES*,DWORD creation,DWORD flags,HANDLE){
 assert(access==(GENERIC_READ|GENERIC_WRITE)&&share==FILE_SHARE_READ&&creation==OPEN_EXISTING&&flags==0x800);
 return control?reinterpret_cast<HANDLE>(6):INVALID_HANDLE_VALUE;
}
BOOL ioctl(HANDLE,DWORD code,void*,DWORD,void* output,DWORD outputBytes,DWORD* returned,OVERLAPPED*){
 if(!admin)return FALSE;
 if(code==0x222058){++liveWrites;if(liveWriteFails)return FALSE;live=0;if(output&&outputBytes>=sizeof(DWORD))*static_cast<DWORD*>(output)=0;if(returned)*returned=sizeof(DWORD);return TRUE;}
 assert(code==0x222024);if(!control)return FALSE;std::memset(output,0,outputBytes);if(live==0&&outputBytes>0x174)static_cast<BYTE*>(output)[0x174]=2;if(returned)*returned=outputBytes;return TRUE;
}
BOOL token(HANDLE,DWORD,HANDLE* out){*out=reinterpret_cast<HANDLE>(4);return TRUE;}
BOOL info(HANDLE,TOKEN_INFORMATION_CLASS,void* data,DWORD,DWORD*){static_cast<TOKEN_ELEVATION*>(data)->TokenIsElevated=admin;return TRUE;}
BOOL handle(HANDLE){return TRUE;}
DWORD module(HMODULE,wchar_t* path,DWORD){wcscpy(path,L"C:\\fixture\\guard.exe");return DWORD(wcslen(path));}
BOOL launch(SHELLEXECUTEINFOW* e){++launches;assert(wcscmp(e->lpParameters,L"--configure-volume-elevated")==0);if(cancelled)return FALSE;e->hProcess=reinterpret_cast<HANDLE>(5);return TRUE;}
DWORD error(){return ERROR_CANCELLED;}
DWORD wait(HANDLE,DWORD){return WAIT_OBJECT_0;}
BOOL exitCode(HANDLE,DWORD* code){*code=childExit;return TRUE;}
}
#define RegOpenKeyExW fixture::open
#define RegQueryValueExW fixture::query
#define RegCloseKey fixture::close
#define RegCreateKeyExW fixture::create
#define RegSetValueExW fixture::set
#define RegFlushKey fixture::flush
#define SetupDiGetClassDevsW fixture::devices
#define SetupDiEnumDeviceInfo fixture::enumerate
#define SetupDiEnumDeviceInterfaces fixture::enumerateInterface
#define SetupDiGetDeviceInterfaceDetailW fixture::detail
#define SetupDiGetDeviceRegistryPropertyW fixture::property
#define SetupDiDestroyDeviceInfoList fixture::destroy
#define CreateFileW fixture::file
#define DeviceIoControl fixture::ioctl
#define OpenProcessToken fixture::token
#define GetTokenInformation fixture::info
#define CloseHandle fixture::handle
#define GetModuleFileNameW fixture::module
#define ShellExecuteExW fixture::launch
#define GetLastError fixture::error
#define WaitForSingleObject fixture::wait
#define GetExitCodeProcess fixture::exitCode
#include "windows_cable_volume.h"
int main(){
 using namespace fixture;
 reset();saved=0;bootSaved=0;assert(cableVolumeStatus()==12); // Saved enabled, live disabled is not ready.
 live=0;assert(cableVolumeStatus()==0);
 live=12;pending=1;assert(cableVolumeStatus()==3010);
 pending=0;control=false;assert(cableVolumeStatus()==31);
 reset();assert(configureCableVolume(false)==0&&writes==2&&value==1&&live==0); // persist and apply
 assert(configureCableVolume(true)==0&&writes==2&&launches==0); // live idempotence
 reset();saved=0;bootSaved=0;value=0;assert(configureCableVolume(false)==0&&writes==2&&live==0);
 reset();saved=0;bootSaved=0;type=REG_SZ;assert(configureCableVolume(false)==0&&writes==2&&live==0);
 reset();saved=0;bootSaved=0;admin=false;assert(configureCableVolume(true)==0&&launches==1&&writes==0);
 reset();saved=0;live=0;assert(savedCableVolumeStatus()==12);assert(configureCableVolume(false)==0&&bootSaved==0&&bootValue==1&&writes==2); // legacy Software-only migration
 reset();saved=bootSaved=0;bootValue=0;assert(savedCableVolumeStatus()==12);assert(configureCableVolume(false)==0&&bootValue==1&&writes==2); // disabled SYSTEM fallback
 reset();bootWriteError=ERROR_ACCESS_DENIED;assert(configureCableVolume(false)==31&&liveWrites==0);
 reset();readError=ERROR_ACCESS_DENIED;assert(configureCableVolume(true)==31&&writes==0&&launches==0);
 reset();admin=false;assert(configureCableVolume(false)==740&&launches==0&&writes==0);
 cancelled=true;assert(configureCableVolume(true)==1223&&launches==1&&writes==0);
 cancelled=false;assert(configureCableVolume(true)==0&&launches==2);
 reset();present=false;assert(configureCableVolume(true)==10&&writes==0&&launches==0);
 reset();writeError=ERROR_ACCESS_DENIED;assert(configureCableVolume(false)==31);
 reset();badReadback=true;assert(configureCableVolume(false)==31);
 reset();flushError=ERROR_ACCESS_DENIED;assert(configureCableVolume(false)==31);
 reset();control=false;assert(configureCableVolume(false)==3010&&writes==2); // fresh driver needs reboot
 reset();saved=0;bootSaved=0;liveWriteFails=true;assert(configureCableVolume(false)==3010&&writes==0);
 std::cout<<"PASS: dual-location 64-bit persistence plus live VB-CABLE driver apply, faults, cancel, reboot fallback, idempotence\n";
}
