// SPDX-License-Identifier: MS-PL
// SoundCurrent additions to the Microsoft SYSVAD-derived driver.
#pragma once
#ifdef SOUNDCURRENT_KERNEL
#include <ntddk.h>
#else
#include <atomic>
#include <cstdint>
#endif
namespace soundcurrent_driver {
// One 48 kHz stereo PCM16 producer and one consumer. The endpoint tables must
// enforce one kernel stream per direction; Windows shared mode mixes clients.
// All operations are bounded, allocation-free and nonblocking. Control calls
// require the corresponding stream's callbacks to be quiescent.
static_assert(sizeof(short) == 2 && sizeof(unsigned) == 4, "PCM16 and 32-bit counters required");
class MicTransport {
    class Counter {
#ifdef SOUNDCURRENT_KERNEL
        volatile LONG value_ = 0;
    public:
        unsigned load() { return static_cast<unsigned>(InterlockedCompareExchange(&value_, 0, 0)); }
        void store(unsigned value) { InterlockedExchange(&value_, static_cast<LONG>(value)); }
        void increment() { InterlockedIncrement(&value_); }
#else
        std::atomic<std::uint32_t> value_{0};
    public:
        unsigned load() { return value_.load(std::memory_order_seq_cst); }
        void store(unsigned value) { value_.store(value, std::memory_order_seq_cst); }
        void increment() { value_.fetch_add(1, std::memory_order_seq_cst); }
#endif
    };
    Counter write_, read_, epoch_, producerActive_;
    unsigned consumerEpoch_ = 0;
public:
    static constexpr unsigned capacityFrames = 2048;
private:
    short pcm_[capacityFrames * 2]{};
public:
    void startProducer() {
        producerActive_.store(0);
        epoch_.increment();
        producerActive_.store(1);
    }
    void stopProducer() { producerActive_.store(0); epoch_.increment(); }
    void startConsumer() {
        // A newly connected recorder must never receive old queued microphone
        // audio from before it was started.
        read_.store(write_.load());
        consumerEpoch_ = epoch_.load();
    }
    unsigned write(const short* stereo, unsigned frames) {
        if (!stereo || !producerActive_.load()) return 0;
        const unsigned position = write_.load();
        const unsigned used = position - read_.load();
        if (used > capacityFrames) return 0;
        const unsigned available = capacityFrames - used;
        const unsigned count = frames < available ? frames : available;
        for (unsigned i = 0; i < count; ++i) {
            const unsigned offset = ((position + i) % capacityFrames) * 2;
            pcm_[offset] = stereo[i * 2]; pcm_[offset + 1] = stereo[i * 2 + 1];
        }
        write_.store(position + count);
        // Overflow drops new frames; the producer never modifies consumer state.
        return count;
    }
    unsigned read(short* stereo, unsigned frames) {
        if (!stereo) return 0;
        const unsigned epoch = epoch_.load();
        if (!producerActive_.load() || epoch != consumerEpoch_) {
            startConsumer();
            for (unsigned i = 0; i < frames; ++i) stereo[i * 2] = stereo[i * 2 + 1] = 0;
            return 0;
        }
        const unsigned position = read_.load();
        const unsigned available = write_.load() - position;
        const unsigned count = available <= capacityFrames ? (frames < available ? frames : available) : 0;
        for (unsigned i = 0; i < count; ++i) {
            const unsigned offset = ((position + i) % capacityFrames) * 2;
            stereo[i * 2] = pcm_[offset]; stereo[i * 2 + 1] = pcm_[offset + 1];
        }
        for (unsigned i = count; i < frames; ++i) stereo[i * 2] = stereo[i * 2 + 1] = 0;
        read_.store(position + count);
        if (!producerActive_.load() || epoch_.load() != epoch) {
            for (unsigned i = 0; i < frames; ++i) stereo[i * 2] = stereo[i * 2 + 1] = 0;
            return 0;
        }
        return count;
    }
};
} // namespace soundcurrent_driver
