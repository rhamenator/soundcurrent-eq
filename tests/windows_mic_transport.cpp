// SPDX-License-Identifier: GPL-3.0-or-later
#include "../native/windows/virtual-driver/vendor/sysvad/soundcurrent_mic_transport.h"
#include <array>
#include <atomic>
#include <iostream>
#include <thread>
#include <cstdlib>
using soundcurrent_driver::MicTransport;
static void require(bool ok) { if (!ok) std::abort(); }
int main() {
    MicTransport q;
    std::array<short, MicTransport::capacityFrames * 2 + 2> input{}, output{};
    for (unsigned i=0;i<input.size();++i) input[i]=static_cast<short>(i+1);
    output.fill(123);
    require(q.read(output.data(), 4)==0);
    for(unsigned i=0;i<8;++i) require(output[i]==0);
    require(q.write(input.data(), 4)==0);
    q.startProducer(); q.startConsumer();
    require(q.write(input.data(), MicTransport::capacityFrames+1)==MicTransport::capacityFrames);
    require(q.write(input.data(), 1)==0);
    require(q.read(output.data(), MicTransport::capacityFrames+1)==MicTransport::capacityFrames);
    for(unsigned i=0;i<MicTransport::capacityFrames*2;++i) require(input[i]==output[i]);
    require(output[MicTransport::capacityFrames*2]==0);
    require(output[MicTransport::capacityFrames*2+1]==0);
    require(q.write(input.data(), 7)==7);
    q.startConsumer(); // newly opened recorder discards queued old audio
    require(q.read(output.data(),7)==0);
    require(q.write(input.data(),7)==7);
    q.stopProducer(); output.fill(123);
    require(q.read(output.data(),7)==0);
    for(unsigned i=0;i<14;++i) require(output[i]==0);
    q.startProducer();
    require(q.read(output.data(),1)==0); // acknowledge new epoch
    require(q.write(input.data(),7)==7);
    require(q.read(output.data(),7)==7);
    for(unsigned i=0;i<14;++i) require(output[i]==input[i]);
    // Real concurrent producer/consumer, repeatedly crossing the ring boundary.
    q.startConsumer();
    constexpr unsigned total=200000;
    std::thread producer([&] {
        for(unsigned i=0;i<total;) {
            short frame[2]={static_cast<short>(i%30000+1),static_cast<short>(-(int(i%30000)+1))};
            if(q.write(frame,1)) ++i; else std::this_thread::yield();
        }
    });
    for(unsigned i=0;i<total;) {
        short frame[2]={123,123};
        if(q.read(frame,1)) {
            require(frame[0]==static_cast<short>(i%30000+1));
            require(frame[1]==static_cast<short>(-(int(i%30000)+1))); ++i;
        } else { require(frame[0]==0 && frame[1]==0); std::this_thread::yield(); }
    }
    producer.join();
    std::cout << "Microphone transport buffering, silence, restart and concurrency passed\n";
}
