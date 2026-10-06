// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <vector>
#include <cstddef>
namespace soundcurrent {
enum EffectParameter : std::size_t { Clarity, Ambience, Surround, DynamicBoost, BassBoost,
    ClarityHz, BassHz, Decay, Damping, Width, Threshold, Ratio, Attack, Release, Makeup, Ceiling, EffectParameterCount };
struct EffectParameterInfo { const char *name; double low, high, initial, step; const char *unit; };
inline constexpr std::array<EffectParameterInfo,EffectParameterCount> effectParameters{{
    {"Clarity",0,1,0,.01,""},{"Ambience",0,1,0,.01,""},{"Surround Sound",0,1,0,.01,""},
    {"Dynamic Boost",0,1,0,.01,""},{"Bass Boost",0,1,0,.01,""},
    {"Clarity frequency",1000,10000,3000,100," Hz"},{"Bass frequency",35,250,90,5," Hz"},
    {"Ambience decay",.1,5,1.2,.1," s"},{"Ambience damping",0,.95,.45,.05,""},
    {"Maximum stereo width",1,2,1.6,.05,"×"},{"Dynamics threshold",-48,-6,-24,1," dBFS"},
    {"Dynamics ratio",1,10,3,.1,":1"},{"Dynamics attack",1,100,10,1," ms"},
    {"Dynamics release",20,1000,180,10," ms"},{"Dynamics makeup",0,18,9,.5," dB"},
    {"Dynamics ceiling",-12,-.1,-1,.1," dBFS"}
}};
struct EnhancementSettings {
    std::array<double,EffectParameterCount> values{};
    EnhancementSettings() { for(std::size_t i=0;i<values.size();++i) values[i]=effectParameters[i].initial; }
    bool valid() const noexcept;
    bool active() const noexcept;
    bool operator==(const EnhancementSettings &) const = default;
};
// Construct off the audio thread. Updates and sample processing never allocate,
// lock, log or perform I/O. Amounts ramp over 20 ms; zero is an exact bypass.
class StereoEnhancer {
public:
    explicit StereoEnhancer(int rate);
    bool configure(const EnhancementSettings &) noexcept;
    void process(double &left,double &right,bool stereo=true) noexcept;
    void reset() noexcept;
private:
    struct Ring {std::vector<double> data;std::size_t position=0;double lowpass=0;};
    int rate_;
    EnhancementSettings target_;
    std::array<double,5> amounts_{};
    std::array<double,2> bass_{},clarity_{};
    std::array<std::array<Ring,4>,2> rooms_;
    std::array<double,4> feedback_{};
    double ramp_,bassCoefficient_=0,clarityCoefficient_=0,attack_=0,release_=0,envelope_=0,limiter_=1;
};
}
