// SPDX-License-Identifier: GPL-3.0-only
#include "microphone_access.h"
#include <iostream>
#include <stdexcept>
using soundcurrent::microphone::AccessPolicy;
void check(bool value){if(!value)throw std::runtime_error("Microphone permission regression");}
int main(){try{
 check(!soundcurrent::microphone::allowed());
 bool rejected=false;try{soundcurrent::microphone::requireAccess();}catch(...){rejected=true;}check(rejected);
 for(bool connected:{false,true}){
  AccessPolicy fresh;check(!fresh.allowed());fresh.observe(connected);check(fresh.disabled()==!connected);check(fresh.allowed()==connected);
  fresh.observe(!connected);check(fresh.disabled()==connected);check(fresh.allowed()==!connected);
 }
 for(bool disabled:{false,true}){
  AccessPolicy saved(disabled);saved.observe(true);check(saved.allowed()==!disabled);
  saved.observe(false);check(!saved.allowed());check(saved.disabled()==disabled);
  saved.observe(true);check(saved.allowed()==!disabled);
  saved.chooseDisabled(!disabled);check(saved.allowed()==disabled);
  AccessPolicy restarted(saved.disabled());restarted.observe(true);check(restarted.allowed()==disabled);
 }
 soundcurrent::microphone::permit(true);soundcurrent::microphone::requireAccess();
 soundcurrent::microphone::permit(false);check(!soundcurrent::microphone::allowed());
 std::cout<<"PASS: fail-closed startup, inverse default, sticky choices, hotplug and capture gate\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
