// SPDX-License-Identifier: GPL-3.0-only
#include "enhancement.h"
#include "vendor/ladspa.h"
#include <array>
#include <new>
namespace {
struct Instance {soundcurrent::StereoEnhancer enhancer;std::array<float *,9> ports{};explicit Instance(int rate):enhancer(rate){}};
LADSPA_Handle instantiate(const LADSPA_Descriptor *,unsigned long rate) {try{return new Instance(int(rate));}catch(...){return nullptr;}}
void connect(LADSPA_Handle handle,unsigned long port,float *data){if(port<9)static_cast<Instance *>(handle)->ports[port]=data;}
void activate(LADSPA_Handle handle){static_cast<Instance *>(handle)->enhancer.reset();}
void run(LADSPA_Handle handle,unsigned long frames){auto &s=*static_cast<Instance *>(handle);soundcurrent::EnhancementSettings settings;
    for(std::size_t i=0;i<5;++i)settings.values[i]=s.ports[4+i]?*s.ports[4+i]:0;
    if(!s.enhancer.configure(settings))settings={};
    s.enhancer.configure(settings);
    for(unsigned long f=0;f<frames;++f){double l=s.ports[0]?s.ports[0][f]:0,r=s.ports[1]?s.ports[1][f]:0;s.enhancer.process(l,r);if(s.ports[2])s.ports[2][f]=float(l);if(s.ports[3])s.ports[3][f]=float(r);}}
void cleanup(LADSPA_Handle handle){delete static_cast<Instance *>(handle);}
constexpr LADSPA_PortDescriptor ports[]{LADSPA_PORT_INPUT|LADSPA_PORT_AUDIO,LADSPA_PORT_INPUT|LADSPA_PORT_AUDIO,LADSPA_PORT_OUTPUT|LADSPA_PORT_AUDIO,LADSPA_PORT_OUTPUT|LADSPA_PORT_AUDIO,
    LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL,LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL,LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL,LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL,LADSPA_PORT_INPUT|LADSPA_PORT_CONTROL};
const char *names[]{"Left input","Right input","Left output","Right output","Clarity","Ambience","Surround","Dynamic","Bass"};
constexpr LADSPA_PortRangeHint hints[]{ {},{},{},{},
    {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_DEFAULT_0,0,1},
    {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_DEFAULT_0,0,1},
    {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_DEFAULT_0,0,1},
    {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_DEFAULT_0,0,1},
    {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_DEFAULT_0,0,1}};
const LADSPA_Descriptor descriptor{410073,"soundcurrent_enhancements",LADSPA_PROPERTY_HARD_RT_CAPABLE,"SoundCurrent listening enhancements","rhamenator","GPL-3.0-only",9,ports,names,hints,nullptr,instantiate,connect,activate,run,nullptr,nullptr,nullptr,cleanup};
}
extern "C" const LADSPA_Descriptor *ladspa_descriptor(unsigned long index){return index==0?&descriptor:nullptr;}
