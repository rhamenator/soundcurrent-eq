// SPDX-License-Identifier: GPL-3.0-only
#ifdef _WIN32
#define NOMINMAX
#include "windows_audio.h"

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr int kBands = 15;
constexpr std::array<double, kBands> frequencies =
    {25, 40, 63, 100, 160, 250, 400, 630, 1000, 1600, 2500, 4000, 6300, 10000, 16000};
constexpr std::array<double, 9> anchors = {32, 64, 125, 250, 500, 1000, 2000, 4000, 8000};
struct Preset { const wchar_t *name; std::array<double, 9> gain; };
constexpr Preset presets[] = {
    {L"Flat", {0,0,0,0,0,0,0,0,0}},
    {L"Balanced", {1,1,.5,0,-.5,0,.5,1,1}},
    {L"Loudness", {5,5,3,1,0,-1,0,2,3}},
    {L"Bass Boost", {5,4,3,1.5,0,0,0,0,0}},
    {L"Deep Bass", {7,6,4,2,0,-1,-1,-1,-1}},
    {L"Punchy Bass", {2,3,5,4,1,-1,0,1,1}},
    {L"Bass Cut", {-6,-5,-4,-2,0,0,0,0,0}},
    {L"Clear Voice", {-3,-2,-1,0,1,2.5,3,1.5,0}},
    {L"Podcast", {-4,-3,-1,0,2,3,2.5,0,-1}},
    {L"TV Dialogue", {-4,-3,-2,0,1.5,3.5,4,1,-1}},
    {L"Vocal Focus", {-2,-1,0,1,2,3,3,1,0}},
    {L"Warm", {2.5,2,1.5,.5,0,-.5,-1,-1,-1.5}},
    {L"Bright", {-1,-1,-.5,0,.5,1,2,2.5,2.5}},
    {L"Soft Treble", {0,0,0,0,0,-.5,-1.5,-3,-4}},
    {L"Treble Detail", {-1,-1,-1,0,0,1,2.5,4,3}},
    {L"Movies", {3,2.5,1.5,0,-1,0,1,2,2}},
    {L"Gaming", {3,2,0,-2,-1,1,3,2,0}},
    {L"FPS Footsteps", {-5,-4,-3,-2,0,2,4,3,1}},
    {L"Night Listening", {-6,-5,-3,0,2,3,1,-3,-5}},
    {L"Small Speakers", {-4,-2,0,2,2,1,1,0,-1}},
    {L"Headphones", {1,1,0,-1,-1.5,0,1.5,2,1}},
    {L"Rock", {3,2,1,-1,-2,0,2,3,2}},
    {L"Pop", {2,2,1,0,1,2,2,2,1}},
    {L"Jazz", {2,1.5,1,0,-1,0,1.5,2,1}},
    {L"Classical", {1,1,0,-1,-1,0,1,2,2}},
    {L"Electronic", {4,4,3,0,-2,0,2,3,3}},
    {L"Dance", {4,4,2,0,-1,0,2,3,2}},
    {L"Hip-Hop", {5,5,3,1,-1,0,1,1,0}},
    {L"R&B", {3,3,2,1,0,1,2,1,0}},
    {L"Acoustic", {1,1,0,1,2,2,1,1,0}},
    {L"Piano", {0,0,0,1,2,2,1,0,-1}},
    {L"Metal", {4,3,1,-2,-2,1,3,2,1}},
    {L"Lo-Fi", {2,2,1,0,-1,-2,-3,-5,-6}},
    {L"Live", {2,1,0,-1,-1,1,2,2,1}},
};
constexpr int presetCount = sizeof(presets) / sizeof(presets[0]);
constexpr UINT kStatusMessage = WM_APP + 1;
constexpr UINT kTrayMessage = WM_APP + 2;
constexpr int idPreset = 100, idOutput = 101, idAuto = 102, idEnable = 103,
              idLock = 104, idUndo = 105, idGain = 106, idBalance = 107,
              idSetup = 108, idSound = 109, idQuit = 110, idMeter = 111,
              idOpen = 112, idTrayEnable = 113,
              idBandStart = 200;

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), towlower);
    return value;
}
bool cableName(const std::wstring &value) {
    const auto name = lower(value);
    return name.find(L"cable input") != std::wstring::npos ||
           name.find(L"cable output") != std::wstring::npos ||
           name.find(L"vb-audio") != std::wstring::npos;
}
double shapeAt(const Preset &preset, double frequency) {
    if (frequency <= anchors.front()) return preset.gain.front();
    if (frequency >= anchors.back()) return preset.gain.back();
    for (std::size_t i = 1; i < anchors.size(); ++i) {
        if (frequency <= anchors[i]) {
            const double portion = std::log(frequency / anchors[i - 1]) /
                                   std::log(anchors[i] / anchors[i - 1]);
            return preset.gain[i - 1] * (1 - portion) + preset.gain[i] * portion;
        }
    }
    return 0;
}

struct Snapshot {
    std::array<int, kBands> band{};
    int gain = 0, balance = 0, preset = 0;
    bool enabled = true;
};

class App {
public:
    explicit App(HWND window) : window_(window) {}
    void create();
    void layout();
    void refreshDevices();
    void onCommand(int id, int event);
    void onSlider(HWND control, int event);
    void timer();
    void undo();
    void close();
    void hide();
    void restore();
    void trayEvent(LPARAM event);
    void addTray();
    void removeTray();
    void status(const std::wstring &message);
    HWND window() const { return window_; }
private:
    HWND control(const wchar_t *className, const wchar_t *text, DWORD style, int id);
    HWND label(const wchar_t *text);
    void profile();
    void controls();
    void save();
    void load();
    void pushUndo();
    void applySnapshot(const Snapshot &value);
    Snapshot snapshot() const;
    std::wstring resolveOutput(const std::vector<soundcurrent::AudioEndpoint> &outputs,
                               const std::wstring &defaultId) const;
    HWND window_{};
    HFONT font_{};
    HWND preset_{}, output_{}, automatic_{}, enable_{}, lock_{}, undo_{},
         gain_{}, balance_{}, meter_{}, status_{}, gainText_{}, balanceText_{},
         presetLabel_{}, outputLabel_{}, meterLabel_{}, setup_{}, sound_{}, quit_{};
    std::array<HWND, kBands> sliders_{}, labels_{};
    Snapshot state_{};
    std::vector<Snapshot> history_;
    soundcurrent::WindowsBridge bridge_;
    std::vector<soundcurrent::AudioEndpoint> outputs_;
    std::wstring captureId_, cableRenderId_, activeOutputId_, lastPhysicalId_, iniPath_;
    bool locked_ = false, automaticOutput_ = true, refreshing_ = false, dragging_ = false;
    bool trayAdded_ = false;
    ULONGLONG lastRefresh_ = 0, lastStart_ = 0;
};

HWND App::control(const wchar_t *className, const wchar_t *text, DWORD style, int id) {
    HWND item = CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style,
                                0, 0, 100, 25, window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                GetModuleHandleW(nullptr), nullptr);
    SendMessageW(item, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    return item;
}
HWND App::label(const wchar_t *text) { return control(L"STATIC", text, 0, 0); }

void App::create() {
    font_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        DEFAULT_PITCH, L"Segoe UI");
    presetLabel_ = label(L"Listening preset");
    preset_ = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, idPreset);
    for (const auto &preset : presets) SendMessageW(preset_, CB_ADDSTRING, 0,
                                                      reinterpret_cast<LPARAM>(preset.name));
    SendMessageW(preset_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Custom"));
    outputLabel_ = label(L"Speakers / headphones");
    output_ = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, idOutput);
    automatic_ = control(L"BUTTON", L"Follow active output", BS_AUTOCHECKBOX | WS_TABSTOP, idAuto);
    enable_ = control(L"BUTTON", L"Equalizer on", BS_AUTOCHECKBOX | WS_TABSTOP, idEnable);
    lock_ = control(L"BUTTON", L"Lock EQ", BS_AUTOCHECKBOX | WS_TABSTOP, idLock);
    undo_ = control(L"BUTTON", L"Undo  (Ctrl+Z)", BS_PUSHBUTTON | WS_TABSTOP, idUndo);
    gainText_ = label(L"Post gain  +0.0 dB");
    gain_ = control(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, idGain);
    SendMessageW(gain_, TBM_SETRANGE, TRUE, MAKELPARAM(-120, 120));
    balanceText_ = label(L"Balance  center");
    balance_ = control(TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, idBalance);
    SendMessageW(balance_, TBM_SETRANGE, TRUE, MAKELPARAM(-100, 100));
    meterLabel_ = label(L"Output level");
    meter_ = control(PROGRESS_CLASSW, L"", PBS_SMOOTH, idMeter);
    SendMessageW(meter_, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    for (int i = 0; i < kBands; ++i) {
        sliders_[i] = control(TRACKBAR_CLASSW, L"", TBS_VERT | TBS_NOTICKS | WS_TABSTOP,
                              idBandStart + i);
        SendMessageW(sliders_[i], TBM_SETRANGE, TRUE, MAKELPARAM(-120, 120));
        std::wstring frequency;
        if (frequencies[i] < 1000) frequency = std::to_wstring(static_cast<int>(frequencies[i]));
        else {
            const int whole = static_cast<int>(frequencies[i] / 1000);
            const int tenths = static_cast<int>(frequencies[i] / 100) % 10;
            frequency = std::to_wstring(whole);
            if (tenths) frequency += L"." + std::to_wstring(tenths);
            frequency += L"k";
        }
        labels_[i] = label(frequency.c_str());
    }
    setup_ = control(L"BUTTON", L"Get signed cable", BS_PUSHBUTTON | WS_TABSTOP, idSetup);
    sound_ = control(L"BUTTON", L"Windows sound settings", BS_PUSHBUTTON | WS_TABSTOP, idSound);
    quit_ = control(L"BUTTON", L"Quit", BS_PUSHBUTTON | WS_TABSTOP, idQuit);
    status_ = label(L"Looking for Windows audio devices...");
    bridge_.setStatusCallback([window = window_](const std::wstring &message) {
        auto *copy = new std::wstring(message);
        if (!PostMessageW(window, kStatusMessage, 0, reinterpret_cast<LPARAM>(copy))) delete copy;
    });
    load();
    applySnapshot(state_);
    controls();
    layout();
    refreshDevices();
    addTray();
    SetTimer(window_, 1, 50, nullptr);
}

void App::addTray() {
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = window_;
    icon.uID = 1;
    icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    icon.uCallbackMessage = kTrayMessage;
    icon.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1));
    wcscpy_s(icon.szTip, L"SoundCurrent EQ");
    trayAdded_ = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
    if (trayAdded_) {
        icon.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &icon);
    }
}
void App::removeTray() {
    if (!trayAdded_) return;
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = window_;
    icon.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &icon);
    trayAdded_ = false;
}
void App::hide() {
    if (trayAdded_) ShowWindow(window_, SW_HIDE);
    else close();
}
void App::restore() {
    ShowWindow(window_, SW_RESTORE);
    SetForegroundWindow(window_);
}
void App::trayEvent(LPARAM event) {
    const auto action = LOWORD(event);
    if (action == WM_LBUTTONDBLCLK || action == NIN_SELECT) { restore(); return; }
    if (action != WM_RBUTTONUP && action != WM_CONTEXTMENU) return;
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, idOpen, L"Open SoundCurrent EQ");
    AppendMenuW(menu, MF_STRING | (state_.enabled ? MF_CHECKED : 0) |
                        (locked_ ? MF_GRAYED : 0), idTrayEnable, L"Equalizer on");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, idQuit, L"Quit app");
    POINT point{}; GetCursorPos(&point);
    SetForegroundWindow(window_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, window_, nullptr);
    DestroyMenu(menu);
    PostMessageW(window_, WM_NULL, 0, 0);
}

void App::layout() {
    RECT client{}; GetClientRect(window_, &client);
    const int width = client.right - client.left, height = client.bottom - client.top;
    const int margin = 20, space = 10;
    const int leftWidth = std::max(250, (width - 3 * margin) / 2);
    MoveWindow(presetLabel_, margin, 14, leftWidth, 22, TRUE);
    MoveWindow(preset_, margin, 39, leftWidth, 250, TRUE);
    MoveWindow(outputLabel_, margin + leftWidth + margin, 14, leftWidth, 22, TRUE);
    MoveWindow(output_, margin + leftWidth + margin, 39, leftWidth, 250, TRUE);
    MoveWindow(automatic_, margin, 76, 220, 26, TRUE);
    MoveWindow(enable_, margin + 230, 76, 130, 26, TRUE);
    MoveWindow(lock_, margin + 365, 76, 100, 26, TRUE);
    MoveWindow(undo_, margin + 475, 76, 140, 27, TRUE);
    MoveWindow(gainText_, margin, 118, 200, 23, TRUE);
    MoveWindow(gain_, margin + 190, 116, std::max(100, width / 2 - 220), 28, TRUE);
    MoveWindow(balanceText_, width / 2 + 20, 118, 150, 23, TRUE);
    MoveWindow(balance_, width / 2 + 166, 116, std::max(100, width / 2 - 190), 28, TRUE);
    MoveWindow(meterLabel_, margin, 158, 120, 23, TRUE);
    MoveWindow(meter_, margin + 120, 158, std::max(100, width - margin * 2 - 120), 22, TRUE);
    const int stripTop = 197;
    const int stripBottom = std::max(stripTop + 175, height - 104);
    const int bandWidth = std::max(37, (width - margin * 2) / kBands);
    for (int i = 0; i < kBands; ++i) {
        const int x = margin + i * bandWidth;
        MoveWindow(sliders_[i], x + 4, stripTop, bandWidth - 8, stripBottom - stripTop - 28, TRUE);
        MoveWindow(labels_[i], x, stripBottom - 24, bandWidth, 23, TRUE);
    }
    const int footer = height - 80;
    MoveWindow(setup_, margin, footer, 155, 29, TRUE);
    MoveWindow(sound_, margin + 165, footer, 190, 29, TRUE);
    MoveWindow(quit_, width - margin - 90, footer, 90, 29, TRUE);
    MoveWindow(status_, margin, height - 39, width - margin * 2, 28, TRUE);
    (void)space;
}

Snapshot App::snapshot() const { return state_; }
void App::pushUndo() {
    if (history_.size() == 50) history_.erase(history_.begin());
    history_.push_back(snapshot());
    controls();
}
void App::profile() {
    std::array<soundcurrent::EqBand, kBands> bands{};
    for (int i = 0; i < kBands; ++i)
        bands[i] = {frequencies[i], state_.band[i] / 10.0, 1.0};
    bridge_.setProfile(bands, state_.gain / 10.0, state_.balance, state_.enabled);
    save();
}
void App::applySnapshot(const Snapshot &value) {
    state_ = value;
    SendMessageW(preset_, CB_SETCURSEL, state_.preset, 0);
    SendMessageW(enable_, BM_SETCHECK, state_.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(gain_, TBM_SETPOS, TRUE, state_.gain);
    SendMessageW(balance_, TBM_SETPOS, TRUE, state_.balance);
    for (int i = 0; i < kBands; ++i) SendMessageW(sliders_[i], TBM_SETPOS, TRUE, state_.band[i]);
    controls();
    profile();
}
void App::controls() {
    EnableWindow(preset_, !locked_);
    EnableWindow(enable_, !locked_);
    EnableWindow(gain_, !locked_);
    EnableWindow(balance_, !locked_);
    for (auto slider : sliders_) EnableWindow(slider, !locked_);
    EnableWindow(undo_, !locked_ && !history_.empty());
    std::wstring gain = L"Post gain  " + std::wstring(state_.gain >= 0 ? L"+" : L"") +
                        std::to_wstring(state_.gain / 10) + L"." +
                        std::to_wstring(std::abs(state_.gain % 10)) + L" dB";
    SetWindowTextW(gainText_, gain.c_str());
    std::wstring balance = state_.balance == 0 ? L"Balance  center" :
        L"Balance  " + std::to_wstring(std::abs(state_.balance)) +
        (state_.balance < 0 ? L"% left" : L"% right");
    SetWindowTextW(balanceText_, balance.c_str());
}
void App::undo() {
    if (locked_ || history_.empty()) return;
    const auto previous = history_.back();
    history_.pop_back();
    applySnapshot(previous);
}

void App::onSlider(HWND item, int event) {
    if (locked_) return;
    if (event == TB_ENDTRACK) { dragging_ = false; return; }
    if (event == TB_THUMBTRACK) {
        if (!dragging_) { pushUndo(); dragging_ = true; }
    } else if (event != TB_THUMBPOSITION) pushUndo();
    const int value = static_cast<int>(SendMessageW(item, TBM_GETPOS, 0, 0));
    if (item == gain_) state_.gain = value;
    else if (item == balance_) state_.balance = value;
    else for (int i = 0; i < kBands; ++i) if (item == sliders_[i]) {
        state_.band[i] = value;
        state_.preset = presetCount;
        SendMessageW(preset_, CB_SETCURSEL, state_.preset, 0);
        break;
    }
    controls();
    profile();
}

void App::onCommand(int id, int event) {
    if (id == idOpen) { restore(); return; }
    if (id == idTrayEnable && !locked_) {
        pushUndo(); state_.enabled = !state_.enabled;
        SendMessageW(enable_, BM_SETCHECK, state_.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
        profile(); return;
    }
    if (id == idQuit) { close(); return; }
    if (id == idSetup) {
        ShellExecuteW(window_, L"open", L"https://vb-audio.com/Cable/", nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    if (id == idSound) {
        ShellExecuteW(window_, L"open", L"ms-settings:sound", nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    if (id == idUndo) { undo(); return; }
    if (id == idLock) {
        locked_ = SendMessageW(lock_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        controls(); save(); return;
    }
    if (id == idAuto) {
        automaticOutput_ = SendMessageW(automatic_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        EnableWindow(output_, !automaticOutput_);
        save(); refreshDevices(); return;
    }
    if (id == idOutput && event == CBN_SELCHANGE && !refreshing_) {
        const auto index = static_cast<std::size_t>(SendMessageW(output_, CB_GETCURSEL, 0, 0));
        if (index < outputs_.size()) { lastPhysicalId_ = outputs_[index].id; save(); refreshDevices(); }
        return;
    }
    if (locked_) return;
    if (id == idEnable) {
        pushUndo(); state_.enabled = SendMessageW(enable_, BM_GETCHECK, 0, 0) == BST_CHECKED;
        profile(); return;
    }
    if (id == idPreset && event == CBN_SELCHANGE) {
        const int index = static_cast<int>(SendMessageW(preset_, CB_GETCURSEL, 0, 0));
        if (index >= 0 && index < presetCount) {
            pushUndo(); state_.preset = index;
            for (int i = 0; i < kBands; ++i)
                state_.band[i] = static_cast<int>(std::lround(shapeAt(presets[index], frequencies[i]) * 10));
            applySnapshot(state_);
        }
    }
}

std::wstring App::resolveOutput(const std::vector<soundcurrent::AudioEndpoint> &outputs,
                                const std::wstring &defaultId) const {
    const auto exists = [&outputs](const std::wstring &id) {
        return std::any_of(outputs.begin(), outputs.end(), [&id](const auto &item) { return item.id == id; });
    };
    if (automaticOutput_ && exists(defaultId)) return defaultId;
    if (exists(lastPhysicalId_)) return lastPhysicalId_;
    return outputs.empty() ? L"" : outputs.front().id;
}

void App::refreshDevices() {
    try {
        auto allOutputs = soundcurrent::windowsAudioEndpoints(false);
        auto inputs = soundcurrent::windowsAudioEndpoints(true);
        std::wstring defaultId;
        try { defaultId = soundcurrent::windowsDefaultOutputId(); } catch (...) {}
        std::wstring capture;
        for (const auto &item : inputs) {
            const auto name = lower(item.name);
            if (name.find(L"cable output") != std::wstring::npos) { capture = item.id; break; }
        }
        if (capture.empty()) for (const auto &item : inputs)
            if (cableName(item.name)) { capture = item.id; break; }
        std::wstring cableRender;
        for (const auto &item : allOutputs)
            if (lower(item.name).find(L"cable input") != std::wstring::npos) {
                cableRender = item.id; break;
            }
        std::vector<soundcurrent::AudioEndpoint> physical;
        for (auto &item : allOutputs) if (!cableName(item.name)) physical.push_back(std::move(item));
        auto selected = resolveOutput(physical, defaultId);
        const bool defaultIsPhysical = std::any_of(physical.begin(), physical.end(),
            [&defaultId](const auto &item) { return item.id == defaultId; });
        if (automaticOutput_ && !defaultIsPhysical && !outputs_.empty())
            for (const auto &item : physical)
                if (std::none_of(outputs_.begin(), outputs_.end(), [&item](const auto &old) {
                        return old.id == item.id;
                    })) selected = item.id;
        const bool cableIsDefault = !cableRender.empty() && defaultId == cableRender;
        const bool changed = capture != captureId_ || cableRender != cableRenderId_ ||
                             selected != activeOutputId_;
        if (changed || (!cableIsDefault && bridge_.running())) bridge_.stop();
        captureId_ = capture;
        cableRenderId_ = cableRender;
        activeOutputId_ = selected;
        if (!selected.empty()) lastPhysicalId_ = selected;
        const bool listChanged = physical.size() != outputs_.size() ||
            !std::equal(physical.begin(), physical.end(), outputs_.begin(), outputs_.end(),
                        [](const auto &a, const auto &b) { return a.id == b.id && a.name == b.name; });
        refreshing_ = true;
        outputs_ = std::move(physical);
        if (listChanged) {
            SendMessageW(output_, CB_RESETCONTENT, 0, 0);
            for (const auto &item : outputs_)
                SendMessageW(output_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.name.c_str()));
        }
        for (std::size_t i = 0; i < outputs_.size(); ++i) if (outputs_[i].id == selected) {
            if (listChanged || changed) SendMessageW(output_, CB_SETCURSEL, i, 0);
            break;
        }
        refreshing_ = false;
        if (capture.empty()) { status(L"Install VB-CABLE from its official website, then restart Windows."); return; }
        if (cableRender.empty()) { status(L"CABLE Input is unavailable. Restart Windows after installing VB-CABLE."); return; }
        if (selected.empty()) { status(L"Connect speakers or headphones to start the equalizer."); return; }
        if (!cableIsDefault) {
            status(L"In Windows sound settings, select CABLE Input as the default output to use the EQ.");
            return;
        }
        if (!bridge_.running() && GetTickCount64() - lastStart_ >= 4000) {
            bridge_.stop();
            lastStart_ = GetTickCount64();
            if (bridge_.start(capture, selected)) {
                profile();
            }
        }
    } catch (const std::exception &error) {
        std::string message = error.what();
        status(L"Windows audio device error: " + std::wstring(message.begin(), message.end()));
    }
}

void App::timer() {
    const auto now = GetTickCount64();
    if (now - lastRefresh_ >= 2000) { lastRefresh_ = now; refreshDevices(); }
    const float peak = bridge_.peak();
    const int level = std::clamp(static_cast<int>(std::lround(peak * 100)), 0, 100);
    SendMessageW(meter_, PBM_SETPOS, level, 0);
    SendMessageW(meter_, PBM_SETBARCOLOR, 0, level >= 95 ? RGB(220, 55, 55)
                                              : level >= 75 ? RGB(230, 170, 35) : RGB(50, 170, 100));
}
void App::status(const std::wstring &message) { SetWindowTextW(status_, message.c_str()); }

void App::load() {
    wchar_t appData[MAX_PATH]{};
    if (!GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) return;
    const auto directory = std::wstring(appData) + L"\\SoundCurrent EQ";
    CreateDirectoryW(directory.c_str(), nullptr);
    iniPath_ = directory + L"\\settings.ini";
    const auto read = [this](const wchar_t *key, int fallback) {
        return static_cast<int>(GetPrivateProfileIntW(L"EQ", key, fallback, iniPath_.c_str()));
    };
    state_.preset = std::clamp(read(L"preset", 0), 0, presetCount);
    state_.gain = std::clamp(read(L"postGainTenths", 0), -120, 120);
    state_.balance = std::clamp(read(L"balance", 0), -100, 100);
    state_.enabled = GetPrivateProfileIntW(L"EQ", L"enabled", 1, iniPath_.c_str()) != 0;
    locked_ = GetPrivateProfileIntW(L"EQ", L"locked", 0, iniPath_.c_str()) != 0;
    automaticOutput_ = GetPrivateProfileIntW(L"EQ", L"autoOutput", 1, iniPath_.c_str()) != 0;
    wchar_t physical[512]{};
    GetPrivateProfileStringW(L"EQ", L"physicalOutputId", L"", physical, 512, iniPath_.c_str());
    lastPhysicalId_ = physical;
    for (int i = 0; i < kBands; ++i) {
        const auto key = L"band" + std::to_wstring(i);
        state_.band[i] = std::clamp(read(key.c_str(), 0), -120, 120);
    }
    SendMessageW(lock_, BM_SETCHECK, locked_ ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(automatic_, BM_SETCHECK, automaticOutput_ ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(output_, !automaticOutput_);
}
void App::save() {
    if (iniPath_.empty()) return;
    const auto integer = [this](const wchar_t *key, int value) {
        const auto text = std::to_wstring(value);
        WritePrivateProfileStringW(L"EQ", key, text.c_str(), iniPath_.c_str());
    };
    integer(L"preset", state_.preset); integer(L"postGainTenths", state_.gain);
    integer(L"balance", state_.balance); integer(L"enabled", state_.enabled);
    integer(L"locked", locked_); integer(L"autoOutput", automaticOutput_);
    WritePrivateProfileStringW(L"EQ", L"physicalOutputId", lastPhysicalId_.c_str(), iniPath_.c_str());
    for (int i = 0; i < kBands; ++i) {
        const auto key = L"band" + std::to_wstring(i);
        integer(key.c_str(), state_.band[i]);
    }
}

void App::close() {
    try {
        if (!cableRenderId_.empty() && soundcurrent::windowsDefaultOutputId() == cableRenderId_) {
            MessageBoxW(window_, L"Windows is still sending sound to CABLE Input. Set your speakers or headphones as the default output in Windows sound settings, then press Quit again.",
                        L"Restore normal sound before quitting", MB_OK | MB_ICONINFORMATION);
            ShellExecuteW(window_, L"open", L"ms-settings:sound", nullptr, nullptr, SW_SHOWNORMAL);
            return;
        }
    } catch (...) {}
    bridge_.stop();
    removeTray();
    save();
    DestroyWindow(window_);
}

LRESULT CALLBACK trackbarSubclass(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
                                   UINT_PTR, DWORD_PTR) {
    if (message == WM_MOUSEWHEEL) return 0;
    if (message == WM_KEYDOWN && wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        SendMessageW(GetParent(window), WM_COMMAND, MAKEWPARAM(idUndo, BN_CLICKED), 0);
        return 0;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto *app = reinterpret_cast<App *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_CREATE) {
        app = new App(window);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->create();
        for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            wchar_t name[32]{};
            GetClassNameW(child, name, 32);
            if (std::wstring(name) == TRACKBAR_CLASSW) SetWindowSubclass(child, trackbarSubclass, 1, 0);
        }
        return 0;
    }
    if (!app) return DefWindowProcW(window, message, wParam, lParam);
    switch (message) {
    case WM_SIZE: app->layout(); return 0;
    case WM_COMMAND: app->onCommand(LOWORD(wParam), HIWORD(wParam)); return 0;
    case WM_HSCROLL: case WM_VSCROLL:
        if (lParam) app->onSlider(reinterpret_cast<HWND>(lParam), LOWORD(wParam));
        return 0;
    case WM_TIMER: app->timer(); return 0;
    case kTrayMessage: app->trayEvent(lParam); return 0;
    case kStatusMessage: {
        std::unique_ptr<std::wstring> text(reinterpret_cast<std::wstring *>(lParam));
        app->status(*text); return 0;
    }
    case WM_CLOSE: app->hide(); return 0;
    case WM_DESTROY:
        KillTimer(window, 1);
        app->removeTray();
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        delete app;
        PostQuitMessage(0); return 0;
    case WM_GETMINMAXINFO: {
        auto *limits = reinterpret_cast<MINMAXINFO *>(lParam);
        limits->ptMinTrackSize = {740, 510}; return 0;
    }
    default: return DefWindowProcW(window, message, wParam, lParam);
    }
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\SoundCurrentEQSingleInstance");
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(L"SoundCurrentEQWindow", nullptr)) {
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(instanceMutex);
        return 0;
    }
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES | ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&common);
    WNDCLASSEXW klass{};
    klass.cbSize = sizeof(klass);
    klass.lpfnWndProc = windowProc;
    klass.hInstance = instance;
    klass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    klass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    klass.hIconSm = klass.hIcon;
    klass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    klass.lpszClassName = L"SoundCurrentEQWindow";
    RegisterClassExW(&klass);
    RECT desktop{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &desktop, 0);
    const int width = std::min(1050L, desktop.right - desktop.left);
    const int height = std::min(700L, desktop.bottom - desktop.top);
    HWND window = CreateWindowExW(0, klass.lpszClassName, L"SoundCurrent EQ",
                                  WS_OVERLAPPEDWINDOW,
                                  desktop.left + (desktop.right - desktop.left - width) / 2,
                                  desktop.top + (desktop.bottom - desktop.top - height) / 2,
                                  width, height, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, show);
    UpdateWindow(window);
    ACCEL accelerators[] = {{FCONTROL | FVIRTKEY, 'Z', idUndo}};
    HACCEL table = CreateAcceleratorTableW(accelerators, 1);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!TranslateAcceleratorW(window, table, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    DestroyAcceleratorTable(table);
    if (instanceMutex) CloseHandle(instanceMutex);
    return static_cast<int>(message.wParam);
}
#endif
