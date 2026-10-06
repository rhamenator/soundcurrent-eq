// SPDX-License-Identifier: GPL-3.0-only
#define NOMINMAX
#include "windows_audio.h"
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cwchar>
int main() {
    int count = 0;
    wchar_t **args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!args || count != 9) { if(args) LocalFree(args); return 2; }
    for (int i = 2; i < count; ++i)
        if (!*args[i] || std::wcslen(args[i]) > 4096) { LocalFree(args); return 2; }
    if ((std::wcscmp(args[1],L"0") && std::wcscmp(args[1],L"1")) ||
        (std::wcscmp(args[8],L"0") && std::wcscmp(args[8],L"1"))) { LocalFree(args); return 2; }
    const bool capture = !std::wcscmp(args[1],L"1");
    const bool volume = !std::wcscmp(args[8],L"1");
    const std::wstring owned=args[2], fallback=args[3];
    const std::array<std::wstring,3> originals{args[4],args[5],args[6]};
    // Versioned protocol argument, so old helpers cannot silently accept a new layout.
    if (std::wcscmp(args[7],L"1")) { LocalFree(args); return 2; }
    LocalFree(args);
    std::fputs("ready\n",stdout); std::fflush(stdout);
    // QProcess owns the write end. EOF also occurs when Windows closes handles
    // after abnormal process termination. No disk journal or elevated service.
    while (std::getchar()!=EOF) {}
    return soundcurrent::windowsRestoreOwnedRoute(capture,owned,fallback,originals,volume) ? 0 : 1;
}
