// SPDX-License-Identifier: GPL-3.0-only
// Opt-in, isolated-VM test. Changes default routes; plays no audio.
#include "windows_managed_route.h"
#include <windows.h>
#include <tlhelp32.h>
#include <QElapsedTimer>
#include <QThread>
#include <QTextStream>
#include <cstdio>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
std::array<std::wstring, 3> defaults(bool capture) {
    std::array<std::wstring, 3> result;
    for (int role = 0; role < 3; ++role)
        result[role] = soundcurrent::windowsDefaultEndpointId(capture, role);
    return result;
}
bool waitDefaults(bool capture, const std::array<std::wstring, 3> &expected) {
    QElapsedTimer timer; timer.start();
    do {
        QCoreApplication::processEvents();
        if (defaults(capture) == expected) return true;
        QThread::msleep(50);
    } while (timer.elapsed() < 10000);
    return false;
}
void exerciseUnownedDefaults(bool capture) {
    std::wstring owned, fallback;
    for (const auto &endpoint : soundcurrent::windowsAudioEndpoints(capture)) {
        if (endpoint.virtualCable && owned.empty()) owned = endpoint.id;
        if (!endpoint.virtualCable && fallback.empty()) fallback = endpoint.id;
    }
    require(!owned.empty() && !fallback.empty(), "Need endpoint ownership fixture");
    const auto expected = defaults(capture);
    for (const auto &id : expected) require(id != owned, "Fixture defaults must be outside the owned route");
    struct Cleanup {
        bool capture; std::wstring owned,fallback;std::array<std::wstring,3> expected;
        ~Cleanup(){soundcurrent::windowsRestoreOwnedRoute(capture,owned,fallback,expected);}
    } cleanup{capture,owned,fallback,expected};
    // Conflicting proposed restoration IDs make an unconditional restore
    // observable. All actual roles are now unowned, as after a user change.
    require(soundcurrent::windowsRestoreOwnedRoute(capture,owned,fallback,
        {owned,owned,owned}), "Recovery unexpectedly failed for unowned roles");
    require(defaults(capture)==expected,"Recovery overwrote unowned Windows defaults");
    std::cout << "PASS: " << (capture ? "microphone" : "playback")
              << " unowned defaults preserved despite conflicting restore IDs\n";
}
void exerciseGuardianFailure(bool capture) {
    std::wstring owned, fallback;
    for (const auto &endpoint : soundcurrent::windowsAudioEndpoints(capture)) {
        if (endpoint.virtualCable && owned.empty()) owned = endpoint.id;
        if (!endpoint.virtualCable && fallback.empty()) fallback = endpoint.id;
    }
    require(!owned.empty() && !fallback.empty(), "Need virtual and physical endpoint fixture");
    auto expected = defaults(capture);
    for (auto &id : expected) if (id == owned) id = fallback;
    bool stoppedBeforeRestore = false;
    soundcurrent::ManagedWindowsRoute route(capture, owned, fallback, false, [&] {
        require(defaults(capture) == std::array<std::wstring,3>{owned,owned,owned},
                "Bridge stop callback occurred after route restoration");
        stoppedBeforeRestore = true;
    });
    require(route.healthy(), "Guardian not running before fault injection");
    require(defaults(capture) == std::array<std::wstring,3>{owned,owned,owned}, "Route not acquired");
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    require(snapshot != INVALID_HANDLE_VALUE, "Cannot enumerate own recovery child");
    DWORD guardianPid = 0;
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot,&entry)) do {
        if (entry.th32ParentProcessID == GetCurrentProcessId() &&
            _wcsicmp(entry.szExeFile,L"soundcurrent-route-guardian.exe") == 0) {
            guardianPid = entry.th32ProcessID; break;
        }
    } while (Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);
    require(guardianPid != 0, "Own guardian child not found");
    HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE,FALSE,guardianPid);
    require(process != nullptr, "Cannot open own guardian child");
    const bool terminated = TerminateProcess(process,99) != FALSE;
    if (terminated) WaitForSingleObject(process,5000);
    CloseHandle(process);
    require(terminated, "Could not terminate own guardian");
    // The owner stays alive: restoration must come from the finished handler,
    // rather than the route destructor or a healthy() caller resetting it.
    require(waitDefaults(capture,expected), "Guardian loss did not restore defaults while owner remained alive");
    require(!route.healthy(), "Dead guardian still reported healthy");
    require(stoppedBeforeRestore, "Guardian loss did not stop bridge before restoration");
    std::cout << "PASS: " << (capture ? "microphone" : "playback")
              << " guardian failure restored defaults while owner stayed alive\n";
}
void exercise(bool capture, bool crash) {
    std::wstring owned, fallback;
    for (const auto &endpoint : soundcurrent::windowsAudioEndpoints(capture)) {
        if (endpoint.virtualCable && owned.empty()) owned = endpoint.id;
        if (!endpoint.virtualCable && fallback.empty()) fallback = endpoint.id;
    }
    require(!owned.empty() && !fallback.empty(), "Need virtual and physical endpoints for this routing fixture");
    auto expected = defaults(capture);
    for (auto &id : expected) if (id == owned) id = fallback;
    struct Cleanup {
        bool capture; std::wstring owned, fallback;
        std::array<std::wstring, 3> expected;
        ~Cleanup() { soundcurrent::windowsRestoreOwnedRoute(capture, owned, fallback, expected); }
    } cleanup{capture, owned, fallback, expected};
    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {"--hold", capture ? "1" : "0",
        QString::fromStdWString(owned), QString::fromStdWString(fallback)});
    require(child.waitForStarted(5000), "Route owner did not start");
    require(child.waitForReadyRead(10000) && child.readAllStandardOutput().trimmed() == "ready",
        "Route owner handshake failed");
    require(defaults(capture) == std::array<std::wstring, 3>{owned, owned, owned},
        "Owner did not acquire all endpoint roles");
    if (crash) child.kill(); else child.closeWriteChannel();
    require(child.waitForFinished(10000), "Route owner did not exit");
    if (!crash) require(child.exitCode() == 0, "Normal owner exit failed");
    require(waitDefaults(capture, expected), "Default roles were not restored after owner exit");
    std::cout << "PASS: " << (capture ? "microphone" : "playback") << ' '
        << (crash ? "forced termination" : "normal exit") << " restored all three roles\n";
}
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        if (app.arguments().size()==2 && app.arguments()[1]=="--snapshot") {
            for(const auto &id:defaults(false)) QTextStream(stdout)<<QString::fromStdWString(id)<<Qt::endl;
            return 0;
        }
        if (app.arguments().size()==5 && app.arguments()[1]=="--verify-defaults") {
            const std::array<std::wstring,3> expected{app.arguments()[2].toStdWString(),app.arguments()[3].toStdWString(),app.arguments()[4].toStdWString()};
            return defaults(false)==expected?0:1;
        }
        if (app.arguments().size() == 5 && app.arguments()[1] == "--hold") {
            soundcurrent::ManagedWindowsRoute route(app.arguments()[2] == "1",
                app.arguments()[3].toStdWString(), app.arguments()[4].toStdWString());
            require(route.healthy(), "Recovery helper not alive");
            QTextStream(stdout) << "ready" << Qt::endl;
            while (std::getchar() != EOF) {}
            return 0;
        }
        if (app.arguments().size() != 2 || app.arguments()[1] != "--run") {
            std::cout << "Use --run only on an independent Windows test clone; changes audio defaults.\n";
            return 0;
        }
        for (bool capture : {false, true})
            for (bool crash : {false, true}) exercise(capture, crash);
        for (bool capture : {false, true}) exerciseGuardianFailure(capture);
        for (bool capture : {false, true}) exerciseUnownedDefaults(capture);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
