// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string_view>
namespace soundcurrent {
enum class ManagedEndpointKind { Other, Playback, MicrophoneFeed, MicrophoneCapture };
inline ManagedEndpointKind managedEndpointKind(std::wstring_view interfaceName) {
    if (interfaceName == L"SoundCurrent Audio") return ManagedEndpointKind::Playback;
    if (interfaceName == L"SoundCurrent Microphone Feed") return ManagedEndpointKind::MicrophoneFeed;
    if (interfaceName == L"SoundCurrent Microphone") return ManagedEndpointKind::MicrophoneCapture;
    return ManagedEndpointKind::Other;
}
} // namespace soundcurrent
