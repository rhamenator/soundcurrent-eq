// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <unknwn.h>
#include <cstdint>
// Shared backend identity; never register both applications' DLL copies.
inline constexpr GUID NativeFloatSubtype = {3,0,0x0010,{0x80,0,0,0xaa,0,0x38,0x9b,0x71}};
inline constexpr CLSID CLSID_SoundCurrentNative = {0x5e9ea3f1,0x4e50,0x4c38,{0xb7,0x88,0x9d,0x91,0x96,0x71,0x80,0xc6}};
struct NativeBand { double frequency, gainDb, q; std::uint32_t type; };
struct NativeProfile {
    std::uint32_t bytes = sizeof(NativeProfile), version = 1, count = 0;
    std::uint32_t enabled = 0, automaticHeadroom = 1;
    std::int32_t balance = 0;
    double gain = 0;
    NativeBand bands[64]{};
    double effects[16]{0,0,0,0,0,3000,90,1.2,.45,1.6,-24,3,10,180,9,-1};
};
// Local object configuration for the host harness; this is NOT cross-process IPC.
MIDL_INTERFACE("f6fc7025-ae68-4be6-b36c-c793db00bd2a") ISoundCurrentNativeControl : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE SetProfile(const NativeProfile *profile) = 0;
};
