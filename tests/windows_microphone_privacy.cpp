// SPDX-License-Identifier: GPL-3.0-only
#include "microphone_access.h"
#include "windows_audio.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
int main(){try{
 soundcurrent::microphone::permit(false);
 const auto begin=std::chrono::steady_clock::now();
 soundcurrent::WindowsBridge bridge;
 if(bridge.start(L"not-an-endpoint",L"not-an-output",true) || bridge.running() || bridge.error()!="Microphone access is disabled.")throw std::runtime_error("Denied microphone bridge reached device setup");
 soundcurrent::WindowsRecorder recorder;bool denied=false;
 try{recorder.start(L"not-an-endpoint",48000,1);}catch(const std::exception&e){denied=std::string(e.what())=="Microphone access is disabled.";}
 if(!denied || recorder.running())throw std::runtime_error("Denied recorder reached device setup");
 if(std::chrono::steady_clock::now()-begin>std::chrono::seconds(1))throw std::runtime_error("Permission rejection waited for device initialization");
 std::cout<<"PASS: Windows microphone backends reject before endpoint activation, without hardware\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
