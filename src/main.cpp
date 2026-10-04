#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSaveFile>
#include <QSlider>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <csignal>
#include <sys/prctl.h>

namespace {

constexpr auto kSink = "soundcurrent_eq";
constexpr auto kOutput = "soundcurrent_eq_output";
constexpr std::array<int, 9> kFrequencies = {32, 64, 125, 250, 500, 1000, 2000, 4000, 8000};
using Gains = std::array<double, 9>;

struct Device {
    QString name;
    QString description;
    int index = -1;
    int priority = 0;
};

QString command(const QString &program, const QStringList &arguments, int timeout = 5000) {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(timeout) || !process.waitForFinished(timeout) ||
        process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const auto error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        throw std::runtime_error((error.isEmpty() ? QStringLiteral("Could not run %1").arg(program) : error).toStdString());
    }
    return QString::fromUtf8(process.readAllStandardOutput());
}

QJsonArray pactlList(const QString &kind) {
    const auto document = QJsonDocument::fromJson(command("pactl", {"--format=json", "list", kind}).toUtf8());
    if (!document.isArray()) throw std::runtime_error("Invalid response from pactl");
    return document.array();
}

QString defaultSink() { return command("pactl", {"get-default-sink"}).trimmed(); }

QList<Device> devices() {
    QList<Device> result;
    for (const auto &item : pactlList("sinks")) {
        const auto sink = item.toObject();
        const auto properties = sink.value("properties").toObject();
        const auto name = sink.value("name").toString();
        if (name == kSink || properties.value("node.virtual").toVariant().toString() == "true") continue;
        Device device;
        device.name = name;
        device.description = sink.value("description").toString(name);
        device.index = sink.value("index").toInt(-1);
        device.priority = properties.value("priority.session").toVariant().toString().toInt();
        result.append(device);
    }
    return result;
}

void moveStreams(int fromIndex, const QString &toName) {
    for (const auto &item : pactlList("sink-inputs")) {
        const auto stream = item.toObject();
        if (stream.value("sink").toInt(-1) != fromIndex ||
            stream.value("properties").toObject().value("node.name").toString() == kOutput) continue;
        try {
            command("pactl", {"move-sink-input", QString::number(stream.value("index").toInt()), toName});
        } catch (const std::exception &) {
            // An application may close its stream while devices are being switched.
        }
    }
}

double headroom(const Gains &gains) {
    const auto peak = std::max(0.0, *std::max_element(gains.begin(), gains.end()));
    return peak > 0 ? -std::min(18.0, peak + 1.0) : 0.0;
}

QString quote(const QString &value) {
    const auto encoded = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(encoded.mid(1, encoded.size() - 2));
}

QString filterConfig(const QString &target, const Gains &gains) {
    QStringList nodes;
    QStringList links;
    nodes << QString("{ type = builtin name = preamp label = linear control = { \"Mult\" = %1 \"Add\" = 0.0 } }")
                 .arg(QString::number(std::pow(10.0, headroom(gains) / 20.0), 'f', 8));
    links << "{ output = \"preamp:Out\" input = \"band_1:In\" }";
    for (size_t i = 0; i < gains.size(); ++i) {
        const auto type = i == 0 ? "bq_lowshelf" : i == 8 ? "bq_highshelf" : "bq_peaking";
        nodes << QString("{ type = builtin name = band_%1 label = %2 control = { \"Freq\" = %3 \"Q\" = 1.0 \"Gain\" = %4 } }")
                     .arg(i + 1).arg(type).arg(kFrequencies[i]).arg(gains[i], 0, 'f', 2);
        if (i > 0) links << QString("{ output = \"band_%1:Out\" input = \"band_%2:In\" }").arg(i).arg(i + 1);
    }
    return QString(R"(
context.spa-libs = {
  audio.convert.* = audioconvert/libspa-audioconvert
  support.* = support/libspa-support
}
context.modules = [
  { name = libpipewire-module-protocol-native }
  { name = libpipewire-module-client-node }
  { name = libpipewire-module-adapter }
  { name = libpipewire-module-filter-chain
    args = {
      node.description = "SoundCurrent EQ"
      media.name = "SoundCurrent EQ"
      audio.channels = 2
      audio.position = [ FL FR ]
      filter.graph = {
        nodes = [ %1 ]
        links = [ %2 ]
      }
      capture.props = { node.name = "%3" media.class = Audio/Sink }
      playback.props = {
        node.name = "%4"
        target.object = %5
        node.passive = true
      }
    }
  }
]
)").arg(nodes.join('\n'), links.join('\n'), kSink, kOutput, quote(target));
}

int nodeId(const QString &name) {
    const auto document = QJsonDocument::fromJson(command("pw-dump", {}, 8000).toUtf8());
    for (const auto &item : document.array()) {
        const auto node = item.toObject();
        if (node.value("info").toObject().value("props").toObject().value("node.name").toString() == name)
            return node.value("id").toInt(-1);
    }
    return -1;
}

class AudioEngine {
public:
    bool active() const { return process_.state() != QProcess::NotRunning; }
    QString target() const { return target_.name; }

    void start(const Device &device, const Gains &gains) {
        stop();
        bool found = false;
        for (const auto &available : devices()) if (available.name == device.name) found = true;
        if (!found) throw std::runtime_error("Selected output device is no longer available");
        if (nodeId(kSink) >= 0) throw std::runtime_error("Another SoundCurrent EQ sink is already running");
        if (!directory_.isValid()) throw std::runtime_error("Could not create temporary audio configuration");
        const auto path = directory_.filePath("filter.conf");
        QFile config(path);
        if (!config.open(QIODevice::WriteOnly | QIODevice::Truncate))
            throw std::runtime_error("Could not write temporary audio configuration");
        config.write(filterConfig(device.name, gains).toUtf8());
        config.close();
        process_.setProgram("pipewire");
        process_.setArguments({"-c", path});
        process_.setProcessChannelMode(QProcess::MergedChannels);
        process_.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGTERM); });
        process_.start();
        if (!process_.waitForStarted(2000)) throw std::runtime_error("Could not start PipeWire filter");
        QElapsedTimer clock;
        clock.start();
        while (clock.elapsed() < 3000) {
            if (process_.state() == QProcess::NotRunning)
                throw std::runtime_error(QString::fromUtf8(process_.readAll()).trimmed().toStdString());
            try { if (nodeId(kSink) >= 0) break; } catch (const std::exception &) {}
            QThread::msleep(100);
        }
        if (nodeId(kSink) < 0) {
            const auto details = QString::fromUtf8(process_.readAll()).trimmed();
            stop();
            throw std::runtime_error(("Timed out waiting for the equalizer sink: " + details).toStdString());
        }
        try {
            command("pactl", {"set-default-sink", kSink});
            moveStreams(device.index, kSink);
            target_ = device;
        } catch (const std::exception &) {
            stop();
            throw;
        }
    }

    void update(const Gains &gains) {
        if (!active()) return;
        const auto id = nodeId(kSink);
        if (id < 0) throw std::runtime_error("Equalizer sink disappeared");
        QStringList controls = {quote("preamp:Mult"), QString::number(std::pow(10.0, headroom(gains) / 20.0), 'f', 8)};
        for (size_t i = 0; i < gains.size(); ++i) {
            controls << quote(QString("band_%1:Gain").arg(i + 1)) << QString::number(gains[i], 'f', 2);
        }
        command("pw-cli", {"set-param", QString::number(id), "Props",
                            "{ params = [ " + controls.join(' ') + " ] }"});
    }

    void stop() {
        if (!target_.name.isEmpty()) {
            try {
                auto restore = target_.name;
                const auto available = devices();
                bool targetFound = false;
                for (const auto &device : available) if (device.name == restore) targetFound = true;
                if (!targetFound && !available.isEmpty()) {
                    restore = std::max_element(available.begin(), available.end(), [](const Device &a, const Device &b) {
                        return a.priority < b.priority;
                    })->name;
                }
                if (targetFound || !available.isEmpty()) {
                    if (defaultSink() == kSink) command("pactl", {"set-default-sink", restore});
                }
                for (const auto &item : pactlList("sinks")) {
                    const auto sink = item.toObject();
                    if (sink.value("name").toString() == kSink && (targetFound || !available.isEmpty()))
                        moveStreams(sink.value("index").toInt(-1), restore);
                }
            } catch (const std::exception &) {
                // A removed target cannot be restored; PipeWire selects the remaining default.
            }
        }
        if (active()) {
            process_.terminate();
            if (!process_.waitForFinished(2000)) {
                process_.kill();
                process_.waitForFinished(2000);
            }
        }
        target_ = {};
    }

    ~AudioEngine() { stop(); }

private:
    QTemporaryDir directory_{QDir::tempPath() + "/soundcurrent-eq-XXXXXX"};
    QProcess process_;
    Device target_;
};

QMap<QString, Gains> builtins() {
    return {
        {"Flat", {0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {"Balanced", {1, 1, 0.5, 0, -0.5, 0, 0.5, 1, 1}},
        {"Bass Boost", {5, 4, 3, 1.5, 0, 0, 0, 0, 0}},
        {"Clear Voice", {-3, -2, -1, 0, 1, 2.5, 3, 1.5, 0}},
        {"Warm", {2.5, 2, 1.5, 0.5, 0, -0.5, -1, -1, -1.5}},
        {"Bright", {-1, -1, -0.5, 0, 0.5, 1, 2, 2.5, 2.5}},
        {"Movies", {3, 2.5, 1.5, 0, -1, 0, 1, 2, 2}},
    };
}

QString presetsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/presets.json";
}

class MainWindow : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle("SoundCurrent EQ");
        setMinimumSize(700, 570);
        resize(900, 750);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        setCentralWidget(scroll);
        auto *container = new QWidget;
        auto *root = new QVBoxLayout(container);
        root->setContentsMargins(26, 24, 26, 24);
        root->setSpacing(18);
        scroll->setWidget(container);

        auto *title = new QLabel("Shape your sound");
        title->setObjectName("title");
        root->addWidget(title);
        root->addWidget(new QLabel("A simple system-wide equalizer for PipeWire audio."));

        auto *outputBox = new QGroupBox("Output device");
        auto *outputLayout = new QVBoxLayout(outputBox);
        auto *outputRow = new QHBoxLayout;
        outputCombo_ = new QComboBox;
        outputCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        outputRow->addWidget(outputCombo_);
        auto *refresh = new QPushButton("Refresh");
        outputRow->addWidget(refresh);
        power_ = new QCheckBox("Equalizer on");
        outputRow->addWidget(power_);
        outputLayout->addLayout(outputRow);
        status_ = new QLabel("Equalizer is off. Your audio uses its normal output.");
        status_->setWordWrap(true);
        status_->setObjectName("status");
        outputLayout->addWidget(status_);
        root->addWidget(outputBox);

        auto *presetBox = new QGroupBox("Listening preset");
        auto *presetRow = new QHBoxLayout(presetBox);
        presetCombo_ = new QComboBox;
        presetRow->addWidget(presetCombo_, 1);
        auto *save = new QPushButton("Save preset");
        presetRow->addWidget(save);
        auto *reset = new QPushButton("Reset to flat");
        presetRow->addWidget(reset);
        root->addWidget(presetBox);

        auto *eqBox = new QGroupBox("Nine-band equalizer");
        auto *eqLayout = new QVBoxLayout(eqBox);
        headroom_ = new QLabel;
        headroom_->setAlignment(Qt::AlignRight);
        eqLayout->addWidget(headroom_);
        auto *bands = new QHBoxLayout;
        bands->setSpacing(12);
        for (size_t i = 0; i < sliders_.size(); ++i) {
            auto *column = new QVBoxLayout;
            auto *value = new QLabel("0 dB");
            value->setAlignment(Qt::AlignCenter);
            value->setObjectName("value");
            values_[i] = value;
            column->addWidget(value);
            auto *slider = new QSlider(Qt::Vertical);
            slider->setRange(-24, 24);
            slider->setSingleStep(1);
            slider->setPageStep(2);
            slider->setTickInterval(4);
            slider->setTickPosition(QSlider::TicksRight);
            slider->setMinimumHeight(190);
            slider->setToolTip(QString::number(kFrequencies[i]) + " Hz");
            sliders_[i] = slider;
            column->addWidget(slider, 1, Qt::AlignHCenter);
            auto *frequency = new QLabel(kFrequencies[i] >= 1000
                                             ? QString::number(kFrequencies[i] / 1000) + "k"
                                             : QString::number(kFrequencies[i]));
            frequency->setAlignment(Qt::AlignCenter);
            column->addWidget(frequency);
            bands->addLayout(column, 1);
            connect(slider, &QSlider::valueChanged, this, [this] { bandChanged(); });
        }
        eqLayout->addLayout(bands, 1);
        eqLayout->addWidget(new QLabel("Boosted bands automatically reduce overall gain to leave headroom."));
        root->addWidget(eqBox, 1);

        loadPresets();
        rebuildPresets("Balanced");
        refreshDevices();

        connect(refresh, &QPushButton::clicked, this, [this] { refreshDevices(); });
        connect(power_, &QCheckBox::toggled, this, [this](bool on) { togglePower(on); });
        connect(outputCombo_, &QComboBox::currentIndexChanged, this, [this] { outputChanged(); });
        connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this] { presetChanged(); });
        connect(save, &QPushButton::clicked, this, [this] { savePreset(); });
        connect(reset, &QPushButton::clicked, this, [this] { presetCombo_->setCurrentText("Flat"); });
        applyTimer_.setSingleShot(true);
        applyTimer_.setInterval(80);
        connect(&applyTimer_, &QTimer::timeout, this, [this] {
            try { audio_.update(gains()); } catch (const std::exception &error) { showError(error.what()); }
        });
        monitor_.setInterval(1500);
        connect(&monitor_, &QTimer::timeout, this, [this] { refreshDevices(); });
        monitor_.start();
    }

    ~MainWindow() override { audio_.stop(); }

private:
    Gains gains() const {
        Gains result{};
        for (size_t i = 0; i < result.size(); ++i) result[i] = sliders_[i]->value() / 2.0;
        return result;
    }

    void setGains(const Gains &value) {
        changing_ = true;
        for (size_t i = 0; i < value.size(); ++i) sliders_[i]->setValue(std::lround(value[i] * 2));
        changing_ = false;
        updateLabels();
        applyTimer_.start();
    }

    void updateLabels() {
        const auto current = gains();
        for (size_t i = 0; i < current.size(); ++i)
            values_[i]->setText((current[i] > 0 ? "+" : "") + QString::number(current[i], 'g', 3) + " dB");
        headroom_->setText("Headroom " + QString::number(headroom(current), 'g', 3) + " dB");
    }

    void bandChanged() {
        if (changing_) return;
        updateLabels();
        changing_ = true;
        presetCombo_->setCurrentText("Custom");
        changing_ = false;
        applyTimer_.start();
    }

    void presetChanged() {
        if (changing_) return;
        const auto name = presetCombo_->currentText();
        if (presets_.contains(name)) setGains(presets_[name]);
    }

    void loadPresets() {
        presets_ = builtins();
        QFile file(presetsPath());
        if (!file.open(QIODevice::ReadOnly)) return;
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key().trimmed().isEmpty() || it.key() == "Custom" || presets_.contains(it.key()) ||
                !it.value().isArray() || it.value().toArray().size() != 9) continue;
            Gains value{};
            bool valid = true;
            for (int i = 0; i < 9; ++i) {
                const auto element = it.value().toArray()[i];
                if (!element.isDouble() || element.toDouble() < -12 || element.toDouble() > 12) valid = false;
                value[i] = element.toDouble();
            }
            if (valid) presets_[it.key()] = value;
        }
    }

    void rebuildPresets(const QString &selected) {
        changing_ = true;
        presetCombo_->clear();
        for (const auto &name : {"Balanced", "Bass Boost", "Clear Voice", "Warm", "Bright", "Movies", "Flat"})
            presetCombo_->addItem(name);
        for (auto it = presets_.begin(); it != presets_.end(); ++it)
            if (!builtins().contains(it.key())) presetCombo_->addItem(it.key());
        presetCombo_->addItem("Custom");
        presetCombo_->setCurrentText(selected);
        changing_ = false;
        if (presets_.contains(selected)) setGains(presets_[selected]);
    }

    void savePreset() {
        bool ok = false;
        const auto name = QInputDialog::getText(this, "Save EQ preset", "Preset name:", QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok) return;
        if (name.isEmpty() || name == "Custom" || builtins().contains(name)) {
            showError("Choose a name that is not a built-in preset.");
            return;
        }
        presets_[name] = gains();
        QJsonObject object;
        for (auto it = presets_.begin(); it != presets_.end(); ++it) {
            if (builtins().contains(it.key())) continue;
            QJsonArray values;
            for (const auto value : it.value()) values.append(value);
            object.insert(it.key(), values);
        }
        const auto path = presetsPath();
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) { showError("Could not create preset folder."); return; }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) { showError("Could not save preset."); return; }
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        file.write(QJsonDocument(object).toJson());
        if (!file.commit()) { showError("Could not finish saving preset."); return; }
        rebuildPresets(name);
        status_->setText("Saved preset “" + name + "”.");
    }

    Device bestDevice(const QList<Device> &candidates) const {
        if (candidates.isEmpty()) return {};
        return *std::max_element(candidates.begin(), candidates.end(), [](const Device &a, const Device &b) {
            return a.priority < b.priority;
        });
    }

    Device findDevice(const QString &name) const {
        for (const auto &device : devices_) if (device.name == name) return device;
        return {};
    }

    Device selectedDevice() const {
        const auto manual = outputCombo_->currentData().toString();
        if (!manual.isEmpty()) return findDevice(manual);
        try {
            const auto current = defaultSink();
            if (current != kSink) {
                const auto device = findDevice(current);
                if (!device.name.isEmpty()) return device;
            }
        } catch (const std::exception &) {}
        return bestDevice(devices_);
    }

    void refreshDevices() {
        try {
            const auto latest = devices();
            const auto selectedName = outputCombo_->currentData().toString();
            QList<Device> added;
            for (const auto &device : latest)
                if (!knownNames_.contains(device.name)) added.append(device);
            const bool initial = knownNames_.isEmpty();
            const bool changed = devices_.size() != latest.size() ||
                !std::equal(devices_.begin(), devices_.end(), latest.begin(), [](const Device &a, const Device &b) {
                    return a.name == b.name && a.description == b.description;
                });
            devices_ = latest;
            knownNames_.clear();
            for (const auto &device : devices_) knownNames_.append(device.name);
            int index = outputCombo_->findData(selectedName);
            if (changed) {
                outputCombo_->blockSignals(true);
                outputCombo_->clear();
                outputCombo_->addItem("Automatic (follow connected devices)", QString());
                for (const auto &device : devices_) outputCombo_->addItem(device.description, device.name);
                index = outputCombo_->findData(selectedName);
                outputCombo_->setCurrentIndex(index >= 0 ? index : 0);
                outputCombo_->blockSignals(false);
            }
            power_->setEnabled(!devices_.isEmpty());

            if (!power_->isChecked()) return;
            if (!audio_.active()) {
                power_->setChecked(false);
                showError("The PipeWire filter stopped unexpectedly.");
                return;
            }
            Device desired;
            if (!selectedName.isEmpty() && index >= 0) {
                desired = findDevice(selectedName);
            } else if (!selectedName.isEmpty() && index < 0) {
                desired = bestDevice(devices_);
                status_->setText("Selected output was unplugged. Switched to automatic output.");
            } else if (!initial && !added.isEmpty()) {
                desired = bestDevice(added);
            } else if (!findDevice(audio_.target()).name.isEmpty()) {
                desired = findDevice(audio_.target());
                const auto current = defaultSink();
                if (current != kSink && !findDevice(current).name.isEmpty()) desired = findDevice(current);
            } else {
                desired = bestDevice(devices_);
            }
            if (desired.name != audio_.target()) {
                if (desired.name.isEmpty()) {
                    audio_.stop();
                    power_->setChecked(false);
                    status_->setText("No output device is connected.");
                } else {
                    audio_.start(desired, gains());
                    status_->setText("On · Playing through " + desired.description);
                }
            }
        } catch (const std::exception &error) {
            showError(error.what());
        }
    }

    void outputChanged() {
        if (!power_->isChecked()) return;
        const auto desired = selectedDevice();
        if (desired.name.isEmpty() || desired.name == audio_.target()) return;
        try {
            audio_.start(desired, gains());
            status_->setText("On · Playing through " + desired.description);
        } catch (const std::exception &error) { showError(error.what()); }
    }

    void togglePower(bool on) {
        if (on) {
            const auto device = selectedDevice();
            if (device.name.isEmpty()) { power_->setChecked(false); showError("No output device is available."); return; }
            try {
                audio_.start(device, gains());
                status_->setText("On · Playing through " + device.description);
            } catch (const std::exception &error) {
                power_->setChecked(false);
                showError(error.what());
            }
        } else {
            audio_.stop();
            status_->setText("Equalizer is off. Your audio uses its normal output.");
        }
    }

    void showError(const QString &message) { status_->setText("Audio error: " + message); }

    AudioEngine audio_;
    QList<Device> devices_;
    QStringList knownNames_;
    QMap<QString, Gains> presets_;
    QComboBox *outputCombo_ = nullptr;
    QComboBox *presetCombo_ = nullptr;
    QCheckBox *power_ = nullptr;
    QLabel *status_ = nullptr;
    QLabel *headroom_ = nullptr;
    std::array<QSlider *, 9> sliders_{};
    std::array<QLabel *, 9> values_{};
    QTimer applyTimer_;
    QTimer monitor_;
    bool changing_ = false;
};

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("SoundCurrent");
    QCoreApplication::setApplicationName("soundcurrent-eq");
    if (app.arguments().contains("--self-test")) {
        try {
            auto available = devices();
            if (available.isEmpty()) throw std::runtime_error("No audio output devices found");
            const auto current = defaultSink();
            auto selected = available.front();
            for (const auto &device : available) if (device.name == current) selected = device;
            const auto before = defaultSink();
            AudioEngine test;
            test.start(selected, builtins()["Bass Boost"]);
            test.update(builtins()["Clear Voice"]);
            if (defaultSink() != kSink) throw std::runtime_error("Equalizer did not become the default sink");
            test.stop();
            if (defaultSink() != before) throw std::runtime_error("Original output was not restored");
            qInfo("Audio routing self-test passed using %s", qPrintable(selected.description));
            return 0;
        } catch (const std::exception &error) {
            qCritical("Audio routing self-test failed: %s", error.what());
            return 1;
        }
    }
    app.setStyleSheet(R"(
        QWidget { background: #111827; color: #e8edf6; font-size: 13px; }
        QGroupBox { background: #1c293c; border: 1px solid #33445e; border-radius: 12px;
                    margin-top: 14px; padding: 14px; font-weight: 700; }
        QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 5px; }
        QLabel { background: transparent; }
        QCheckBox { background: transparent; }
        QLabel#title { font-size: 28px; font-weight: 800; }
        QLabel#value { color: #90d9ce; font-weight: 700; }
        QLabel#status { color: #90d9ce; }
        QPushButton, QComboBox { background: #2d405a; border: 1px solid #4a5d77;
                                border-radius: 7px; padding: 7px 10px; }
        QPushButton:hover, QComboBox:hover { background: #385572; }
        QComboBox QAbstractItemView { background: #26374d; selection-background-color: #2c9d91; }
        QSlider::groove:vertical { background: #344762; width: 7px; border-radius: 3px; }
        QSlider::handle:vertical { background: #eafbf7; height: 16px; margin: 0 -6px; border-radius: 8px; }
        QScrollArea { border: none; }
    )");
    MainWindow window;
    window.show();
    return app.exec();
}
