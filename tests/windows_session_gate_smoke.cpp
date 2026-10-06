// SPDX-License-Identifier: GPL-3.0-only
// Qt-free opt-in test of the exact production global audio session gate.
#include "windows_audio_session_gate.h"
#include <iostream>
#include <cwchar>
int wmain(int argc,wchar_t **argv) {
    if(argc!=3 || std::wcscmp(argv[1],L"--hold")!=0) {
        std::cout<<"Use --hold fixture-directory only in an isolated test VM.\n";return 0;
    }
    soundcurrent::WindowsAudioSessionGate gate;
    if(!gate.acquire())return 2;
    std::cout<<"ready"<<std::endl;
    Sleep(60000);
    return 0;
}
