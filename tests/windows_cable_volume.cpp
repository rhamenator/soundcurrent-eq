// SPDX-License-Identifier: GPL-3.0-only
// Inert Win32 API fixture: never reads/writes registry, elevates, or touches devices.
#include <windows.h>
#include <setupapi.h>
#include <shellapi.h>
#include <cassert>
#include <cstring>
#include <iostream>
namespace fixture {
int saved=12, pending=0, writes=0, launches=0;
bool admin=true, present=true, cancelled=false, badReadback=false;
LONG readError=0, writeError=0, flushError=0;
DWORD type=REG_DWORD, value=1;
HKEY config=reinterpret_cast<HKEY>(1), marker=reinterpret_cast<HKEY>(2);
void reset(){saved=12;pending=writes=launches=0;admin=present=true;cancelled=badReadback=false;readError=writeError=flushError=0;type=REG_DWORD;value=1;}
LONG open(HKEY,const wchar_t* path,DWORD,REGSAM access,HKEY* out){
 assert(access&KEY_WOW64_64KEY);
 if(wcsstr(path,L"VolumePending")){if(!pending)return ERROR_FILE_NOT_FOUND;*out=marker;return 0;}
 if(readError)return readError;
 if(saved==12)return ERROR_FILE_NOT_FOUND;
 *out=config;return 0;
}
LONG query(HKEY,const wchar_t*,DWORD*,DWORD* t,BYTE* data,DWORD* bytes){
 *t=type;*bytes=sizeof(DWORD);std::memcpy(data,&value,sizeof(value));return 0;
}
LONG close(HKEY){return 0;}
LONG create(HKEY,const wchar_t* path,DWORD,wchar_t*,DWORD options,REGSAM access,SECURITY_ATTRIBUTES*,HKEY* out,DWORD*){
 assert(access&KEY_WOW64_64KEY);
 if(wcsstr(path,L"VolumePending")){assert(options==REG_OPTION_VOLATILE);pending=1;*out=marker;}
 else {*out=config;}
 return 0;
}
LONG set(HKEY,const wchar_t* name,DWORD,DWORD t,const BYTE* data,DWORD bytes){
 assert(wcscmp(name,L"VBAudioCableWDM_VolCtl")==0&&t==REG_DWORD&&bytes==sizeof(DWORD));
 ++writes;if(writeError)return writeError;
 saved=0;type=REG_DWORD;std::memcpy(&value,data,bytes);if(badReadback)value=0;return 0;
}
LONG flush(HKEY){return flushError;}
HDEVINFO devices(const GUID*,const wchar_t*,HWND,DWORD){return reinterpret_cast<HDEVINFO>(3);}
BOOL enumerate(HDEVINFO,DWORD i,SP_DEVINFO_DATA*){return present&&i==0;}
BOOL property(HDEVINFO,SP_DEVINFO_DATA*,DWORD,DWORD* t,BYTE* data,DWORD bytes,DWORD* needed){
 constexpr wchar_t id[]=L"ROOT\\VBAudioVACWDM\0";if(needed)*needed=sizeof(id);*t=REG_MULTI_SZ;
 if(!data||bytes<sizeof(id))return FALSE;std::memcpy(data,id,sizeof(id));return TRUE;
}
BOOL destroy(HDEVINFO){return TRUE;}
BOOL token(HANDLE,DWORD,HANDLE* out){*out=reinterpret_cast<HANDLE>(4);return TRUE;}
BOOL info(HANDLE,TOKEN_INFORMATION_CLASS,void* data,DWORD,DWORD*){static_cast<TOKEN_ELEVATION*>(data)->TokenIsElevated=admin;return TRUE;}
BOOL handle(HANDLE){return TRUE;}
DWORD module(HMODULE,wchar_t* path,DWORD){wcscpy(path,L"C:\\fixture\\guard.exe");return DWORD(wcslen(path));}
BOOL launch(SHELLEXECUTEINFOW* e){++launches;assert(wcscmp(e->lpParameters,L"--configure-volume-elevated")==0);if(cancelled)return FALSE;e->hProcess=reinterpret_cast<HANDLE>(5);return TRUE;}
DWORD error(){return ERROR_CANCELLED;}
DWORD wait(HANDLE,DWORD){return WAIT_OBJECT_0;}
BOOL exitCode(HANDLE,DWORD* code){*code=3010;return TRUE;}
}
#define RegOpenKeyExW fixture::open
#define RegQueryValueExW fixture::query
#define RegCloseKey fixture::close
#define RegCreateKeyExW fixture::create
#define RegSetValueExW fixture::set
#define RegFlushKey fixture::flush
#define SetupDiGetClassDevsW fixture::devices
#define SetupDiEnumDeviceInfo fixture::enumerate
#define SetupDiGetDeviceRegistryPropertyW fixture::property
#define SetupDiDestroyDeviceInfoList fixture::destroy
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
 reset();assert(configureCableVolume(false)==3010&&writes==1&&value==1); // missing
 assert(configureCableVolume(true)==3010&&writes==1&&launches==0); // pending idempotence
 pending=0;assert(configureCableVolume(true)==0&&writes==1&&launches==0); // reboot boundary
 reset();saved=0;value=0;assert(configureCableVolume(false)==3010&&writes==1);
 reset();saved=0;type=REG_SZ;assert(configureCableVolume(false)==3010&&writes==1);
 reset();saved=0;admin=false;assert(configureCableVolume(true)==0&&launches==0&&writes==0);
 reset();readError=ERROR_ACCESS_DENIED;assert(configureCableVolume(true)==31&&writes==0&&launches==0);
 reset();admin=false;assert(configureCableVolume(false)==740&&launches==0&&writes==0);
 cancelled=true;assert(configureCableVolume(true)==1223&&launches==1&&writes==0);
 cancelled=false;assert(configureCableVolume(true)==3010&&launches==2);
 reset();present=false;assert(configureCableVolume(true)==10&&writes==0&&launches==0);
 reset();writeError=ERROR_ACCESS_DENIED;assert(configureCableVolume(false)==31);
 reset();badReadback=true;assert(configureCableVolume(false)==31);
 reset();flushError=ERROR_ACCESS_DENIED;assert(configureCableVolume(false)==31);
 std::cout<<"PASS: fixed 64-bit configuration, absent/zero/one/type, access/readback/flush faults, cancel, no silent UAC, pending reboot, idempotence\n";
}
