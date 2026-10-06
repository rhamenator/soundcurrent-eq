// SPDX-License-Identifier: GPL-3.0-only
// Read-only endpoint/default-role/OEM-effect inventory and DLL trust preflight.
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propvarutil.h>
#include <wintrust.h>
#include <softpub.h>
#include <wrl/client.h>
#include <filesystem>
#include <iostream>
#include <string>
using Microsoft::WRL::ComPtr;
int wmain(int argc,wchar_t **argv) {
    if(argc==3&&std::wstring(argv[1])==L"--check-dll"){
        const auto path=std::filesystem::absolute(argv[2]);
        WINTRUST_FILE_INFO file{sizeof(file)};file.pcwszFilePath=path.c_str();
        WINTRUST_DATA data{sizeof(data)};data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_NONE;
        data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;
        data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;GUID policy=WINTRUST_ACTION_GENERIC_VERIFY_V2;
        const auto result=WinVerifyTrust(nullptr,&policy,&data);data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&policy,&data);
        std::wcout<<L"DLL Authenticode trust: "<<(result==ERROR_SUCCESS?L"trusted":L"untrusted/unsigned")<<L" (0x"<<std::hex<<result<<L")\n";
        std::wcout<<L"No registration, endpoint changes or certificate installation performed. Authenticode trust alone does not prove protected-audio eligibility.\n";
        return result==ERROR_SUCCESS?0:2;
    }
    if(argc!=1){std::wcerr<<L"Usage: soundcurrent-native-endpoint-audit [--check-dll path]\n";return 1;}
    const auto init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(init))return 1;
    int result=0;{
    ComPtr<IMMDeviceEnumerator> enumerator;
    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&enumerator))))result=1;
    else for(const auto flow:{eRender,eCapture}){
        std::wcout<<(flow==eRender?L"Playback":L"Capture")<<L" endpoints (read-only):\n";
        for(const auto role:{eConsole,eMultimedia,eCommunications}){ComPtr<IMMDevice> d;LPWSTR id=nullptr;
            if(SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow,role,&d))&&SUCCEEDED(d->GetId(&id))){std::wcout<<L" default role "<<role<<L": "<<id<<L"\n";CoTaskMemFree(id);}}
        ComPtr<IMMDeviceCollection> collection;if(FAILED(enumerator->EnumAudioEndpoints(flow,DEVICE_STATE_ACTIVE,&collection)))continue;
        UINT count=0;collection->GetCount(&count);for(UINT i=0;i<count;++i){ComPtr<IMMDevice> d;collection->Item(i,&d);if(!d)continue;
            LPWSTR raw=nullptr;if(FAILED(d->GetId(&raw)))continue;const std::wstring id(raw);CoTaskMemFree(raw);
            ComPtr<IPropertyStore> properties;d->OpenPropertyStore(STGM_READ,&properties);PROPVARIANT name{};PropVariantInit(&name);
            if(properties&&SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName,&name))&&name.vt==VT_LPWSTR)std::wcout<<L" "<<name.pwszVal<<L"\n";PropVariantClear(&name);
            std::wcout<<L"  "<<id<<L"\n";
            const auto brace=id.rfind(L'{');if(brace==std::wstring::npos)continue;
            GUID guid{};if(FAILED(CLSIDFromString(id.substr(brace).c_str(),&guid)))continue;wchar_t canonical[40]{};StringFromGUID2(guid,canonical,40);
            const auto key=std::wstring(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\")+(flow==eRender?L"Render\\":L"Capture\\")+canonical+L"\\FxProperties";
            HKEY handle=nullptr;const auto opened=RegOpenKeyExW(HKEY_LOCAL_MACHINE,key.c_str(),0,KEY_QUERY_VALUE,&handle);
            if(opened!=ERROR_SUCCESS){std::wcout<<L"  Effect-store read status: "<<opened<<L" (no compatibility conclusion)\n";continue;}
            for(DWORD index=0;index<128;++index){wchar_t value[256]{};DWORD length=256,type=0,bytes=0;
                const auto status=RegEnumValueW(handle,index,value,&length,nullptr,&type,nullptr,&bytes);if(status==ERROR_NO_MORE_ITEMS)break;if(status!=ERROR_SUCCESS)continue;
                std::wcout<<L"  Effect property: "<<value<<L"; registry type "<<type<<L"; "<<bytes<<L" bytes\n";}
            RegCloseKey(handle);
        }
    }}CoUninitialize();return result;
}
