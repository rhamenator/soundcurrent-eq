// SPDX-License-Identifier: GPL-3.0-only
// Opt-in Windows integration test: --run plays a quiet tone through VB-CABLE.
#define NOMINMAX
#include "windows_audio.h"
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT result, const char *action) {
    if (FAILED(result)) {
        char message[160];
        std::snprintf(message, sizeof(message), "%s: 0x%08lX", action,
                      static_cast<unsigned long>(result));
        throw std::runtime_error(message);
    }
}
struct Measurement { double left = 0, right = 0; std::size_t frames = 0; };
class Probe {
public:
    Probe(const std::wstring &renderId, const std::wstring &loopbackId) {
        ComPtr<IMMDeviceEnumerator> devices;
        check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                               IID_PPV_ARGS(devices.GetAddressOf())), "Enumerator");
        ComPtr<IMMDevice> renderDevice, loopbackDevice;
        check(devices->GetDevice(renderId.c_str(), renderDevice.GetAddressOf()), "Tone device");
        check(devices->GetDevice(loopbackId.c_str(), loopbackDevice.GetAddressOf()), "Loopback device");
        check(renderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
              reinterpret_cast<void **>(render_.GetAddressOf())), "Tone client");
        check(loopbackDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
              reinterpret_cast<void **>(capture_.GetAddressOf())), "Loopback client");
        WAVEFORMATEX *raw = nullptr;
        check(render_->GetMixFormat(&raw), "Tone mix format");
        renderFormat_.reset(raw);
        check(capture_->GetMixFormat(&raw), "Loopback mix format");
        captureFormat_.reset(raw);
        for (const auto *f : {renderFormat_.get(), captureFormat_.get()}) {
            const bool floating = f->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                (f->wFormatTag == WAVE_FORMAT_EXTENSIBLE && f->cbSize >= 22 &&
                 reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(f)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            if (!floating || f->wBitsPerSample != 32 || f->nChannels < 2)
                throw std::runtime_error("This diagnostic requires stereo or multichannel float mix formats");
        }
        std::printf("Tone mix: %lu Hz, %u channels; loopback: %lu Hz, %u channels\n",
                    renderFormat_->nSamplesPerSec, renderFormat_->nChannels,
                    captureFormat_->nSamplesPerSec, captureFormat_->nChannels);
        check(render_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0,
                                   renderFormat_.get(), nullptr), "Tone initialize");
        check(capture_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                    1000000, 0, captureFormat_.get(), nullptr), "Loopback initialize");
        check(render_->GetBufferSize(&capacity_), "Tone buffer size");
        check(render_->GetService(IID_PPV_ARGS(writer_.GetAddressOf())), "Tone writer");
        check(capture_->GetService(IID_PPV_ARGS(reader_.GetAddressOf())), "Loopback reader");
        check(capture_->Start(), "Start loopback");
        check(render_->Start(), "Start tone");
    }
    ~Probe() { if (render_) render_->Stop(); if (capture_) capture_->Stop(); }
    Measurement measure(double frequency, unsigned durationMs = 1600) {
        Measurement result;
        const auto begin = GetTickCount64();
        while (GetTickCount64() - begin < durationMs) {
            UINT32 padding = 0;
            check(render_->GetCurrentPadding(&padding), "Tone padding");
            const UINT32 available = capacity_ - padding;
            if (available) {
                BYTE *raw = nullptr;
                check(writer_->GetBuffer(available, &raw), "Tone buffer");
                auto *samples = reinterpret_cast<float *>(raw);
                std::fill_n(samples, available * renderFormat_->nChannels, 0.0f);
                for (UINT32 frame = 0; frame < available; ++frame) {
                    const auto value = static_cast<float>(0.02 * std::sin(phase_));
                    phase_ = std::fmod(phase_ + 2 * std::numbers::pi * frequency /
                                       renderFormat_->nSamplesPerSec, 2 * std::numbers::pi);
                    samples[frame * renderFormat_->nChannels] = value;
                    samples[frame * renderFormat_->nChannels + 1] = value;
                }
                check(writer_->ReleaseBuffer(available, 0), "Release tone");
            }
            UINT32 frames = 0;
            check(reader_->GetNextPacketSize(&frames), "Loopback packet");
            while (frames) {
                BYTE *raw = nullptr;
                DWORD flags = 0;
                check(reader_->GetBuffer(&raw, &frames, &flags, nullptr, nullptr), "Loopback read");
                if (GetTickCount64() - begin >= 800) {
                    if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                        const auto *samples = reinterpret_cast<const float *>(raw);
                        for (UINT32 frame = 0; frame < frames; ++frame) {
                            const float left = samples[frame * captureFormat_->nChannels];
                            const float right = samples[frame * captureFormat_->nChannels + 1];
                            result.left += left * left;
                            result.right += right * right;
                        }
                    }
                    result.frames += frames;
                }
                check(reader_->ReleaseBuffer(frames), "Release loopback");
                check(reader_->GetNextPacketSize(&frames), "Next loopback packet");
            }
            Sleep(3);
        }
        if (!result.frames) throw std::runtime_error("No physical-output loopback frames");
        result.left = std::sqrt(result.left / result.frames);
        result.right = std::sqrt(result.right / result.frames);
        return result;
    }
private:
    ComPtr<IAudioClient> render_, capture_;
    ComPtr<IAudioRenderClient> writer_;
    ComPtr<IAudioCaptureClient> reader_;
    std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> renderFormat_{nullptr, CoTaskMemFree},
                                                         captureFormat_{nullptr, CoTaskMemFree};
    UINT32 capacity_ = 0;
    double phase_ = 0;
};
double decibels(double value, double reference) { return 20 * std::log10(value / reference); }
}

int main(int argc, char **argv) {
    try {
        check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "COM");
        std::wstring cableInput, cableOutput, speakers;
        for (bool capture : {false, true}) {
            for (const auto &device : soundcurrent::windowsAudioEndpoints(capture)) {
                // Keep narrow console output independent of the Windows console code page.
                const std::string name(device.name.begin(), device.name.end());
                std::printf("%s: %s\n", capture ? "Capture" : "Render", name.c_str());
                if (!capture && device.name.find(L"CABLE Input") != std::wstring::npos) cableInput = device.id;
                else if (capture && device.name.find(L"CABLE Output") != std::wstring::npos) cableOutput = device.id;
                else if (!capture && device.name.find(L"VB-Audio") == std::wstring::npos && speakers.empty()) speakers = device.id;
            }
        }
        if (argc != 2 || std::string(argv[1]) != "--run") {
            std::puts("Use --run in an isolated Windows test system to play a -34 dBFS test tone.");
            return 0;
        }
        if (cableInput.empty() || cableOutput.empty() || speakers.empty())
            throw std::runtime_error("VB-CABLE and a physical stereo output are required");
        soundcurrent::WindowsBridge bridge;
        bridge.setStatusCallback([](const std::wstring &message) {
            std::printf("Bridge: %s\n", std::string(message.begin(), message.end()).c_str());
        });
        bridge.setProfile({}, 0, 0, true);
        if (!bridge.start(cableOutput, speakers)) throw std::runtime_error("Bridge did not start");
        Probe probe(cableInput, speakers);
        const auto flat = probe.measure(1000);
        std::printf("Flat frames: %zu, bridge peak: %.6f, RMS: %.6f / %.6f\n", flat.frames, bridge.peak(), flat.left, flat.right);
        if (!bridge.running() || flat.left < 0.0001 || flat.right < 0.0001)
            throw std::runtime_error("Flat route is silent");
        std::printf("Flat RMS: L %.6f, R %.6f\n", flat.left, flat.right);
        bridge.setProfile({}, 6, 0, true);
        const auto gain = probe.measure(1000);
        const double gainDb = decibels(gain.left, flat.left);
        std::printf("Live post gain: %.2f dB (expected +6)\n", gainDb);
        if (std::abs(gainDb - 6) > 0.5) throw std::runtime_error("Live gain change failed");
        const std::array bands{soundcurrent::EqBand{1000, -12, 1}};
        bridge.setProfile(bands, 0, 0, true);
        const auto cut = probe.measure(1000);
        const double cutDb = decibels(cut.left, flat.left);
        std::printf("Live 1 kHz EQ cut: %.2f dB (expected -12)\n", cutDb);
        if (std::abs(cutDb + 12) > 0.5) throw std::runtime_error("Live EQ change failed");
        bridge.setProfile(bands, 6, 75, false);
        const auto bypass = probe.measure(1000);
        const double bypassDb = decibels(bypass.left, flat.left);
        std::printf("Bypass: %.2f dB relative to Flat (expected 0)\n", bypassDb);
        if (std::abs(bypassDb) > 0.5) throw std::runtime_error("Bypass failed");
        bridge.setProfile({}, 0, -100, true);
        const auto balance = probe.measure(1000);
        std::printf("Full-left balance RMS: L %.6f, R %.6f\n", balance.left, balance.right);
        if (std::abs(decibels(balance.left, flat.left)) > 0.5 || balance.right > flat.right * 0.001)
            throw std::runtime_error("Live balance change failed");
        bridge.stop();
        bridge.setProfile({}, 0, 0, true);
        if (!bridge.start(cableOutput, speakers)) throw std::runtime_error("Bridge restart failed");
        const auto restarted = probe.measure(1000);
        if (!bridge.running() || std::abs(decibels(restarted.left, flat.left)) > 0.5)
            throw std::runtime_error("Restarted audio is silent or changed level");
        bridge.stop();
        std::puts("PASS: live route, post gain, EQ, bypass, balance, and restart");
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
