// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "dsp.h"
#include <atomic>
#include <memory>
#include <mutex>
namespace soundcurrent::native {
// Experimental stereo float32 adapter. Control methods are non-RT; prepare/reset
// require the host to stop processing first. Processing makes one mailbox attempt.
class Processor {
public:
    bool prepare(int sampleRate, unsigned channels, unsigned maxFrames);
    bool submit(std::span<const EqBand> bands, double gain, int balance, bool enabled,
                bool headroom, const EnhancementSettings &effects);
    bool process(const float *input, float *output, unsigned frames, bool silent);
    void reset();
    bool enabled() const { return enabled_; } // audio thread only
private:
    struct Snapshot { PreparedEqProfile eq; EnhancementSettings effects; };
    std::unique_ptr<StereoEqualizer> eq_;
    int rate_ = 0; unsigned maxFrames_ = 0;
    bool enabled_ = false, pending_ = false;
    Snapshot snapshot_;
    std::mutex producer_;
    std::atomic_flag mailbox_ = ATOMIC_FLAG_INIT;
};
}
