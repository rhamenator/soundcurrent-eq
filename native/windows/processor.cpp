// SPDX-License-Identifier: GPL-3.0-only
#include "processor.h"
#include <cstring>
#include <thread>
namespace soundcurrent::native {
bool Processor::prepare(int rate, unsigned channels, unsigned maxFrames) {
    if (channels != 2 || rate < 8000 || rate > 384000 || !maxFrames || maxFrames > 1048576) return false;
    auto eq = std::make_unique<StereoEqualizer>(rate);
    PreparedEqProfile bypass;
    eq->applyPreparedProfile(bypass);
    std::lock_guard lock(producer_);
    eq_ = std::move(eq); rate_ = rate; maxFrames_ = maxFrames;
    enabled_ = pending_ = false; snapshot_ = {}; return true;
}
bool Processor::submit(std::span<const EqBand> bands, double gain, int balance,
                       bool enabled, bool headroom, const EnhancementSettings &effects) {
    std::lock_guard lock(producer_);
    if (!eq_ || !effects.valid()) return false;
    Snapshot next;
    if (!prepareEqProfile(bands, rate_, gain, balance, enabled, headroom, next.eq)) return false;
    next.effects = effects;
    // Only the non-realtime producer may wait. The audio consumer never retries.
    while (mailbox_.test_and_set(std::memory_order_acquire)) std::this_thread::yield();
    snapshot_ = next; pending_ = true;
    mailbox_.clear(std::memory_order_release); return true;
}
bool Processor::process(const float *input, float *output, unsigned frames, bool silent) {
    if (!eq_ || !output || frames > maxFrames_ || (!silent && !input)) return false;
    Snapshot next; bool changed = false;
    if (!mailbox_.test_and_set(std::memory_order_acquire)) {
        if (pending_) { next = snapshot_; pending_ = false; changed = true; }
        mailbox_.clear(std::memory_order_release);
    }
    if (changed) {
        eq_->applyPreparedProfile(next.eq); eq_->setEnhancements(next.effects);
        enabled_ = next.eq.enabled;
    }
    const auto bytes = std::size_t(frames) * 2 * sizeof(float);
    if (silent) std::memset(output, 0, bytes);
    else if (input != output) std::memmove(output, input, bytes);
    if (enabled_) eq_->process(output, frames);
    return true;
}
void Processor::reset() { if (eq_) eq_->reset(); }
}
