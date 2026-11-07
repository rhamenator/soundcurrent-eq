// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <atomic>
#include <optional>
#include <stdexcept>
namespace soundcurrent::microphone {
// Preference and hardware availability are deliberately separate. A hotplug
// event must never rewrite a user's saved choice or grant access without a mic.
class AccessPolicy {
public:
 explicit AccessPolicy(std::optional<bool> savedDisabled={}):disabled_(savedDisabled.value_or(true)),initialized_(savedDisabled.has_value()),manual_(savedDisabled.has_value()){}
 void observe(bool connected){connected_=connected;if(!manual_)disabled_=!connected;initialized_=true;}
 void chooseDisabled(bool disabled){disabled_=disabled;initialized_=true;manual_=true;}
 bool allowed()const{return initialized_&&connected_&&!disabled_;}
 bool disabled()const{return disabled_;}
 bool connected()const{return connected_;}
private:bool disabled_=true,initialized_=false,connected_=false,manual_=false;
};
// Closed until metadata discovery and the saved preference have been applied.
// Capture backends check this before opening and while processing microphone
// streams. Playback loopback is intentionally independent of microphone access.
inline std::atomic_bool captureAllowed{false};
inline bool allowed(){return captureAllowed.load(std::memory_order_acquire);}
inline void permit(bool value){captureAllowed.store(value,std::memory_order_release);}
inline void requireAccess(){if(!allowed())throw std::runtime_error("Microphone access is disabled.");}
}
