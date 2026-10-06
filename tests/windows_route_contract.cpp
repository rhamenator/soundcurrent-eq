// SPDX-License-Identifier: GPL-3.0-only
#include "windows_audio.h"
#include <cstdio>
int main() {
    soundcurrent::WindowsBridge bridge;
    if (bridge.start(L"same", L"same", false, true) || bridge.running() || bridge.error().empty()) return 1;
    if (bridge.start(L"source", L"output", true, true) || bridge.running() || bridge.error().empty()) return 2;
    bridge.stop();
    std::puts("Loopback feedback and incompatible microphone routes rejected before opening devices");
    return 0;
}
