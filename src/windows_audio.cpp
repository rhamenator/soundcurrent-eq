// SPDX-License-Identifier: GPL-3.0-only
#ifdef _WIN32
#define NOMINMAX
#include "windows_audio.h"

#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <propvarutil.h>
#include <initguid.h>
#include <propkeydef.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace soundcurrent {
namespace {

class Apartment {
public:
    Apartment() {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (result == RPC_E_CHANGED_MODE) return;
        if (FAILED(result)) throw std::runtime_error("Could not initialize Windows audio COM");
        initialized_ = true;
    }
    ~Apartment() { if (initialized_) CoUninitialize(); }
private:
    bool initialized_ = false;
};

void check(HRESULT result, const char *action) {
    if (SUCCEEDED(result)) return;
    std::ostringstream message;
    message << action << " failed (0x" << std::hex << std::uppercase
            << static_cast<unsigned long>(result) << ")";
    throw std::runtime_error(message.str());
}

std::wstring widen(const std::string &value) {
    return std::wstring(value.begin(), value.end());
}

WAVEFORMATEX stereoFloat48k() {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    format.nChannels = 2;
    format.nSamplesPerSec = 48000;
    format.wBitsPerSample = 32;
    format.nBlockAlign = 8;
    format.nAvgBytesPerSec = 48000 * 8;
    return format;
}

enum class SampleType { Float32, Pcm16, Pcm24, Pcm32 };

SampleType outputSampleType(const WAVEFORMATEX *format) {
    WORD tag = format->wFormatTag;
    if (tag == WAVE_FORMAT_EXTENSIBLE) {
        if (format->cbSize < 22) throw std::runtime_error("Invalid speaker mix format");
        const auto *extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(format);
        if (extended->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) tag = WAVE_FORMAT_IEEE_FLOAT;
        else if (extended->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) tag = WAVE_FORMAT_PCM;
    }
    if (tag == WAVE_FORMAT_IEEE_FLOAT && format->wBitsPerSample == 32)
        return SampleType::Float32;
    if (tag == WAVE_FORMAT_PCM) {
        if (format->wBitsPerSample == 16) return SampleType::Pcm16;
        if (format->wBitsPerSample == 24) return SampleType::Pcm24;
        if (format->wBitsPerSample == 32) return SampleType::Pcm32;
    }
    throw std::runtime_error("Unsupported speaker mix sample format");
}

void writeSample(BYTE *destination, SampleType type, float sample) {
    const double clipped = std::clamp(static_cast<double>(sample), -1.0, 1.0);
    if (type == SampleType::Float32) {
        std::memcpy(destination, &sample, sizeof(sample));
    } else if (type == SampleType::Pcm16) {
        const auto integer = static_cast<std::int16_t>(std::lrint(clipped * 32767.0));
        std::memcpy(destination, &integer, sizeof(integer));
    } else if (type == SampleType::Pcm24) {
        const auto integer = static_cast<std::int32_t>(std::lrint(clipped * 8388607.0));
        destination[0] = static_cast<BYTE>(integer);
        destination[1] = static_cast<BYTE>(integer >> 8);
        destination[2] = static_cast<BYTE>(integer >> 16);
    } else {
        const auto integer = static_cast<std::int32_t>(std::llrint(clipped * 2147483647.0));
        std::memcpy(destination, &integer, sizeof(integer));
    }
}

void checkFormat(IAudioClient *client, WAVEFORMATEX *format, const char *which) {
    WAVEFORMATEX *closest = nullptr;
    const HRESULT result = client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, format, &closest);
    if (closest) CoTaskMemFree(closest);
    if (result != S_OK)
        throw std::runtime_error(std::string(which) + " does not support shared 48 kHz stereo float audio");
}

ComPtr<IMMDeviceEnumerator> enumerator() {
    ComPtr<IMMDeviceEnumerator> result;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                           IID_PPV_ARGS(result.GetAddressOf())), "Enumerate audio devices");
    return result;
}

class OutputVolumeLease {
public:
    explicit OutputVolumeLease(IMMDevice *device) {
        check(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void **>(volume_.GetAddressOf())),
              "Read speaker volume");
        check(volume_->GetMasterVolumeLevelScalar(&original_), "Read speaker level");
        check(volume_->GetMute(&wasMuted_), "Read speaker mute");
        if (original_ < 0.999f)
            check(volume_->SetMasterVolumeLevelScalar(1.0f, nullptr), "Set full speaker level for EQ");
        if (wasMuted_) check(volume_->SetMute(FALSE, nullptr), "Unmute speaker for EQ");
    }
    ~OutputVolumeLease() {
        if (!volume_) return;
        float current = 0.0f;
        if (SUCCEEDED(volume_->GetMasterVolumeLevelScalar(&current)) && current > 0.999f)
            volume_->SetMasterVolumeLevelScalar(original_, nullptr);
        BOOL muted = FALSE;
        if (wasMuted_ && SUCCEEDED(volume_->GetMute(&muted)) && !muted)
            volume_->SetMute(TRUE, nullptr);
    }
private:
    ComPtr<IAudioEndpointVolume> volume_;
    float original_ = 1.0f;
    BOOL wasMuted_ = FALSE;
};

} // namespace

std::vector<AudioEndpoint> windowsAudioEndpoints(bool capture) {
    Apartment apartment;
    const auto devices = enumerator();
    ComPtr<IMMDeviceCollection> collection;
    check(devices->EnumAudioEndpoints(capture ? eCapture : eRender, DEVICE_STATE_ACTIVE,
                                      collection.GetAddressOf()), "List audio endpoints");
    UINT count = 0;
    check(collection->GetCount(&count), "Count audio endpoints");
    std::vector<AudioEndpoint> result;
    result.reserve(count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        check(collection->Item(i, device.GetAddressOf()), "Read audio endpoint");
        LPWSTR id = nullptr;
        check(device->GetId(&id), "Read audio endpoint ID");
        AudioEndpoint endpoint;
        endpoint.id = id;
        CoTaskMemFree(id);
        ComPtr<IPropertyStore> properties;
        check(device->OpenPropertyStore(STGM_READ, properties.GetAddressOf()),
              "Read audio endpoint properties");
        PROPVARIANT name;
        PropVariantInit(&name);
        check(properties->GetValue(PKEY_Device_FriendlyName, &name), "Read audio endpoint name");
        endpoint.name = name.vt == VT_LPWSTR ? name.pwszVal : L"Unnamed audio endpoint";
        PropVariantClear(&name);
        PROPVARIANT interfaceName;
        PropVariantInit(&interfaceName);
        if (SUCCEEDED(properties->GetValue(PKEY_DeviceInterface_FriendlyName, &interfaceName)) &&
            interfaceName.vt == VT_LPWSTR) {
            const std::wstring interfaceText = interfaceName.pwszVal;
            endpoint.virtualCable = interfaceText.find(L"VB-Audio") != std::wstring::npos;
        }
        PropVariantClear(&interfaceName);
        ComPtr<IAudioClient> audio;
        if (SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                       reinterpret_cast<void **>(audio.GetAddressOf())))) {
            WAVEFORMATEX *mix = nullptr;
            if (SUCCEEDED(audio->GetMixFormat(&mix))) {
                endpoint.channels = mix->nChannels;
                CoTaskMemFree(mix);
            }
        }
        result.push_back(std::move(endpoint));
    }
    return result;
}

std::wstring windowsDefaultEndpointId(bool capture, int role) {
    Apartment apartment;
    const auto devices = enumerator();
    ComPtr<IMMDevice> device;
    check(devices->GetDefaultAudioEndpoint(capture ? eCapture : eRender, static_cast<ERole>(role), device.GetAddressOf()),
          "Read default output endpoint");
    LPWSTR id = nullptr;
    check(device->GetId(&id), "Read default output ID");
    std::wstring result(id);
    CoTaskMemFree(id);
    return result;
}

std::wstring windowsDefaultOutputId() { return windowsDefaultEndpointId(false); }
std::wstring windowsDefaultInputId() { return windowsDefaultEndpointId(true); }

std::string WindowsBridge::error() const {
    std::lock_guard lock(stateMutex_);
    return error_;
}
std::vector<std::int16_t> WindowsBridge::takeMeterPcm() {
    std::lock_guard lock(meterMutex_);
    std::vector<std::int16_t> result(meterPcm_.begin(), meterPcm_.begin() + meterSamples_);
    meterSamples_ = 0;
    return result;
}

WindowsBridge::~WindowsBridge() { stop(); }

bool WindowsBridge::start(std::wstring captureId, std::wstring outputId, bool microphone) {
    if (captureId.empty() || outputId.empty() || worker_.joinable()) return false;
    { std::lock_guard lock(stateMutex_); ready_ = false; error_.clear(); }
    stopRequested_ = false;
    running_ = false;
    worker_ = std::thread([this, captureId = std::move(captureId),
                           outputId = std::move(outputId), microphone]() mutable {
        run(std::move(captureId), std::move(outputId), microphone);
    });
    std::unique_lock lock(stateMutex_);
    const bool ready = initialized_.wait_for(lock, std::chrono::seconds(5), [this] {
        return ready_ || !error_.empty();
    }) && ready_;
    lock.unlock();
    if (!ready) stop();
    return ready;
}

void WindowsBridge::stop() {
    stopRequested_ = true;
    if (worker_.joinable()) worker_.join();
    running_ = false;
    peak_ = 0.0f;
    { std::lock_guard lock(meterMutex_); meterSamples_ = 0; }
}

bool WindowsBridge::setProfile(std::span<const EqBand> bands, double postGainDb,
                               int balancePercent, bool enabled, bool automaticHeadroom, const EnhancementSettings &effects) {
    StereoEqualizer validator(48000);
    if (!validator.setProfile(bands, postGainDb, balancePercent, enabled, automaticHeadroom)) return false;
    if(!effects.valid())return false;
    Profile proposed;
    proposed.count = bands.size();
    std::copy(bands.begin(), bands.end(), proposed.bands.begin());
    proposed.postGainDb = postGainDb;
    proposed.balancePercent = balancePercent;
    proposed.enabled = enabled;
    proposed.automaticHeadroom = automaticHeadroom;
    proposed.effects = effects;
    std::lock_guard guard(profileMutex_);
    profile_ = proposed;
    ++profileVersion_;
    return true;
}

void WindowsBridge::run(std::wstring captureId, std::wstring outputId, bool microphone) {
    try {
        Apartment apartment;
        const auto devices = enumerator();
        ComPtr<IMMDevice> captureDevice, outputDevice;
        check(devices->GetDevice(captureId.c_str(), captureDevice.GetAddressOf()),
              "Open cable recording endpoint");
        check(devices->GetDevice(outputId.c_str(), outputDevice.GetAddressOf()),
              "Open speaker endpoint");
        ComPtr<IAudioClient> captureAudio, outputAudio;
        check(captureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                      reinterpret_cast<void **>(captureAudio.GetAddressOf())),
              "Open cable capture stream");
        check(outputDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                     reinterpret_cast<void **>(outputAudio.GetAddressOf())),
              "Open speaker render stream");
        auto captureFormat = stereoFloat48k();
        if (microphone) {
            WAVEFORMATEX *mix = nullptr;
            check(captureAudio->GetMixFormat(&mix), "Read microphone mix format");
            if (mix->nChannels == 1) {
                captureFormat.nChannels = 1;
                captureFormat.nBlockAlign = 4;
                captureFormat.nAvgBytesPerSec = 48000 * 4;
            }
            CoTaskMemFree(mix);
        } else checkFormat(captureAudio.Get(), &captureFormat, "Cable recording endpoint");
        WAVEFORMATEX *mixFormatRaw = nullptr;
        check(outputAudio->GetMixFormat(&mixFormatRaw), "Read speaker mix format");
        std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> outputFormat(mixFormatRaw, CoTaskMemFree);
        if (!outputFormat || outputFormat->nChannels < 2 ||
            outputFormat->nSamplesPerSec < 8000 || outputFormat->nSamplesPerSec > 384000 ||
            outputFormat->nBlockAlign != outputFormat->nChannels * outputFormat->wBitsPerSample / 8)
            throw std::runtime_error("Unsupported speaker channel layout or sample rate");
        const auto sampleType = outputSampleType(outputFormat.get());
        OutputVolumeLease volume(outputDevice.Get());
        constexpr REFERENCE_TIME bufferTime = 2000000; // 200 ms, in 100 ns units
        check(captureAudio->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                        microphone ? AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY : 0,
                                        bufferTime, 0, &captureFormat, nullptr), "Initialize cable capture");
        check(outputAudio->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferTime, 0,
                                       outputFormat.get(), nullptr), "Initialize speaker output");
        UINT32 captureBufferFrames = 0, outputBufferFrames = 0;
        check(captureAudio->GetBufferSize(&captureBufferFrames), "Size capture buffer");
        check(outputAudio->GetBufferSize(&outputBufferFrames), "Size output buffer");
        ComPtr<IAudioCaptureClient> reader;
        ComPtr<IAudioRenderClient> writer;
        check(captureAudio->GetService(IID_PPV_ARGS(reader.GetAddressOf())),
              "Read cable capture interface");
        check(outputAudio->GetService(IID_PPV_ARGS(writer.GetAddressOf())),
              "Read speaker render interface");
        std::vector<float> scratch(std::max<UINT32>(captureBufferFrames, 1) * 2);
        constexpr std::size_t ringCapacity = 48000;
        std::vector<float> ring(ringCapacity * 2);
        std::size_t readPosition = 0, writePosition = 0, queued = 0;
        double resamplePhase = 0.0;
        const double sourceFramesPerOutputFrame = 48000.0 / outputFormat->nSamplesPerSec;
        StereoEqualizer eq(48000);
        unsigned long long appliedVersion = 0;
        DWORD mmcssTask = 0;
        HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &mmcssTask);
        check(outputAudio->Start(), "Start speaker output");
        check(captureAudio->Start(), "Start cable capture");
        { std::lock_guard lock(stateMutex_); ready_ = true; running_ = true; }
        initialized_.notify_all();
        if (status_) status_(L"Processing audio through the selected output");
        while (!stopRequested_) {
            Profile pending;
            bool changed = false;
            if (profileMutex_.try_lock()) {
                if (profileVersion_ != appliedVersion) {
                    pending = profile_;
                    appliedVersion = profileVersion_;
                    changed = true;
                }
                profileMutex_.unlock();
            }
            if (changed && !eq.setProfile(std::span(pending.bands.data(), pending.count),
                                          pending.postGainDb, pending.balancePercent,
                                          pending.enabled, pending.automaticHeadroom))
                throw std::runtime_error("The selected EQ settings are invalid");

            if(changed && !eq.setEnhancements(pending.effects))throw std::runtime_error("Invalid enhancements");

            UINT32 packetFrames = 0;
            check(reader->GetNextPacketSize(&packetFrames), "Read cable packet size");
            while (packetFrames) {
                BYTE *data = nullptr;
                DWORD flags = 0;
                check(reader->GetBuffer(&data, &packetFrames, &flags, nullptr, nullptr),
                      "Read cable audio");
                if (packetFrames > captureBufferFrames)
                    throw std::runtime_error("Cable packet exceeds its capture buffer");
                const std::size_t samples = static_cast<std::size_t>(packetFrames) * 2;
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) std::fill_n(scratch.data(), samples, 0.0f);
                else if (captureFormat.nChannels == 1) {
                    const auto *mono = reinterpret_cast<const float *>(data);
                    for (UINT32 i = 0; i < packetFrames; ++i)
                        scratch[i * 2] = scratch[i * 2 + 1] = mono[i];
                } else std::memcpy(scratch.data(), data, samples * sizeof(float));
                peak_ = eq.process(scratch.data(), packetFrames);
                if (!microphone && meterMutex_.try_lock()) {
                    // Only the visible UI consumes this bounded tap. Never block audio.
                    constexpr std::size_t limit = 32768;
                    if (meterSamples_ + samples > limit) meterSamples_ = 0;
                    if (samples <= limit) for (std::size_t i = 0; i < samples; ++i)
                        meterPcm_[meterSamples_++] = static_cast<std::int16_t>(std::lround(
                            std::clamp(scratch[i], -1.0f, 1.0f) * 32767.0f));
                    meterMutex_.unlock();
                }
                for (UINT32 frame = 0; frame < packetFrames; ++frame) {
                    if (queued == ringCapacity) {
                        readPosition = (readPosition + 1) % ringCapacity;
                        --queued;
                    }
                    ring[writePosition * 2] = scratch[frame * 2];
                    ring[writePosition * 2 + 1] = scratch[frame * 2 + 1];
                    writePosition = (writePosition + 1) % ringCapacity;
                    ++queued;
                }
                check(reader->ReleaseBuffer(packetFrames), "Release cable audio");
                check(reader->GetNextPacketSize(&packetFrames), "Read next cable packet size");
            }

            UINT32 padding = 0;
            check(outputAudio->GetCurrentPadding(&padding), "Read output buffer level");
            const UINT32 writable = outputBufferFrames - padding;
            if (writable) {
                BYTE *raw = nullptr;
                check(writer->GetBuffer(writable, &raw), "Write speaker buffer");
                const std::size_t bytesPerSample = outputFormat->wBitsPerSample / 8;
                for (UINT32 frame = 0; frame < writable; ++frame) {
                    float left = 0.0f, right = 0.0f;
                    if (queued >= 2) {
                        const auto next = (readPosition + 1) % ringCapacity;
                        left = static_cast<float>(ring[readPosition * 2] * (1.0 - resamplePhase) +
                                                  ring[next * 2] * resamplePhase);
                        right = static_cast<float>(ring[readPosition * 2 + 1] * (1.0 - resamplePhase) +
                                                   ring[next * 2 + 1] * resamplePhase);
                        resamplePhase += sourceFramesPerOutputFrame;
                        while (resamplePhase >= 1.0 && queued) {
                            readPosition = (readPosition + 1) % ringCapacity;
                            --queued;
                            resamplePhase -= 1.0;
                        }
                    }
                    BYTE *destination = raw + static_cast<std::size_t>(frame) * outputFormat->nBlockAlign;
                    std::memset(destination, 0, outputFormat->nBlockAlign);
                    writeSample(destination, sampleType, left);
                    writeSample(destination + bytesPerSample, sampleType, right);
                }
                check(writer->ReleaseBuffer(writable, 0), "Release speaker buffer");
            }
            Sleep(5);
        }
        captureAudio->Stop();
        outputAudio->Stop();
        if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
        if (status_) status_(L"Equalizer stopped; normal output is available");
    } catch (const std::exception &error) {
        { std::lock_guard lock(stateMutex_); error_ = error.what(); }
        initialized_.notify_all();
        if (status_) status_(L"Audio bridge error: " + widen(error.what()));
    }
    running_ = false;
    peak_ = 0.0f;
}

} // namespace soundcurrent
#endif
