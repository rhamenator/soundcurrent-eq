// SPDX-License-Identifier: GPL-3.0-only
#include "apo_api.h"
#include <mmreg.h>
#include <audiomediatype.h>
#include <audioenginebaseapo.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
class Media final:public IAudioMediaType {
    std::atomic<ULONG> refs_{1};WAVEFORMATEX wave_{};
public:
    UNCOMPRESSEDAUDIOFORMAT f{NativeFloatSubtype,2,4,32,48000,3};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **out)override{if(!out)return E_POINTER;*out=nullptr;if(iid!=__uuidof(IUnknown)&&iid!=__uuidof(IAudioMediaType))return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release()override{const auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE IsCompressedFormat(BOOL *out)override{if(!out)return E_POINTER;*out=FALSE;return S_OK;}
    HRESULT STDMETHODCALLTYPE IsEqual(IAudioMediaType *other,DWORD *flags)override{if(!other||!flags)return E_POINTER;*flags=0;UNCOMPRESSEDAUDIOFORMAT b{};if(FAILED(other->GetUncompressedAudioFormat(&b)))return E_INVALIDARG;return std::memcmp(&b,&f,sizeof(f))==0?S_OK:S_FALSE;}
    const WAVEFORMATEX *STDMETHODCALLTYPE GetAudioFormat()override{wave_={WAVE_FORMAT_IEEE_FLOAT,2,48000,384000,8,32,0};return &wave_;}
    HRESULT STDMETHODCALLTYPE GetUncompressedAudioFormat(UNCOMPRESSEDAUDIOFORMAT *out)override{if(!out)return E_POINTER;*out=f;return S_OK;}
};
int wmain(int argc,wchar_t **argv){try{
 require(argc==2,"DLL argument");const auto path=std::filesystem::absolute(argv[1]);
 HMODULE dll=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);require(dll!=nullptr,"DLL load");
 auto create=reinterpret_cast<HRESULT (__stdcall *)(REFCLSID,REFIID,void **)>(GetProcAddress(dll,"DllGetClassObject"));
 auto unload=reinterpret_cast<HRESULT (__stdcall *)()>(GetProcAddress(dll,"DllCanUnloadNow"));require(create&&unload,"COM exports");require(unload()==S_OK,"initial unload");
 IClassFactory *factory=nullptr;require(create(CLSID_SoundCurrentNative,__uuidof(IClassFactory),reinterpret_cast<void **>(&factory))==S_OK,"factory");
 IAudioProcessingObject *apo=nullptr;require(factory->CreateInstance(nullptr,__uuidof(IAudioProcessingObject),reinterpret_cast<void **>(&apo))==S_OK,"object");
 require(unload()==S_FALSE,"live objects prevent unload");
 IAudioProcessingObjectConfiguration *config=nullptr;IAudioProcessingObjectRT *rt=nullptr;ISoundCurrentNativeControl *control=nullptr;IAudioSystemEffects *effect=nullptr;
 require(SUCCEEDED(apo->QueryInterface(__uuidof(IAudioProcessingObjectConfiguration),reinterpret_cast<void **>(&config)))&&SUCCEEDED(apo->QueryInterface(__uuidof(IAudioProcessingObjectRT),reinterpret_cast<void **>(&rt)))&&SUCCEEDED(apo->QueryInterface(__uuidof(ISoundCurrentNativeControl),reinterpret_cast<void **>(&control)))&&SUCCEEDED(apo->QueryInterface(__uuidof(IAudioSystemEffects),reinterpret_cast<void **>(&effect))),"required interfaces");
 IUnknown *a=nullptr,*b=nullptr;apo->QueryInterface(__uuidof(IUnknown),reinterpret_cast<void **>(&a));rt->QueryInterface(__uuidof(IUnknown),reinterpret_cast<void **>(&b));require(a==b,"COM identity");a->Release();b->Release();
 APO_REG_PROPERTIES *registration=nullptr;require(apo->GetRegistrationProperties(&registration)==S_OK&&registration->clsid==CLSID_SoundCurrentNative&&registration->u32NumAPOInterfaces==4,"registration");CoTaskMemFree(registration);
 auto *media=new Media;IAudioMediaType *suggested=nullptr;
 require(apo->IsInputFormatSupported(nullptr,media,&suggested)==S_OK&&suggested==media,"stereo float32 accepted");suggested->Release();suggested=nullptr;
 media->f.dwSamplesPerFrame=1;require(apo->IsOutputFormatSupported(nullptr,media,&suggested)==APOERR_FORMAT_NOT_SUPPORTED&&!suggested,"mono rejected");media->f.dwSamplesPerFrame=2;
 media->f.dwValidBitsPerSample=16;require(apo->IsInputFormatSupported(nullptr,media,&suggested)==APOERR_FORMAT_NOT_SUPPORTED,"integer rejected");media->f.dwValidBitsPerSample=32;
 media->f.dwChannelMask=12;require(apo->IsInputFormatSupported(nullptr,media,&suggested)==APOERR_FORMAT_NOT_SUPPORTED,"non-front stereo rejected");media->f.dwChannelMask=3;
 APO_CONNECTION_DESCRIPTOR id{APO_CONNECTION_BUFFER_TYPE_EXTERNAL,0,64,media,APO_CONNECTION_DESCRIPTOR_SIGNATURE},od=id;APO_CONNECTION_DESCRIPTOR *ids[]={&id},*ods[]={&od};
 require(config->LockForProcess(1,ids,1,ods)==APOERR_NOT_INITIALIZED,"initialize required");
 APOInitBaseStruct init{sizeof(APOInitBaseStruct),CLSID_SoundCurrentNative};require(apo->Initialize(sizeof(init),reinterpret_cast<BYTE *>(&init))==S_OK&&apo->Initialize(0,nullptr)==APOERR_ALREADY_INITIALIZED,"initialize state");
 od.u32MaxFrameCount=32;require(config->LockForProcess(1,ids,1,ods)==APOERR_INVALID_OUTPUT_MAXFRAMECOUNT,"capacity mismatch");od.u32MaxFrameCount=64;
 require(config->LockForProcess(1,ids,1,ods)==S_OK&&config->LockForProcess(1,ids,1,ods)==APOERR_APO_LOCKED,"lock state");
 HNSTIME latency=-1;require(apo->GetLatency(&latency)==S_OK&&latency==0&&rt->CalcInputFrames(64)==64&&rt->CalcOutputFrames(64)==64,"timing");
 std::array<float,128> input{},output{};input.fill(1.25f);
 APO_CONNECTION_PROPERTY ip{reinterpret_cast<UINT_PTR>(input.data()),64,BUFFER_VALID,APO_CONNECTION_PROPERTY_SIGNATURE};
 APO_CONNECTION_PROPERTY op{reinterpret_cast<UINT_PTR>(output.data()),0,BUFFER_INVALID,APO_CONNECTION_PROPERTY_SIGNATURE};APO_CONNECTION_PROPERTY *ips[]={&ip},*ops[]={&op};
 rt->APOProcess(1,ips,1,ops);require(op.u32BufferFlags==BUFFER_VALID&&op.u32ValidFrameCount==64&&input==output,"exact initial bypass");
 rt->APOProcess(1,ips,1,ips);require(ip.u32ValidFrameCount==64&&ip.u32BufferFlags==BUFFER_VALID&&input[0]==1.25f,"aliased connection and audio bypass");
 NativeProfile profile;profile.enabled=1;profile.gain=6;profile.automaticHeadroom=0;
 require(control->SetProfile(&profile)==S_OK,"local control");input.fill(.1f);rt->APOProcess(1,ips,1,ops);require(std::abs(output[0]-.199526f)<.00001f,"native gain immediate");
 profile.count=65;require(control->SetProfile(&profile)==E_INVALIDARG,"profile size bounds");profile.count=0;profile.version=2;require(control->SetProfile(&profile)==E_INVALIDARG,"profile version bounds");profile.version=1;profile.enabled=0;require(control->SetProfile(&profile)==S_OK,"disable");
 ip.u32BufferFlags=BUFFER_SILENT;ip.pBuffer=0;rt->APOProcess(1,ips,1,ops);require(op.u32BufferFlags==BUFFER_SILENT&&op.u32ValidFrameCount==64&&output[0]==0,"silent bypass null source");
 ip.u32ValidFrameCount=65;rt->APOProcess(1,ips,1,ops);require(op.u32BufferFlags==BUFFER_INVALID&&op.u32ValidFrameCount==0,"oversized process rejected");
 require(config->UnlockForProcess()==S_OK&&config->UnlockForProcess()==APOERR_ALREADY_UNLOCKED,"unlock state");require(apo->Reset()==S_OK,"reset");
 effect->Release();control->Release();rt->Release();config->Release();apo->Release();media->Release();
 factory->LockServer(TRUE);factory->Release();require(unload()==S_FALSE,"server lock prevents unload");
 require(create(CLSID_SoundCurrentNative,__uuidof(IClassFactory),reinterpret_cast<void **>(&factory))==S_OK,"factory reacquire");factory->LockServer(FALSE);factory->Release();require(unload()==S_OK,"final unload");FreeLibrary(dll);
 std::cout<<"PASS: real DLL/COM lifecycle, APO format negotiation, lock/reset, exact/aliased bypass, native gain, profile bounds, silence, capacity and unload; no device association performed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
