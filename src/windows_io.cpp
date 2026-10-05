// SPDX-License-Identifier: GPL-3.0-only
#ifdef _WIN32
#define NOMINMAX
#include "windows_audio.h"
#include <windows.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace soundcurrent {
namespace {
using Microsoft::WRL::ComPtr;
class Apartment {
    bool owned_ = false;
public:
    Apartment() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) throw std::runtime_error("Windows audio COM unavailable");
        owned_ = SUCCEEDED(hr);
    }
    ~Apartment() { if (owned_) CoUninitialize(); }
};
void checked(HRESULT hr, const char *what) {
    if (SUCCEEDED(hr)) return;
    std::ostringstream out; out << what << " failed (0x" << std::hex << static_cast<unsigned long>(hr) << ')';
    throw std::runtime_error(out.str());
}
ComPtr<IMMDevice> endpoint(const std::wstring &id) {
    ComPtr<IMMDeviceEnumerator> e;
    checked(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              IID_PPV_ARGS(e.GetAddressOf())), "Enumerate endpoints");
    ComPtr<IMMDevice> result;
    checked(e->GetDevice(id.c_str(), result.GetAddressOf()), "Open endpoint");
    return result;
}
ComPtr<IAudioEndpointVolume> volume(const std::wstring &id) {
    ComPtr<IAudioEndpointVolume> result;
    checked(endpoint(id)->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void **>(result.GetAddressOf())), "Open endpoint volume");
    return result;
}
void copyVolume(const std::wstring &from, const std::wstring &to) {
    auto a = volume(from), b = volume(to);
    float level = 1; BOOL muted = FALSE;
    checked(a->GetMasterVolumeLevelScalar(&level), "Read output level");
    checked(a->GetMute(&muted), "Read output mute");
    checked(b->SetMasterVolumeLevelScalar(level, nullptr), "Set output level");
    checked(b->SetMute(muted, nullptr), "Set output mute");
}
// Windows exposes default endpoint reads publicly but still uses this COM ABI
// for writes. Only SetDefaultEndpoint is called. Failure leaves an actionable
// error; acquisition rolls back each role already changed. ABI reference:
// https://github.com/tartakynov/audioswitch/blob/master/IPolicyConfig.h
struct PolicyConfig : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX **) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX *, WAVEFORMATEX *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, INT64 *, INT64 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, INT64 *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void *) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY &, PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY &, PROPVARIANT *) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};
void setDefault(const std::wstring &id, int role) {
    constexpr GUID clsid{0x870af99c,0x171d,0x4f9e,{0xaf,0x0d,0xe6,0x3d,0xf4,0x0c,0x2b,0xc9}};
    constexpr GUID iid{0xf8679f50,0x850a,0x41cf,{0x9c,0x72,0x43,0x0f,0x29,0x02,0x90,0xc8}};
    ComPtr<PolicyConfig> policy;
    checked(CoCreateInstance(clsid, nullptr, CLSCTX_ALL, iid,
                              reinterpret_cast<void **>(policy.GetAddressOf())), "Automatic audio routing unavailable");
    checked(policy->SetDefaultEndpoint(id.c_str(), static_cast<ERole>(role)), "Change default audio endpoint");
}
WAVEFORMATEX floatFormat(int rate, int channels) {
    WAVEFORMATEX f{}; f.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    f.nChannels = static_cast<WORD>(channels); f.nSamplesPerSec = rate;
    f.wBitsPerSample = 32; f.nBlockAlign = static_cast<WORD>(channels * 4);
    f.nAvgBytesPerSec = rate * f.nBlockAlign; return f;
}
ComPtr<IAudioClient> stream(const std::wstring &id) {
    ComPtr<IAudioClient> result;
    checked(endpoint(id)->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void **>(result.GetAddressOf())), "Open audio stream");
    return result;
}
} // namespace

WindowsRouteLease::WindowsRouteLease(bool capture, const std::wstring &cableId,
                                     const std::wstring &fallbackId, bool copyPlaybackVolume)
    : capture_(capture), volume_(copyPlaybackVolume), cable_(cableId), fallback_(fallbackId) {
    Apartment apartment;
    for (int i = 0; i < 3; ++i) {
        try { originals_[i] = windowsDefaultEndpointId(capture_, i); }
        catch (...) { originals_[i] = fallback_; }
        if (originals_[i] == cable_) originals_[i] = fallback_;
    }
    if (volume_) copyVolume(fallback_, cable_);
    int changed = 0;
    try { for (; changed < 3; ++changed) setDefault(cable_, changed); }
    catch (...) {
        for (int i = 0; i < changed; ++i) try { setDefault(originals_[i], i); } catch (...) {}
        throw;
    }
}
WindowsRouteLease::~WindowsRouteLease() {
    try {
        Apartment apartment;
        if (volume_ && windowsDefaultEndpointId(false) == cable_) {
            // Transfer volume adjustments made on the system's EQ endpoint back
            // to the selected speakers when quitting or bypassing.
            try { copyVolume(cable_, fallback_); } catch (...) {}
        }
        for (int i = 0; i < 3; ++i) {
            try {
                if (windowsDefaultEndpointId(capture_, i) != cable_) continue;
                try { setDefault(originals_[i], i); }
                catch (...) { setDefault(fallback_, i); }
            } catch (...) {}
        }
    } catch (...) {}
}

struct WindowsRecorder::Impl {
    std::atomic<bool> stop{false}, running{false};
    std::thread worker;
    mutable std::mutex mutex;
    std::condition_variable initialized;
    std::string failure;
    std::vector<std::int16_t> pcm;
};
WindowsRecorder::WindowsRecorder() : impl_(std::make_unique<Impl>()) {}
WindowsRecorder::~WindowsRecorder() { stop(); }
void WindowsRecorder::start(const std::wstring &id, int rate, int channels) {
    stop();
    if (rate < 8000 || rate > 192000 || channels < 1 || channels > 2)
        throw std::runtime_error("Unsupported recording format");
    impl_->stop = false;
    { std::lock_guard lock(impl_->mutex); impl_->failure.clear(); impl_->pcm.clear(); }
    impl_->worker = std::thread([this, id, rate, channels] {
        try {
            Apartment apartment; auto audio = stream(id); auto format = floatFormat(rate, channels);
            checked(audio->Initialize(AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                1000000, 0, &format, nullptr), "Initialize microphone recording");
            ComPtr<IAudioCaptureClient> reader;
            checked(audio->GetService(IID_PPV_ARGS(reader.GetAddressOf())), "Open microphone reader");
            checked(audio->Start(), "Start microphone recording");
            impl_->running = true; impl_->initialized.notify_all();
            while (!impl_->stop) {
                UINT32 frames = 0;
                checked(reader->GetNextPacketSize(&frames), "Read microphone packet size");
                while (frames) {
                    BYTE *data = nullptr; DWORD flags = 0;
                    checked(reader->GetBuffer(&data, &frames, &flags, nullptr, nullptr), "Read microphone samples");
                    {
                        std::lock_guard lock(impl_->mutex);
                        const std::size_t size = static_cast<std::size_t>(frames) * channels;
                        if (impl_->pcm.size() + size > static_cast<std::size_t>(rate) * channels * 30)
                            throw std::runtime_error("Microphone recording consumer stalled");
                        const auto *samples = reinterpret_cast<float *>(data);
                        for (std::size_t i = 0; i < size; ++i) {
                            const float value = flags & AUDCLNT_BUFFERFLAGS_SILENT ? 0.0f : samples[i];
                            impl_->pcm.push_back(static_cast<std::int16_t>(std::lround(
                                std::clamp(std::isfinite(value) ? value : 0.0f, -1.0f, 1.0f) * 32767.0f)));
                        }
                    }
                    checked(reader->ReleaseBuffer(frames), "Release microphone packet");
                    checked(reader->GetNextPacketSize(&frames), "Read next microphone packet");
                }
                Sleep(2);
            }
            audio->Stop();
        } catch (const std::exception &e) {
            std::lock_guard lock(impl_->mutex); impl_->failure = e.what();
        }
        impl_->running = false; impl_->initialized.notify_all();
    });
    std::unique_lock lock(impl_->mutex);
    const bool started = impl_->initialized.wait_for(lock, std::chrono::seconds(5), [this] {
        return impl_->running || !impl_->failure.empty();
    }) && impl_->running;
    const auto failure = impl_->failure;
    lock.unlock();
    if (!started) { stop(); throw std::runtime_error(failure.empty() ? "Microphone start timed out" : failure); }
}
void WindowsRecorder::stop() {
    impl_->stop = true;
    if (impl_->worker.joinable()) impl_->worker.join();
    impl_->running = false;
}
bool WindowsRecorder::running() const { return impl_->running; }
std::string WindowsRecorder::error() const { std::lock_guard lock(impl_->mutex); return impl_->failure; }
std::vector<std::int16_t> WindowsRecorder::take() {
    std::lock_guard lock(impl_->mutex); std::vector<std::int16_t> result;
    result.swap(impl_->pcm); return result;
}

void windowsPlayPcm(const std::wstring &id, std::span<const std::int16_t> pcm, int sampleRate, int channels) {
    if (pcm.empty() || sampleRate < 8000 || sampleRate > 192000 ||
        channels < 1 || channels > 2 || pcm.size() % channels)
        throw std::runtime_error("Invalid calibration audio");
    Apartment apartment; auto audio = stream(id); auto format = floatFormat(sampleRate, channels);
    checked(audio->Initialize(AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        1000000, 0, &format, nullptr), "Initialize test playback");
    ComPtr<IAudioRenderClient> writer;
    checked(audio->GetService(IID_PPV_ARGS(writer.GetAddressOf())), "Open test playback writer");
    UINT32 capacity = 0; checked(audio->GetBufferSize(&capacity), "Size test playback buffer");
    std::size_t offset = 0;
    checked(audio->Start(), "Start test playback");
    const auto totalFrames = pcm.size() / channels;
    while (offset < totalFrames) {
        UINT32 padding = 0; checked(audio->GetCurrentPadding(&padding), "Read test playback padding");
        const auto frames = static_cast<UINT32>(std::min<std::size_t>(capacity - padding, totalFrames - offset));
        if (frames) {
            BYTE *raw = nullptr; checked(writer->GetBuffer(frames, &raw), "Write test playback");
            auto *samples = reinterpret_cast<float *>(raw);
            for (UINT32 i = 0; i < frames * channels; ++i)
                samples[i] = pcm[offset * channels + i] / 32768.0f;
            checked(writer->ReleaseBuffer(frames, 0), "Release test playback"); offset += frames;
        }
        Sleep(2);
    }
    UINT32 padding = 1;
    while (padding) {
        Sleep(2); checked(audio->GetCurrentPadding(&padding), "Drain test playback");
    }
    audio->Stop();
}
} // namespace soundcurrent
#endif
