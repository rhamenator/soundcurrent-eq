// SPDX-License-Identifier: GPL-3.0-only
#include "enhancement.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace soundcurrent {
bool EnhancementSettings::valid() const noexcept {
    for(std::size_t i=0;i<values.size();++i) if(!std::isfinite(values[i]) || values[i]<effectParameters[i].low || values[i]>effectParameters[i].high) return false;
    return true;
}
bool EnhancementSettings::active() const noexcept {for(std::size_t i=0;i<5;++i)if(values[i]>0)return true;return false;}
StereoEnhancer::StereoEnhancer(int rate):rate_(rate),ramp_(1.0/(rate*.02)) {
    if(rate<8000 || rate>384000)throw std::invalid_argument("Unsupported enhancement sample rate");
    constexpr double seconds[]{.0297,.0371,.0411,.0437};
    for(std::size_t c=0;c<2;++c)for(std::size_t i=0;i<4;++i)
        rooms_[c][i].data.assign(std::max<std::size_t>(1,std::size_t(rate*(seconds[i]+c*.0013))),0);
    configure(target_);
}
bool StereoEnhancer::configure(const EnhancementSettings &s) noexcept {
    if(!s.valid())return false;
    if(!target_.active() && s.active())reset(); // Do not resurrect a paused room tail.
    target_=s;
    bassCoefficient_=1-std::exp(-2*std::numbers::pi*s.values[BassHz]/rate_);
    clarityCoefficient_=1-std::exp(-2*std::numbers::pi*std::min(s.values[ClarityHz],rate_*.4)/rate_);
    attack_=std::exp(-1/(rate_*s.values[Attack]*.001));release_=std::exp(-1/(rate_*s.values[Release]*.001));
    for(std::size_t i=0;i<4;++i)feedback_[i]=std::pow(10.,-3.*rooms_[0][i].data.size()/(rate_*s.values[Decay]));
    return true;
}
void StereoEnhancer::process(double &l,double &r,bool stereo) noexcept {
    if(!std::isfinite(l))l=0;
    if(!std::isfinite(r))r=0;
    if(!target_.active() && std::all_of(amounts_.begin(),amounts_.end(),[](double a){return a==0;}))return;
    for(std::size_t i=0;i<5;++i)amounts_[i]+=std::clamp(target_.values[i]-amounts_[i],-ramp_,ramp_);
    double x[]{l,r};
    for(std::size_t c=0;c<2;++c){
        bass_[c]+=bassCoefficient_*(x[c]-bass_[c]);clarity_[c]+=clarityCoefficient_*(x[c]-clarity_[c]);
        if(std::abs(bass_[c])<1e-30)bass_[c]=0;
        if(std::abs(clarity_[c])<1e-30)clarity_[c]=0;
        x[c]+=bass_[c]*(std::pow(10.,amounts_[BassBoost]*9/20)-1)
            +(x[c]-clarity_[c])*(std::pow(10.,amounts_[Clarity]*6/20)-1);
        double wet=0;
        for(std::size_t i=0;i<4;++i){auto &b=rooms_[c][i];const double delayed=b.data[b.position];
            b.lowpass=target_.values[Damping]*b.lowpass+(1-target_.values[Damping])*delayed;
            b.data[b.position]=std::clamp(x[c]*.25+b.lowpass*feedback_[i],-16.,16.);
            if(++b.position==b.data.size())b.position=0;
            wet+=delayed;
            if(std::abs(b.lowpass)<1e-30)b.lowpass=0;
        }
        x[c]+=wet*amounts_[Ambience]*.35;
    }
    if(stereo){const double mid=(x[0]+x[1])*.5;const double side=(x[0]-x[1])*.5*(1+amounts_[Surround]*(target_.values[Width]-1));x[0]=mid+side;x[1]=mid-side;}
    const double peak=std::max(std::abs(x[0]),std::abs(x[1]));
    const double coefficient=peak>envelope_?attack_:release_;envelope_=coefficient*envelope_+(1-coefficient)*peak;
    if(envelope_<1e-30)envelope_=0;
    if(amounts_[DynamicBoost]>0){
        const double db=20*std::log10(std::max(1e-12,envelope_));
        const double over=std::max(0.,db-target_.values[Threshold]);
        const double reduction=over*(1-1/target_.values[Ratio]);
        const double gain=std::pow(10.,amounts_[DynamicBoost]*(target_.values[Makeup]-reduction)/20);
        x[0]*=gain;x[1]*=gain;
        const double ceiling=std::pow(10.,target_.values[Ceiling]/20);
        const double required=std::min(1.,ceiling/std::max(1e-12,std::max(std::abs(x[0]),std::abs(x[1]))));
        limiter_=required<limiter_?required:release_*limiter_+(1-release_)*required;
        x[0]*=limiter_;x[1]*=limiter_;
    }else limiter_=1;
    l=x[0];r=x[1];
}
void StereoEnhancer::reset() noexcept {
    amounts_.fill(0);bass_.fill(0);clarity_.fill(0);envelope_=0;limiter_=1;
    for(auto &channel:rooms_)for(auto &r:channel){std::fill(r.data.begin(),r.data.end(),0);r.position=0;r.lowpass=0;}
}
}
