// SPDX-License-Identifier: GPL-3.0-only
#include "processor.h"
#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
thread_local bool realtime = false;
std::atomic<unsigned> rtMemory{0};
void *operator new(std::size_t size) { if(realtime)++rtMemory; if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc(); }
void operator delete(void *p) noexcept { if(realtime)++rtMemory;std::free(p); }
void operator delete(void *p,std::size_t) noexcept {operator delete(p);}
void *operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void *p) noexcept {::operator delete(p);}
void operator delete[](void *p,std::size_t) noexcept {::operator delete(p);}
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main(){try{
 using namespace soundcurrent;native::Processor p;
 require(!p.prepare(48000,1,64)&&!p.prepare(48000,256,64)&&!p.prepare(48000,2,0),"format/capacity validation");
 require(p.prepare(48000,2,64),"prepare");std::array<float,128> in{},out{},reference{};
 for(unsigned i=0;i<128;++i)in[i]=float(1.5*std::sin(i*.07));
 realtime=true;const bool initial=p.process(in.data(),out.data(),64,false);realtime=false;
 require(initial&&std::memcmp(in.data(),out.data(),sizeof(in))==0,"initial exact bypass including >0 dBFS");
 EqBand band{1000,6,.8};EnhancementSettings effects;effects.values[0]=.4;
 require(p.submit({&band,1},-3,20,true,true,effects),"submit");
 StereoEqualizer eq(48000);eq.setProfile({&band,1},-3,20,true,true);eq.setEnhancements(effects);
 for(unsigned block=0;block<20;++block){reference=in;eq.process(reference.data(),64);realtime=true;const bool ok=p.process(in.data(),out.data(),64,false);realtime=false;require(ok,"process");for(unsigned i=0;i<128;++i)require(out[i]==reference[i],"DSP parity");}
 require(!p.submit({&band,1},100,0,true,true,{}),"invalid profile");
 require(p.submit({},0,0,false,false,{}),"disable");realtime=true;p.process(in.data(),in.data(),64,false);realtime=false;
 require(std::memcmp(in.data(),out.data(),sizeof(in))!=0,"enable/disable exercised");
 auto original=in;realtime=true;p.process(in.data(),in.data(),64,false);realtime=false;require(in==original,"in-place bypass");
 realtime=true;p.process(nullptr,out.data(),64,true);realtime=false;for(auto sample:out)require(sample==0,"silent bypass");
 out.fill(42);require(!p.process(in.data(),out.data(),65,false)&&out[0]==42,"oversized block");
 require(!p.process(nullptr,out.data(),64,false)&&!p.process(in.data(),nullptr,64,false),"invalid buffers");
 native::Processor gainProbe; require(gainProbe.prepare(48000,2,64),"gain probe setup");require(gainProbe.submit({},6,0,true,false,{}),"gain update");in.fill(.1f);realtime=true;gainProbe.process(in.data(),out.data(),64,false);realtime=false;require(std::abs(out[0]-.199526f)<.00001f,"immediate gain");
 effects={};effects.values[1]=.8;require(p.submit({},0,0,true,false,effects),"ambience update");in.fill(0);in[0]=in[1]=.1f;realtime=true;p.process(in.data(),out.data(),64,false);realtime=false;in.fill(0);
 bool tail=false;for(unsigned b=0;b<400;++b){realtime=true;p.process(nullptr,out.data(),64,true);realtime=false;for(auto x:out)tail=tail||std::abs(x)>1e-7;}
 require(tail,"silent input retains effect tails");
 std::atomic<bool> done=false;std::thread control([&]{for(int i=0;i<100;++i)require(p.submit({},i%5,0,true,false,{}),"concurrent submit");done=true;});
 while(!done){realtime=true;p.process(in.data(),out.data(),64,false);realtime=false;}control.join();
 require(rtMemory==0,"realtime allocation/free detected");p.reset();
 std::cout<<"PASS: format limits, exact bypass, DSP parity, silent tails, immediate gain, bounded mailbox concurrency and no RT allocation/free\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
