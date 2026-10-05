// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "dsp.h"

#include <array>
#include <cstdint>
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace soundcurrent {

struct AudioEndpoint {
    std::wstring id;
    std::wstring name;
    unsigned channels = 2;
    bool virtualCable = false;
};

std::vector<AudioEndpoint> windowsAudioEndpoints(bool capture);
std::wstring windowsDefaultOutputId();
std::wstring windowsDefaultInputId();
std::wstring windowsDefaultEndpointId(bool capture, int role = 1);

// Reversible route ownership. Restore only roles that still point to our cable.
class WindowsRouteLease {
public:
    WindowsRouteLease(bool capture, const std::wstring &cableId,
                      const std::wstring &fallbackId, bool copyPlaybackVolume = false);
    ~WindowsRouteLease();
    WindowsRouteLease(const WindowsRouteLease &) = delete;
    WindowsRouteLease &operator=(const WindowsRouteLease &) = delete;
private:
    bool capture_;
    bool volume_;
    std::wstring cable_, fallback_;
    std::array<std::wstring, 3> originals_;
};

// A fixed-format capture stream; Windows converts a physical microphone's mix
// format. Captured PCM remains in a bounded buffer and is never persisted.
class WindowsRecorder {
public:
    WindowsRecorder();
    ~WindowsRecorder();
    void start(const std::wstring &endpoint, int sampleRate = 96000, int channels = 1);
    void stop();
    std::vector<std::int16_t> take();
    bool running() const;
    std::string error() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
void windowsPlayPcm(const std::wstring &endpoint, std::span<const std::int16_t> pcm,
                    int sampleRate = 96000);

class WindowsBridge {
public:
    using StatusCallback = std::function<void(const std::wstring &)>;

    WindowsBridge() = default;
    ~WindowsBridge();
    WindowsBridge(const WindowsBridge &) = delete;
    WindowsBridge &operator=(const WindowsBridge &) = delete;

    bool start(std::wstring captureId, std::wstring outputId, bool microphone = false);
    std::vector<std::int16_t> takeMeterPcm();
    std::string error() const;
    void stop();
    bool running() const { return running_.load(); }
    float peak() const { return peak_.load(); }
    void setStatusCallback(StatusCallback callback) { status_ = std::move(callback); }
    bool setProfile(std::span<const EqBand> bands, double postGainDb,
                    int balancePercent, bool enabled, bool automaticHeadroom = true);

private:
    struct Profile {
        std::array<EqBand, kMaxProcessingBands> bands{};
        std::size_t count = 0;
        double postGainDb = 0.0;
        int balancePercent = 0;
        bool enabled = true;
        bool automaticHeadroom = true;
    };

    void run(std::wstring captureId, std::wstring outputId, bool microphone);
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    std::atomic<float> peak_{0.0f};
    std::thread worker_;
    std::mutex profileMutex_;
    Profile profile_;
    unsigned long long profileVersion_ = 1;
    StatusCallback status_;
    mutable std::mutex stateMutex_;
    std::condition_variable initialized_;
    bool ready_ = false;
    std::string error_;
    std::mutex meterMutex_;
    std::array<std::int16_t, 32768> meterPcm_{};
    std::size_t meterSamples_ = 0;
};

} // namespace soundcurrent
