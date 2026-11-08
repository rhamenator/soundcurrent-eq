// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <setupapi.h>
#include <shellapi.h>
#include <array>
#include <vector>
#include <string>

// Fixed-purpose machine configuration only. No caller-supplied paths, registry
// keys, values, commands, or DLLs are accepted by these actions.
namespace {
constexpr wchar_t volumeKey[] = L"SOFTWARE\\VB-Audio\\Cable";
constexpr wchar_t volumeValue[] = L"VBAudioCableWDM_VolCtl";
// Volatile machine marker disappears at reboot and is shared by both apps/users.
constexpr wchar_t volumePendingKey[] = L"SOFTWARE\\SoundCurrent\\VBCableVolumePending";
// Observed in VB-Audio's signed 3.3.1.7 control panel and used only for the
// primary VBAudioVACWDM control interface. The fixed IOCTL below is the action
// behind its "Enable Windows Volume Control" menu item.
constexpr GUID cableControlInterface{0xd657ec7b, 0xe6cc, 0x48c6,
                                     {0x9a, 0x35, 0xc0, 0xf9, 0x07, 0xe5, 0xda, 0xb9}};
constexpr DWORD cableReadStateIoctl = 0x222024;
constexpr DWORD cableSetVolumeIoctl = 0x222058;
constexpr std::size_t cableStateBytes = 0x1a0;
constexpr std::size_t cableVolumeFlagsOffset = 0x174;
struct RegistryKey {
    HKEY key = nullptr;
    ~RegistryKey() { if (key) RegCloseKey(key); }
};
struct DeviceInformation {
    HDEVINFO value = INVALID_HANDLE_VALUE;
    ~DeviceInformation() { if (value != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(value); }
};
struct NativeHandle {
    HANDLE value = INVALID_HANDLE_VALUE;
    NativeHandle() = default;
    NativeHandle(const NativeHandle &) = delete;
    NativeHandle &operator=(const NativeHandle &) = delete;
    NativeHandle(NativeHandle &&other) noexcept : value(other.value) { other.value = INVALID_HANDLE_VALUE; }
    NativeHandle &operator=(NativeHandle &&other) noexcept {
        if (this != &other) {
            if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value);
            value = other.value;
            other.value = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    ~NativeHandle() { if (value != INVALID_HANDLE_VALUE && value != nullptr) CloseHandle(value); }
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
NativeHandle openPrimaryCableControl() {
    NativeHandle result;
    DeviceInformation devices;
    devices.value = SetupDiGetClassDevsW(&cableControlInterface, nullptr, nullptr,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devices.value == INVALID_HANDLE_VALUE) return result;
    SP_DEVICE_INTERFACE_DATA interfaceData{};
    interfaceData.cbSize = sizeof(interfaceData);
    if (!SetupDiEnumDeviceInterfaces(devices.value, nullptr, &cableControlInterface, 0, &interfaceData))
        return result;
    DWORD required = 0;
    SetupDiGetDeviceInterfaceDetailW(devices.value, &interfaceData, nullptr, 0, &required, nullptr);
    if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || required > 65536) return result;
    std::vector<BYTE> storage(required);
    auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(storage.data());
    detail->cbSize = sizeof(*detail);
    if (!SetupDiGetDeviceInterfaceDetailW(devices.value, &interfaceData, detail, required, nullptr, nullptr))
        return result;
    // Match VB-Audio's signed control panel exactly. Its control interface is
    // opened with the 0x800 device attribute rather than FILE_ATTRIBUTE_NORMAL.
    constexpr DWORD cableControlOpenFlags = 0x800;
    result.value = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, cableControlOpenFlags, nullptr);
    return result;
}
int liveCableVolumeStatus() {
    auto device = openPrimaryCableControl();
    if (device.value == INVALID_HANDLE_VALUE) return 31;
    std::array<BYTE, cableStateBytes> state{};
    DWORD returned = 0;
    if (!DeviceIoControl(device.value, cableReadStateIoctl, nullptr, 0, state.data(), DWORD(state.size()),
                         &returned, nullptr)) return 31;
    if (returned <= cableVolumeFlagsOffset) return 31;
    return (state[cableVolumeFlagsOffset] & 0x02) != 0 ? 0 : 12;
}
int applyLiveCableVolume() {
    auto device = openPrimaryCableControl();
    if (device.value == INVALID_HANDLE_VALUE) return 31;
    DWORD enabled = 1;
    DWORD driverResult = DWORD(-1), returned = 0;
    if (!DeviceIoControl(device.value, cableSetVolumeIoctl,
                         &enabled, sizeof(enabled),
                         &driverResult, sizeof(driverResult), &returned, nullptr)) return 31;
    return liveCableVolumeStatus();
}
int cableVolumeStatus() {
    const int saved = savedCableVolumeStatus();
    if (saved) return saved;
    const int live = liveCableVolumeStatus();
    if (live == 0) return 0;
    RegistryKey pending;
    const auto result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, volumePendingKey, 0,
                                     KEY_QUERY_VALUE | KEY_WOW64_64KEY, &pending.key);
    if (result == ERROR_SUCCESS) return 3010;
    return result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND ? live : 31;
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
    if (!primaryCablePresent()) return 10;
    const int saved = savedCableVolumeStatus();
    if (saved != 0 && saved != 12) return saved;
    const int live = liveCableVolumeStatus();
    if (live == 0 && saved == 0) return 0;
    // VB-Audio restricts its control interface to an elevated process. A
    // persisted value alone is not proof that the running driver was updated.
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
    const int current = savedCableVolumeStatus();
    if (current != 0 && current != 12) return current;
    if (current == 12) {
        RegistryKey key;
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, volumeKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key.key, nullptr) != ERROR_SUCCESS) return 31;
        const DWORD enabled = 1;
        if (RegSetValueExW(key.key, volumeValue, 0, REG_DWORD,
                reinterpret_cast<const BYTE *>(&enabled), sizeof(enabled)) != ERROR_SUCCESS) return 31;
        if (RegFlushKey(key.key) != ERROR_SUCCESS || savedCableVolumeStatus() != 0) return 31;
    }
    // Registry persistence alone does not change a running VB-CABLE driver.
    // Apply and verify its fixed control IOCTL; fall back to a reboot boundary
    // only when a newly installed driver cannot yet expose the control handle.
    if (applyLiveCableVolume() == 0) return 0;
    RegistryKey pending;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, volumePendingKey, 0, nullptr, REG_OPTION_VOLATILE,
            KEY_READ | KEY_WRITE | KEY_WOW64_64KEY, nullptr, &pending.key, &disposition) != ERROR_SUCCESS) return 31;
    return 3010;
}
} // namespace
