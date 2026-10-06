// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
namespace soundcurrent {
// Shared across Windows users/sessions and held for the entire app/setup run.
// Clients request only synchronize/modify rights, matching the common DACL.
class WindowsAudioSessionGate {
    HANDLE handle_ = nullptr;
    bool held_ = false;
public:
    WindowsAudioSessionGate() = default;
    WindowsAudioSessionGate(const WindowsAudioSessionGate&) = delete;
    WindowsAudioSessionGate& operator=(const WindowsAudioSessionGate&) = delete;
    ~WindowsAudioSessionGate() {
        if (held_) ReleaseMutex(handle_);
        if (handle_) CloseHandle(handle_);
    }
    bool acquire() {
        if (handle_) return false;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;0x00100001;;;WD)(A;;GA;;;SY)(A;;GA;;;BA)",
            SDDL_REVISION_1,&descriptor,nullptr)) return false;
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),descriptor,FALSE};
        handle_ = CreateMutexExW(&security,L"Global\\SoundCurrent.AudioSession.v1",0,
                                 SYNCHRONIZE | MUTEX_MODIFY_STATE);
        LocalFree(descriptor);
        if (!handle_) return false;
        const DWORD result = WaitForSingleObject(handle_,0);
        held_ = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
        return held_;
    }
};
} // namespace soundcurrent
