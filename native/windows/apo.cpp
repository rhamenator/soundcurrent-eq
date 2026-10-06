// SPDX-License-Identifier: GPL-3.0-only
#include "apo_api.h"
#include <mmreg.h>
#include <audiomediatype.h>
#include <audioenginebaseapo.h>
#include "processor.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <new>
namespace {
std::atomic<unsigned> objects{0}, serverLocks{0};
bool format(IAudioMediaType *media, UNCOMPRESSEDAUDIOFORMAT &f) {
    if (!media || FAILED(media->GetUncompressedAudioFormat(&f))) return false;
    return f.guidFormatType == NativeFloatSubtype && f.dwSamplesPerFrame == 2 &&
        f.dwBytesPerSampleContainer == 4 && f.dwValidBitsPerSample == 32 &&
        std::isfinite(f.fFramesPerSecond) && f.fFramesPerSecond >= 8000 && f.fFramesPerSecond <= 384000 &&
        std::floor(f.fFramesPerSecond) == f.fFramesPerSecond && (f.dwChannelMask == 0 || f.dwChannelMask == 3);
}
HRESULT negotiate(IAudioMediaType *opposite, IAudioMediaType *requested, IAudioMediaType **supported) {
    if (!supported) return E_POINTER; *supported = nullptr;
    UNCOMPRESSEDAUDIOFORMAT f{},other{};
    if (!format(requested,f) || (opposite && (!format(opposite,other) ||
        f.fFramesPerSecond != other.fFramesPerSecond || f.dwChannelMask != other.dwChannelMask))) return APOERR_FORMAT_NOT_SUPPORTED;
    requested->AddRef(); *supported = requested; return S_OK;
}
class Apo final : public IAudioProcessingObject, public IAudioProcessingObjectConfiguration,
                  public IAudioProcessingObjectRT, public IAudioSystemEffects,
                  public ISoundCurrentNativeControl {
    std::atomic<ULONG> refs_{1}; bool initialized_ = false, locked_ = false;
    UINT32 maxFrames_ = 0; soundcurrent::native::Processor processor_;
public:
    Apo(){++objects;} ~Apo(){--objects;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(iid==__uuidof(IUnknown)||iid==__uuidof(IAudioProcessingObject))*out=static_cast<IAudioProcessingObject *>(this);
        else if(iid==__uuidof(IAudioProcessingObjectConfiguration))*out=static_cast<IAudioProcessingObjectConfiguration *>(this);
        else if(iid==__uuidof(IAudioProcessingObjectRT))*out=static_cast<IAudioProcessingObjectRT *>(this);
        else if(iid==__uuidof(IAudioSystemEffects))*out=static_cast<IAudioSystemEffects *>(this);
        else if(iid==__uuidof(ISoundCurrentNativeControl))*out=static_cast<ISoundCurrentNativeControl *>(this);
        else return E_NOINTERFACE;
        AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override{const auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Initialize(UINT32 size,BYTE *data) override {
        if(initialized_)return APOERR_ALREADY_INITIALIZED;
        if(size){if(!data)return E_POINTER;if(size<sizeof(APOInitBaseStruct))return E_INVALIDARG;
            APOInitBaseStruct base{};std::memcpy(&base,data,sizeof(base));
            if(base.cbSize!=size)return E_INVALIDARG;if(base.clsid!=CLSID_SoundCurrentNative)return APOERR_INVALID_APO_CLSID;
        }else if(data)return E_INVALIDARG;
        initialized_=true;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Reset() override {if(locked_)return APOERR_APO_LOCKED;processor_.reset();return S_OK;}
    HRESULT STDMETHODCALLTYPE GetLatency(HNSTIME *time) override{if(!time)return E_POINTER;*time=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetInputChannelCount(UINT32 *channels) override{if(!channels)return E_POINTER;*channels=2;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetRegistrationProperties(APO_REG_PROPERTIES **out) override {
        if(!out)return E_POINTER;*out=nullptr;
        constexpr UINT32 count=4;const auto bytes=sizeof(APO_REG_PROPERTIES)+(count-1)*sizeof(IID);
        auto *p=static_cast<APO_REG_PROPERTIES *>(CoTaskMemAlloc(bytes));if(!p)return E_OUTOFMEMORY;std::memset(p,0,bytes);
        p->clsid=CLSID_SoundCurrentNative;p->Flags=static_cast<APO_FLAG>(APO_FLAG_DEFAULT|APO_FLAG_INPLACE);
        wcscpy_s(p->szFriendlyName,L"SoundCurrent native stereo processor (experimental)");wcscpy_s(p->szCopyrightInfo,L"SoundCurrent contributors; GPL-3.0-only");
        p->u32MajorVersion=1;p->u32MinorVersion=0;p->u32MinInputConnections=p->u32MaxInputConnections=1;
        p->u32MinOutputConnections=p->u32MaxOutputConnections=1;p->u32MaxInstances=UINT32_MAX;p->u32NumAPOInterfaces=count;
        p->iidAPOInterfaceList[0]=__uuidof(IAudioProcessingObject);p->iidAPOInterfaceList[1]=__uuidof(IAudioProcessingObjectConfiguration);
        p->iidAPOInterfaceList[2]=__uuidof(IAudioProcessingObjectRT);p->iidAPOInterfaceList[3]=__uuidof(IAudioSystemEffects);*out=p;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE IsInputFormatSupported(IAudioMediaType *op,IAudioMediaType *req,IAudioMediaType **out) override{return negotiate(op,req,out);}
    HRESULT STDMETHODCALLTYPE IsOutputFormatSupported(IAudioMediaType *op,IAudioMediaType *req,IAudioMediaType **out) override{return negotiate(op,req,out);}
    HRESULT STDMETHODCALLTYPE LockForProcess(UINT32 ni,APO_CONNECTION_DESCRIPTOR **in,UINT32 no,APO_CONNECTION_DESCRIPTOR **out) override {
        if(!initialized_)return APOERR_NOT_INITIALIZED;if(locked_)return APOERR_APO_LOCKED;
        if(ni!=1||no!=1)return APOERR_NUM_CONNECTIONS_INVALID;
        if(!in||!out||!in[0]||!out[0])return E_POINTER;
        if(in[0]->u32Signature!=APO_CONNECTION_DESCRIPTOR_SIGNATURE || out[0]->u32Signature!=APO_CONNECTION_DESCRIPTOR_SIGNATURE)return E_INVALIDARG;
        UNCOMPRESSEDAUDIOFORMAT a{},b{};
        if(!format(in[0]->pFormat,a)||!format(out[0]->pFormat,b)||a.fFramesPerSecond!=b.fFramesPerSecond||a.dwChannelMask!=b.dwChannelMask)return APOERR_INVALID_CONNECTION_FORMAT;
        if(!in[0]->u32MaxFrameCount || out[0]->u32MaxFrameCount<in[0]->u32MaxFrameCount)return APOERR_INVALID_OUTPUT_MAXFRAMECOUNT;
        try{if(!processor_.prepare(static_cast<int>(a.fFramesPerSecond),2,in[0]->u32MaxFrameCount))return E_INVALIDARG;}
        catch(const std::bad_alloc &){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
        maxFrames_=in[0]->u32MaxFrameCount;locked_=true;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UnlockForProcess() override{if(!locked_)return APOERR_ALREADY_UNLOCKED;locked_=false;return S_OK;}
    UINT32 STDMETHODCALLTYPE CalcInputFrames(UINT32 frames) override{return frames;}
    UINT32 STDMETHODCALLTYPE CalcOutputFrames(UINT32 frames) override{return frames;}
    void STDMETHODCALLTYPE APOProcess(UINT32 ni,APO_CONNECTION_PROPERTY **in,UINT32 no,APO_CONNECTION_PROPERTY **out) override {
        if(no!=1||!out||!out[0])return;
        auto *destination=out[0];
        const APO_CONNECTION_PROPERTY saved=(ni==1&&in&&in[0])?*in[0]:APO_CONNECTION_PROPERTY{};
        destination->u32ValidFrameCount=0;destination->u32BufferFlags=BUFFER_INVALID;
        if(!locked_||ni!=1||!in||!in[0] || saved.u32Signature!=APO_CONNECTION_PROPERTY_SIGNATURE || destination->u32Signature!=APO_CONNECTION_PROPERTY_SIGNATURE)return;
        const auto *source=&saved;if(source->u32ValidFrameCount>maxFrames_ || (source->u32BufferFlags!=BUFFER_VALID && source->u32BufferFlags!=BUFFER_SILENT))return;
        const bool silent=source->u32BufferFlags==BUFFER_SILENT;
        if(!processor_.process(reinterpret_cast<const float *>(source->pBuffer),reinterpret_cast<float *>(destination->pBuffer),source->u32ValidFrameCount,silent))return;
        destination->u32ValidFrameCount=source->u32ValidFrameCount;
        destination->u32BufferFlags=silent&&!processor_.enabled()?BUFFER_SILENT:BUFFER_VALID;
    }
    HRESULT STDMETHODCALLTYPE SetProfile(const NativeProfile *p) override {
        if(!p)return E_POINTER;if(!locked_)return APOERR_NOT_INITIALIZED;
        if(p->bytes!=sizeof(NativeProfile)||p->version!=1||p->count>64||p->enabled>1||p->automaticHeadroom>1)return E_INVALIDARG;
        std::array<soundcurrent::EqBand,64> bands{};
        for(UINT32 i=0;i<p->count;++i){if(p->bands[i].type>3)return E_INVALIDARG;bands[i]={p->bands[i].frequency,p->bands[i].gainDb,p->bands[i].q,static_cast<soundcurrent::FilterType>(p->bands[i].type)};}
        soundcurrent::EnhancementSettings effects;std::copy(std::begin(p->effects),std::end(p->effects),effects.values.begin());
        return processor_.submit({bands.data(),p->count},p->gain,p->balance,p->enabled!=0,p->automaticHeadroom!=0,effects)?S_OK:E_INVALIDARG;
    }
};
class Factory final: public IClassFactory {
    std::atomic<ULONG> refs_{1};
public:
    Factory(){++objects;}~Factory(){--objects;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **out) override{if(!out)return E_POINTER;*out=nullptr;if(iid!=__uuidof(IUnknown)&&iid!=__uuidof(IClassFactory))return E_NOINTERFACE;*out=static_cast<IClassFactory *>(this);AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override{const auto n=--refs_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown *outer,REFIID iid,void **out) override{if(!out)return E_POINTER;*out=nullptr;if(outer)return CLASS_E_NOAGGREGATION;auto *p=new(std::nothrow) Apo;if(!p)return E_OUTOFMEMORY;const auto hr=p->QueryInterface(iid,out);p->Release();return hr;}
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override{if(lock)++serverLocks;else if(serverLocks) --serverLocks;return S_OK;}
};
}
extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid,REFIID iid,void **out){if(!out)return E_POINTER;*out=nullptr;if(clsid!=CLSID_SoundCurrentNative)return CLASS_E_CLASSNOTAVAILABLE;auto *p=new(std::nothrow) Factory;if(!p)return E_OUTOFMEMORY;const auto hr=p->QueryInterface(iid,out);p->Release();return hr;}
extern "C" HRESULT __stdcall DllCanUnloadNow(){return objects==0&&serverLocks==0?S_OK:S_FALSE;}
