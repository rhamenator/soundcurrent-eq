// SPDX-License-Identifier: GPL-3.0-only
// Qt adapters included inside main.cpp's anonymous namespace after the shared
// device and filter types. WASAPI and Windows COM stay in windows_audio/io.cpp.
std::vector<soundcurrent::AudioEndpoint> cableEndpoints(bool capture) {
    auto all = soundcurrent::windowsAudioEndpoints(capture);
    std::erase_if(all, [](const auto &d) { return !d.virtualCable; });
    return all;
}
QString standardCable(bool capture) {
    for (const auto &d : cableEndpoints(capture)) {
        const auto name = QString::fromStdWString(d.name);
        if (name.startsWith(capture ? "CABLE Output (" : "CABLE Input (", Qt::CaseInsensitive))
            return QString::fromStdWString(d.id);
    }
    throw std::runtime_error("Install VB-CABLE using Audio driver setup, then refresh devices.");
}
QString defaultSink() {
    const auto id = QString::fromStdWString(soundcurrent::windowsDefaultOutputId());
    try { if (id == standardCable(false)) return kSink; } catch (...) {}
    return id;
}
QString defaultSource() { return QString::fromStdWString(soundcurrent::windowsDefaultInputId()); }
QList<Device> devices() {
    QList<Device> result;
    const auto current = soundcurrent::windowsDefaultOutputId();
    for (const auto &d : soundcurrent::windowsAudioEndpoints(false)) {
        if (d.virtualCable || d.channels < 2) continue;
        result.append({QString::fromStdWString(d.id), QString::fromStdWString(d.name), -1,
                       d.id == current ? 100 : 0});
    }
    return result;
}
QList<InputDevice> inputDevices() {
    QList<InputDevice> result;
    const auto current = soundcurrent::windowsDefaultInputId();
    for (const auto &d : soundcurrent::windowsAudioEndpoints(true)) {
        if (d.virtualCable || d.channels < 1 || d.channels > 2) continue;
        InputDevice input;
        input.name = QString::fromStdWString(d.id);
        input.description = QString::fromStdWString(d.name);
        input.channels = static_cast<int>(d.channels);
        input.priority = d.id == current ? 100 : 0;
        result.append(input);
    }
    return result;
}
std::vector<soundcurrent::EqBand> nativeBands(const Bands &bands) {
    std::vector<soundcurrent::EqBand> result;
    result.reserve(bands.size());
    for (const auto &b : bands) result.push_back({b.frequency, b.gain, b.q, b.type});
    return result;
}
soundcurrent::WindowsBridge *playbackMeterSource = nullptr;
class AudioEngine {
public:
    void setEnhancements(const soundcurrent::EnhancementSettings &s) { effects_=s; }
    bool active() const { return bridge_.running(); }
    QString target() const { return target_.name; }
    bool smart() const { return false; }
    bool legacyVolumeManaged() const { return false; }
    void start(const Device &device, const Bands &bands, double gain = 0, int balance = 0) {
        stop();
        const auto available = devices();
        if (std::none_of(available.begin(), available.end(), [&](const Device &d) { return d.name == device.name; }))
            throw std::runtime_error("Selected speakers are disconnected");
        const auto capture = standardCable(true), render = standardCable(false);
        update(bands, gain, balance);
        try {
            route_ = std::make_unique<soundcurrent::WindowsRouteLease>(false, render.toStdWString(),
                                                                       device.name.toStdWString(), true);
            if (!bridge_.start(capture.toStdWString(), device.name.toStdWString()))
                throw std::runtime_error(bridge_.error().empty() ? "Audio bridge did not start" : bridge_.error());
            target_ = device;
            playbackMeterSource = &bridge_;
        } catch (...) { stop(); throw; }
    }
    void update(const Bands &bands, double gain = 0, int balance = 0) {
        auto filters = nativeBands(bands);
        if (!bridge_.setProfile(filters, gain, balance, true, true, effects_))
            throw std::runtime_error("Invalid equalizer settings");
        bands_ = bands;
    }
    void updateGain(double gain, int balance) { update(bands_, gain, balance); }
    void stop() {
        if (playbackMeterSource == &bridge_) playbackMeterSource = nullptr;
        bridge_.stop();
        route_.reset();
        target_ = {};
    }
    ~AudioEngine() { stop(); }
private:
    soundcurrent::WindowsBridge bridge_;
    std::unique_ptr<soundcurrent::WindowsRouteLease> route_;
    Device target_;
    Bands bands_;
    soundcurrent::EnhancementSettings effects_;
};

struct MicCable { QString render, capture, description; };
QList<MicCable> microphoneCables() {
    QList<MicCable> result;
    const auto renders = cableEndpoints(false), captures = cableEndpoints(true);
    QString primary;
    try { primary = standardCable(false); } catch (...) {}
    for (const auto &render : renders) {
        const auto id = QString::fromStdWString(render.id);
        if (id == primary) continue;
        auto expected = QString::fromStdWString(render.name);
        expected.replace("Input", "Output", Qt::CaseInsensitive);
        for (const auto &capture : captures)
            if (expected.compare(QString::fromStdWString(capture.name), Qt::CaseInsensitive) == 0) {
                result.append({id, QString::fromStdWString(capture.id), QString::fromStdWString(render.name)});
                break;
            }
    }
    return result;
}
class MicrophoneEngine {
public:
    bool active() const { return bridge_.running(); }
    QString target() const { return target_.name; }
    void setCable(const QString &id) { if (requested_ != id) { stop(); requested_ = id; } }
    void start(const InputDevice &device, const MicTuning &adjustments, double gain) {
        stop();
        const auto cables = microphoneCables();
        MicCable selected;
        if (requested_.isEmpty() && !cables.isEmpty()) selected = cables.first();
        else for (const auto &cable : cables) if (cable.render == requested_) selected = cable;
        if (selected.render.isEmpty())
            throw std::runtime_error("Install a separate second cable (for example VB-CABLE A/B) and select it here. Playback uses the bundled standard cable.");
        update(adjustments, gain);
        if (!bridge_.start(device.name.toStdWString(), selected.render.toStdWString(), true))
            throw std::runtime_error(bridge_.error().empty() ? "Microphone audio bridge did not start" : bridge_.error());
        try {
            route_ = std::make_unique<soundcurrent::WindowsRouteLease>(true, selected.capture.toStdWString(),
                                                                       device.name.toStdWString());
            target_ = device;
        } catch (...) { stop(); throw; }
    }
    void update(const MicTuning &adjustments, double gain) {
        using T = soundcurrent::FilterType;
        Bands bands{{80, 0, 0.707, T::HighPass}};
        constexpr std::array<T, 4> types{T::LowShelf, T::Peaking, T::Peaking, T::HighShelf};
        for (int i = 0; i < 4; ++i)
            bands.append({double(kMicFrequencies[i]), (adjustments.correction.isEmpty() ? kNaturalMic[i] : 0.0) + adjustments[i], 0.8, types[i]});
        bands.append(adjustments.correction);
        if (!bridge_.setProfile(nativeBands(bands), gain, 0, true, false))
            throw std::runtime_error("Invalid microphone tuning");
    }
    void stop() { bridge_.stop(); route_.reset(); target_ = {}; }
    ~MicrophoneEngine() { stop(); }
private:
    soundcurrent::WindowsBridge bridge_;
    std::unique_ptr<soundcurrent::WindowsRouteLease> route_;
    InputDevice target_;
    QString requested_;
};
