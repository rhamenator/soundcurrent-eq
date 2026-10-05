// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "dsp.h"

#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace soundcurrent {

struct AudioEndpoint {
    std::wstring id;
    std::wstring name;
};

std::vector<AudioEndpoint> windowsAudioEndpoints(bool capture);
std::wstring windowsDefaultOutputId();

class WindowsBridge {
public:
    using StatusCallback = std::function<void(const std::wstring &)>;

    WindowsBridge() = default;
    ~WindowsBridge();
    WindowsBridge(const WindowsBridge &) = delete;
    WindowsBridge &operator=(const WindowsBridge &) = delete;

    bool start(std::wstring captureId, std::wstring outputId);
    void stop();
    bool running() const { return running_.load(); }
    float peak() const { return peak_.load(); }
    void setStatusCallback(StatusCallback callback) { status_ = std::move(callback); }
    bool setProfile(std::span<const EqBand> bands, double postGainDb,
                    int balancePercent, bool enabled);

private:
    struct Profile {
        std::array<EqBand, 31> bands{};
        std::size_t count = 0;
        double postGainDb = 0.0;
        int balancePercent = 0;
        bool enabled = true;
    };

    void run(std::wstring captureId, std::wstring outputId);
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> running_{false};
    std::atomic<float> peak_{0.0f};
    std::thread worker_;
    std::mutex profileMutex_;
    Profile profile_;
    unsigned long long profileVersion_ = 1;
    StatusCallback status_;
};

} // namespace soundcurrent
