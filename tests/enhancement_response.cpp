// SPDX-License-Identifier: GPL-3.0-only
#include "enhancement.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <stdexcept>
std::atomic<std::size_t> allocations{0};
void *operator new(std::size_t n){++allocations;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete(void *p,std::size_t) noexcept {std::free(p);}
void require(bool yes,const char *message){if(!yes)throw std::runtime_error(message);}
double tone(soundcurrent::EffectParameter effect,double hz,double amplitude=.05) {
    soundcurrent::StereoEnhancer e(48000);soundcurrent::EnhancementSettings s;s.values[effect]=1;require(e.configure(s),"configure");double energy=0;
    const auto before=allocations.load();
    for(int i=0;i<48000;++i){double l=amplitude*std::sin(2*std::numbers::pi*hz*i/48000),r=l;e.process(l,r);if(i>24000)energy+=l*l;}
    require(allocations.load()==before,"processing allocated");return std::sqrt(energy/23999);
}
int main(){try{
    using namespace soundcurrent;
    StereoEnhancer dry(48000);double l=.123,r=-.456;dry.process(l,r);require(l==.123 && r==-.456,"zero must bypass exactly");
    EnhancementSettings bad;bad.values[Clarity]=std::numeric_limits<double>::quiet_NaN();require(!dry.configure(bad),"NaN accepted");bad={};bad.values[Ratio]=11;require(!dry.configure(bad),"range accepted");
    const auto bassLow=tone(BassBoost,40),bassHigh=tone(BassBoost,10000);require(bassLow>bassHigh*2,"bass not selective");
    const auto detailLow=tone(Clarity,100),detailHigh=tone(Clarity,12000);require(detailHigh>detailLow*1.5,"clarity not selective");
    StereoEnhancer width(48000);EnhancementSettings s;s.values[Surround]=1;width.configure(s);
    for(int i=0;i<2000;++i){l=.2;r=-.2;width.process(l,r);}require(l>.3 && std::abs(l+r)<1e-9,"side width failed");
    l=.2;r=.2;width.process(l,r);require(std::abs(l-.2)<1e-9 && l==r,"mono sum changed");
    l=.2;r=-.2;width.process(l,r,false);require(l==.2 && r==-.2,"mono widened");
    StereoEnhancer room(48000);s={};s.values[Ambience]=1;room.configure(s);
    for(int i=0;i<1000;++i){l=r=0;room.process(l,r);}double tail=0;l=r=.2;room.process(l,r);
    for(int i=0;i<24000;++i){l=r=0;room.process(l,r);tail+=l*l+r*r;}require(tail>1e-5,"ambience has no tail");
    room.reset();l=r=0;room.process(l,r);require(l==0 && r==0,"reset retained tail");
    require(tone(DynamicBoost,1000,.02)>.02/std::sqrt(2)*2,"dynamic quiet boost absent");
    StereoEnhancer limiter(48000);s={};s.values[DynamicBoost]=1;limiter.configure(s);
    for(int i=0;i<24000;++i){l=r=4;limiter.process(l,r);require(std::isfinite(l) && std::abs(l)<=std::pow(10.,-.05)+1e-9,"dynamic ceiling failed");}
    s.values[Ceiling]=-6;limiter.configure(s);l=r=4;limiter.process(l,r);require(std::abs(l)<=.502,"ceiling update not immediate");
    StereoEnhancer a(48000),b(48000);s={};for(int i=0;i<5;++i)s.values[i]=.5;a.configure(s);b.configure(s);
    for(int i=0;i<48000;++i){l=.1*std::sin(i*.17);r=.08*std::cos(i*.23);double x=l,y=r;a.process(l,r);b.process(x,y);require(l==x && r==y,"determinism failed");}
    s={};a.configure(s);for(int i=0;i<2000;++i){l=r=0;a.process(l,r);}l=.21;r=-.12;a.process(l,r);require(l==.21 && r==-.12,"disabled amount did not settle to bypass");
    std::cout<<"PASS: five audible/selective effects, mono/width, ambience tails, dynamic ceiling/update, bounds, exact zero bypass, reset and allocation-free processing\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
