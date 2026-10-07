// SPDX-License-Identifier: GPL-3.0-only
#include "windows_audio.h"
#include <QCoreApplication>
#include <QTextStream>
#include <windows.h>
#include <shellapi.h>
#include <array>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() == 2 && args[1] == "--check-ready") {
        try {
            bool render = false, capture = false;
            for (const auto &d : soundcurrent::windowsAudioEndpoints(false))
                render |= d.virtualCable && d.name.rfind(L"CABLE Input (", 0) == 0;
            for (const auto &d : soundcurrent::windowsAudioEndpoints(true))
                capture |= d.virtualCable && d.name.rfind(L"CABLE Output (", 0) == 0;
            return render && capture ? 0 : 10;
        } catch (...) { return 10; }
    }
    if (args.size() != 3 || (args[1] != "--install" && args[1] != "--remove")) return 2;
    const bool install = args[1] == "--install";
    std::array<std::wstring, 3> output, input;
    try {
        for (int role = 0; role < 3; ++role) {
            // A workstation can have no active microphone before setup.
            try { output[role] = soundcurrent::windowsDefaultEndpointId(false, role); } catch (...) {}
            try { input[role] = soundcurrent::windowsDefaultEndpointId(true, role); } catch (...) {}
        }
        const auto path = args[2].toStdWString();
        SHELLEXECUTEINFOW execute{sizeof(execute)};
        execute.fMask = SEE_MASK_NOCLOSEPROCESS;
        execute.lpVerb = L"runas";
        execute.lpFile = path.c_str();
        execute.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&execute) || !execute.hProcess) return 3;
        const DWORD wait = WaitForSingleObject(execute.hProcess, INFINITE);
        DWORD code = 1;
        if (wait == WAIT_OBJECT_0) GetExitCodeProcess(execute.hProcess, &code);
        CloseHandle(execute.hProcess);
        // VB-CABLE's installer can make its new endpoints the Windows default.
        // Restore only roles still pointing to the primary cable, preserving
        // physical endpoints chosen manually while its installer was open.
        if (install) {
            for (bool capture : {false, true}) {
                auto originals = capture ? input : output;
                std::wstring fallback;
                for (const auto &id : originals) if (!id.empty()) { fallback = id; break; }
                if (fallback.empty()) continue; // No previous endpoint to restore.
                for (auto &id : originals) if (id.empty()) id = fallback;
                for (const auto &endpoint : soundcurrent::windowsAudioEndpoints(capture)) {
                    const auto prefix = capture ? L"CABLE Output (" : L"CABLE Input (";
                    if (!endpoint.virtualCable || endpoint.name.rfind(prefix, 0) != 0) continue;
                    if (!soundcurrent::windowsRestoreOwnedRoute(capture, endpoint.id, fallback, originals)) return 4;
                }
            }
        }
        return (code == 0 || code == 3010) ? 0 : 5;
    } catch (const std::exception &error) {
        QTextStream(stderr) << error.what() << Qt::endl;
        return 6;
    }
}
