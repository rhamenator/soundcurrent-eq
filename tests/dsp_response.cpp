// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"

#include <cmath>
#include <cstdio>
#include <numbers>
#include <array>
#include <vector>

namespace {
double measure(soundcurrent::StereoEqualizer &eq, double frequency) {
    constexpr int rate = 48000;
    std::vector<float> samples(rate * 2);
    for (int i = 0; i < rate; ++i)
        samples[i * 2] = samples[i * 2 + 1] =
            static_cast<float>(0.1 * std::sin(2.0 * std::numbers::pi * frequency * i / rate));
    eq.reset();
    eq.process(samples.data(), rate);
    double sum = 0.0;
    for (int i = rate / 2; i < rate; ++i) sum += samples[i * 2] * samples[i * 2];
    return std::sqrt(sum / (rate / 2));
}
}

int main() {
    soundcurrent::StereoEqualizer eq(48000);
    if (!eq.setProfile({}, 0.0, 0)) return 1;
    const double flat = measure(eq, 1000.0);
    const std::array cutBand{soundcurrent::EqBand{1000.0, -12.0, 1.0}};
    if (!eq.setProfile(cutBand, 0.0, 0)) return 2;
    const double cut = measure(eq, 1000.0);
    const double cutDb = 20.0 * std::log10(cut / flat);
    if (std::abs(cutDb + 12.0) > 0.5) return 3;
    if (!eq.setProfile({}, 6.0, 0)) return 4;
    const double gainDb = 20.0 * std::log10(measure(eq, 1000.0) / flat);
    if (std::abs(gainDb - 6.0) > 0.2) return 5;
    if (!eq.setProfile({}, 0.0, -100)) return 6;
    float stereo[2] = {0.2f, 0.2f};
    eq.process(stereo, 1);
    if (std::abs(stereo[0] - 0.2f) > 1e-5 || std::abs(stereo[1]) > 1e-5) return 7;
    const std::array invalidBand{soundcurrent::EqBand{1000.0, 99.0, 1.0}};
    if (eq.setProfile(invalidBand, 0.0, 0)) return 8;
    using T = soundcurrent::FilterType;
    for (auto type : {T::LowShelf, T::HighShelf}) {
        const double center = type == T::LowShelf ? 400.0 : 4000.0;
        const double frequency = type == T::LowShelf ? 40.0 : 18000.0;
        const std::array shelf{soundcurrent::EqBand{center, -6.0, 0.707, type}};
        if (!eq.setProfile(shelf, 0, 0)) return 9;
        const double expected = soundcurrent::filterResponseDb(shelf[0], 48000, frequency);
        const double ratio = 20 * std::log10(measure(eq, frequency) / flat);
        if (std::abs(ratio - expected) > 0.2 || std::abs(expected + 6.0) > 0.3) return 10;
    }
    const std::array highpass{soundcurrent::EqBand{80, 0, 0.707, T::HighPass}};
    if (!eq.setProfile(highpass, 0, 0)) return 11;
    if (20 * std::log10(measure(eq, 20) / flat) > -22.0) return 12;
    std::vector<soundcurrent::EqBand> combined(31, {1000, 0, 1});
    combined.push_back({12246, -1.48, 0.29}); // published JBL correction Q
    combined.push_back({1000, -3, 1});
    if (!eq.setProfile(combined, 0, 0)) return 13;
    combined.resize(soundcurrent::kMaxProcessingBands + 1, {1000, 0, 1});
    if (eq.setProfile(combined, 0, 0)) return 14;
    if(!eq.setProfile({},0,0))return 15;
    soundcurrent::EnhancementSettings effects;effects.values[soundcurrent::BassBoost]=1;
    if(!eq.setEnhancements(effects))return 16;
    if(measure(eq,40)<flat*2)return 17;
    if(!eq.setProfile({},0,0,false) || std::abs(measure(eq,40)-flat)>1e-5)return 18;
    std::printf("Windows DSP core: 1 kHz cut %.1f dB, post gain %.1f dB, balance passed\n",
                cutDb, gainDb);
    return 0;
}
