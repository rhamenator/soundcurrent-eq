// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <setupapi.h>
#include <shellapi.h>
#include <vector>
#include <string>

// Fixed-purpose machine configuration only. No caller-supplied paths, registry
// keys, values, commands, or DLLs are accepted by these actions.
namespace {
constexpr wchar_t volumeKey[] = L"SOFTWARE\\VB-Audio\\Cable";
constexpr wchar_t volumeValue[] = L"VBAudioCableWDM_VolCtl";
// Volatile machine marker disappears at reboot and is shared by both apps/users.
constexpr wchar_t volumePendingKey[] = L"SOFTWARE\\SoundCurrent\\VBCableVolumePending";
struct RegistryKey {
    HKEY key = nullptr;
    ~RegistryKey() { if (key) RegCloseKey(key); }
};
bool primaryCablePresent() {
    const auto devices = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    SP_DEVINFO_DATA device{}; device.cbSize = sizeof(device);
    for (DWORD i = 0; !found && SetupDiEnumDeviceInfo(devices, i, &device); ++i) {
        DWORD type = 0, bytes = 0;
        SetupDiGetDeviceRegistryPropertyW(devices, &device, SPDRP_HARDWAREID, &type, nullptr, 0, &bytes);
        if (!bytes || bytes > 65536) continue;
        std::vector<wchar_t> ids(bytes / sizeof(wchar_t) + 2, L'\0');
        if (!SetupDiGetDeviceRegistryPropertyW(devices, &device, SPDRP_HARDWAREID, &type,
                reinterpret_cast<BYTE *>(ids.data()), bytes, nullptr) || type != REG_MULTI_SZ) continue;
        for (const wchar_t *id = ids.data(); *id; id += wcslen(id) + 1)
            if (_wcsicmp(id, L"VBAudioVACWDM") == 0 || _wcsicmp(id, L"ROOT\\VBAudioVACWDM") == 0)
                found = true;
    }
    SetupDiDestroyDeviceInfoList(devices);
    return found;
}
// 0: saved, 12: absent/disabled/wrong type, 31: read failure.
int savedCableVolumeStatus() {
    RegistryKey key;
    auto result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, volumeKey, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key.key);
    if (result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) return 12;
    if (result != ERROR_SUCCESS) return 31;
    DWORD type = 0, value = 0, bytes = sizeof(value);
    result = RegQueryValueExW(key.key, volumeValue, nullptr, &type, reinterpret_cast<BYTE *>(&value), &bytes);
    if (result == ERROR_FILE_NOT_FOUND || result == ERROR_MORE_DATA) return 12;
    if (result != ERROR_SUCCESS) return 31;
    return type == REG_DWORD && bytes == sizeof(DWORD) && value == 1 ? 0 : 12;
}
int cableVolumeStatus() {
    const int saved = savedCableVolumeStatus();
    if (saved) return saved;
    RegistryKey pending;
    const auto result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, volumePendingKey, 0,
                                     KEY_QUERY_VALUE | KEY_WOW64_64KEY, &pending.key);
    if (result == ERROR_SUCCESS) return 3010;
    return result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND ? 0 : 31;
}
bool elevatedProcess() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{}; DWORD size = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)
                          && elevation.TokenIsElevated;
    CloseHandle(token);
    return elevated;
}
int configureCableVolume(bool allowElevation) {
    const int status = cableVolumeStatus();
    if (status != 12) return status; // Already saved: no elevation or write.
    if (!primaryCablePresent()) return 10;
    if (!elevatedProcess()) {
        if (!allowElevation) return ERROR_ELEVATION_REQUIRED;
        std::vector<wchar_t> self(32768);
        const auto length = GetModuleFileNameW(nullptr, self.data(), DWORD(self.size()));
        if (!length || length >= self.size()) return 31;
        SHELLEXECUTEINFOW execute{};
        execute.cbSize = sizeof(execute);
        execute.fMask = SEE_MASK_NOCLOSEPROCESS;
        execute.lpVerb = L"runas";
        execute.lpFile = self.data();
        execute.lpParameters = L"--configure-volume-elevated";
        execute.nShow = SW_HIDE;
        if (!ShellExecuteExW(&execute)) return GetLastError() == ERROR_CANCELLED ? ERROR_CANCELLED : 31;
        if (!execute.hProcess) return 31;
        DWORD code = 31;
        if (WaitForSingleObject(execute.hProcess, INFINITE) == WAIT_OBJECT_0)
            GetExitCodeProcess(execute.hProcess, &code);
        CloseHandle(execute.hProcess);
        return int(code);
    }
    // Recheck after elevation. Concurrent installs write the same fixed value;
    // key creation and the DWORD update are idempotent.
    // Do not restart the device or disturb active streams.
    if (!primaryCablePresent()) return 10;
    const int current = cableVolumeStatus();
    if (current != 12) return current;
    RegistryKey pending, key;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, volumePendingKey, 0, nullptr, REG_OPTION_VOLATILE,
            KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, nullptr, &pending.key, &disposition) != ERROR_SUCCESS) return 31;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, volumeKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
            KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key.key, nullptr) != ERROR_SUCCESS) return 31;
    const DWORD enabled = 1;
    if (RegSetValueExW(key.key, volumeValue, 0, REG_DWORD,
            reinterpret_cast<const BYTE *>(&enabled), sizeof(enabled)) != ERROR_SUCCESS) return 31;
    if (RegFlushKey(key.key) != ERROR_SUCCESS || savedCableVolumeStatus() != 0) return 31;
    return 3010; // Saved is not proof of a live driver change; require reboot.
}
} // namespace
