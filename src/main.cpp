#include "localization.h"
// SPDX-License-Identifier: GPL-3.0-only
#include "accelerating_spinbox.h"
// Copyright (C) 2026 rhamenator

#include "dsp.h"
#include "update_panel.h"
#include "enhancement_controls.h"
#ifndef SOUNDCURRENT_EFFECTS_DIR
#define SOUNDCURRENT_EFFECTS_DIR "/usr/lib/soundcurrent-eq"
#endif
#include "equipment_profiles.h"
#include "processing_guard.h"
#ifdef _WIN32
#include "windows_audio.h"
#include "windows_managed_route.h"
#endif
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMainWindow>
#include <QMessageBox>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <complex>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <csignal>
#include <future>
#include <cstring>
#ifndef Q_OS_WIN
#include <sys/prctl.h>
#include <unistd.h>
#endif

namespace {

constexpr auto kSink = "soundcurrent_eq";
constexpr auto kOutput = "soundcurrent_eq_output";
constexpr auto kMicSource = "soundcurrent_mic";
constexpr auto kMicInput = "soundcurrent_mic_input";
constexpr int kMinBands = 5;
constexpr int kDefaultBands = 15;
constexpr int kMaxBands = 31;
constexpr int kProcessingBands = int(soundcurrent::kMaxProcessingBands);
constexpr std::array<int, 31> kIsoFrequencies = {
    20, 25, 31, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630,
    800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000,
    12500, 16000, 20000
};
constexpr std::array<int, 9> kLegacyFrequencies = {32, 64, 125, 250, 500, 1000, 2000, 4000, 8000};

struct Band {
    double frequency = 1000.0;
    double gain = 0.0;
    double q = 1.0;
    soundcurrent::FilterType type = soundcurrent::FilterType::Peaking;
};
using Bands = QVector<Band>;

struct SpeakerProfile {
    QString id, name, attribution;
    QString brand, equipmentType = "Unclassified";
    QStringList links;
    Bands filters;
};
const QVector<SpeakerProfile> &speakerProfiles() {
    static const QVector<SpeakerProfile> profiles = [] {
        QFile file(":/speakers/profiles.json");
        if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error(SC_TR("Speaker profile resource is missing").toStdString());
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        if (root.value("schema").toInt() != 1) throw std::runtime_error(SC_TR("Unsupported speaker profile schema").toStdString());
        QVector<SpeakerProfile> result;
        QStringList ids;
        for (const auto &value : root.value("profiles").toArray()) {
            const auto item = value.toObject();
            SpeakerProfile profile;
            profile.id = item.value("id").toString();
            profile.name = item.value("name").toString();
            if (profile.id == "Sony SS-CS5") profile.name += " (original; not SS-CS5M2)";
            if (profile.id.isEmpty() || ids.contains(profile.id)) throw std::runtime_error(SC_TR("Invalid speaker identity").toStdString());
            ids.append(profile.id);
            profile.attribution = item.value("measurement").toString() + " · " + item.value("measurementDate").toString();
            profile.links.append(item.value("sourceUrl").toString());
            for (const auto &link : item.value("measurementUrls").toArray()) profile.links.append(link.toString());
            for (const auto &filter : item.value("filters").toArray()) {
                const auto f = filter.toObject();
                const auto type = f.value("type").toString();
                using T = soundcurrent::FilterType;
                if (type != "PK" && type != "LS" && type != "HS") throw std::runtime_error(SC_TR("Invalid speaker filter type").toStdString());
                Band band{f.value("frequency").toDouble(-1), f.value("gain").toDouble(999), f.value("q").toDouble(-1),
                          type == "LS" ? T::LowShelf : type == "HS" ? T::HighShelf : T::Peaking};
                if (!std::isfinite(band.frequency) || !std::isfinite(band.gain) || !std::isfinite(band.q) ||
                    band.frequency < 20 || band.frequency > 20000 || std::abs(band.gain) > 6 ||
                    band.q < 0.1 || band.q > 6 || (band.frequency < 80 && band.gain > 0))
                    throw std::runtime_error(SC_TR("Speaker filter is outside conservative bounds").toStdString());
                profile.filters.append(band);
            }
            if (profile.filters.isEmpty() || profile.filters.size() > 16)
                throw std::runtime_error(SC_TR("Invalid speaker correction filter count").toStdString());
            result.append(profile);
        }
        for(const auto &p:soundcurrent::equipment::bundledProfiles()) {
            if(p.kind!="speaker")continue;
            const QString fullName=p.brand+" "+p.model;
            bool existing=false;
            for(auto &legacy:result)if(legacy.id.compare(fullName,Qt::CaseInsensitive)==0) {legacy.brand=p.brand;legacy.equipmentType=p.equipmentType;existing=true;break;}
            if(existing)continue;
            SpeakerProfile entry;entry.id=p.id;entry.name=fullName;entry.brand=p.brand;entry.equipmentType=p.equipmentType;
            entry.attribution=p.conditions+"\n"+p.provenance;entry.links={p.source};
            for(const auto &b:p.filters)entry.filters.append({b.frequency,b.gainDb,b.q,b.type});
            result.append(entry);
        }
        for(auto &p:result)if(p.brand.isEmpty())p.brand=p.id.section(' ',0,0);
        std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.name.compare(b.name,Qt::CaseInsensitive)<0;});
        return result;
    }();
    return profiles;
}

double interpolate(const Bands &source, double frequency, bool forQ = false) {
    if (source.isEmpty()) return forQ ? 1.0 : 0.0;
    if (frequency <= source.first().frequency) return forQ ? source.first().q : source.first().gain;
    if (frequency >= source.last().frequency) return forQ ? source.last().q : source.last().gain;
    for (qsizetype i = 1; i < source.size(); ++i) {
        if (frequency <= source[i].frequency) {
            const auto &left = source[i - 1];
            const auto &right = source[i];
            const auto fraction = std::log(frequency / left.frequency) / std::log(right.frequency / left.frequency);
            return (forQ ? left.q : left.gain) * (1 - fraction) + (forQ ? right.q : right.gain) * fraction;
        }
    }
    return 0.0;
}

Bands defaultBands(int count) {
    Bands result;
    result.reserve(count);
    if (count == kDefaultBands) {
        constexpr std::array<int, 15> standard = {25, 40, 63, 100, 160, 250, 400, 630,
                                                   1000, 1600, 2500, 4000, 6300, 10000, 16000};
        for (const auto frequency : standard) result.append({double(frequency), 0.0, 1.0});
        return result;
    }
    for (int i = 0; i < count; ++i) {
        const auto index = std::lround(double(i) * (kIsoFrequencies.size() - 1) / (count - 1));
        result.append({double(kIsoFrequencies[index]), 0.0, 1.0});
    }
    return result;
}

Bands remapBands(const Bands &old, int count) {
    auto result = defaultBands(count);
    for (auto &band : result) {
        band.gain = std::clamp(interpolate(old, band.frequency), -12.0, 12.0);
        band.q = std::clamp(interpolate(old, band.frequency, true), 0.3, 10.0);
    }
    return result;
}

struct Device {
    QString name;
    QString description;
    int index = -1;
    int priority = 0;
};

struct InputDevice : Device { int channels = 1; };

#ifndef Q_OS_WIN
QString command(const QString &program, const QStringList &arguments, int timeout = 5000) {
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(timeout) || !process.waitForFinished(timeout) ||
        process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const auto error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        throw std::runtime_error((error.isEmpty() ? SC_TR("Could not run %1").arg(program) : error).toStdString());
    }
    return QString::fromUtf8(process.readAllStandardOutput());
}

QJsonArray pactlList(const QString &kind) {
    const auto document = QJsonDocument::fromJson(command("pactl", {"--format=json", "list", kind}).toUtf8());
    if (!document.isArray()) throw std::runtime_error(SC_TR("Invalid response from pactl").toStdString());
    return document.array();
}

QString defaultSink() { return command("pactl", {"get-default-sink"}).trimmed(); }
QString defaultSource() { return command("pactl", {"get-default-source"}).trimmed(); }

struct SinkState {
    QStringList volumes;
    bool muted = false;
};

SinkState sinkState(const QString &name) {
    for (const auto &item : pactlList("sinks")) {
        const auto sink = item.toObject();
        if (sink.value("name").toString() != name) continue;
        const auto channels = sink.value("channel_map").toString().split(',', Qt::SkipEmptyParts);
        const auto volume = sink.value("volume").toObject();
        SinkState state;
        for (const auto &channel : channels) {
            const int raw = volume.value(channel).toObject().value("value").toInt(-1);
            if (raw < 0) throw std::runtime_error(SC_TR("Could not read output volume").toStdString());
            state.volumes << QString::number(raw);
        }
        if (state.volumes.isEmpty()) throw std::runtime_error(SC_TR("Output has no volume channels").toStdString());
        state.muted = sink.value("mute").toBool();
        return state;
    }
    throw std::runtime_error(SC_TR("Output device is no longer available").toStdString());
}

void setSinkState(const QString &name, const SinkState &state) {
    command("pactl", QStringList{"set-sink-volume", name} + state.volumes);
    command("pactl", {"set-sink-mute", name, state.muted ? "1" : "0"});
}

int guardOutputVolume(const QString &target, const QString &rawVolumes, const QString &rawMute) {
    if (target.isEmpty() || target == kSink || (rawMute != "0" && rawMute != "1")) return 2;
    SinkState original;
    original.muted = rawMute == "1";
    for (const auto &value : rawVolumes.split(',', Qt::SkipEmptyParts)) {
        bool valid = false;
        const int raw = value.toInt(&valid);
        if (!valid || raw < 0 || raw > 131072) return 2;
        original.volumes << value;
    }
    if (original.volumes.isEmpty() || original.volumes.size() > 8) return 2;
    char message = 0;
    while (true) {
        const auto received = ::read(STDIN_FILENO, &message, 1);
        if (received == 1 && message == 'Q') return 0;
        if (received == 0) break;
        if (received < 0 && errno != EINTR) return 1;
    }
    try { setSinkState(target, original); }
    catch (const std::exception &) { return 1; }
    return 0;
}

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

bool inputPortAvailable(const QJsonObject &source) {
    const auto active = source.value("active_port").toString();
    const auto ports = source.value("ports").toArray();
    for (const auto &value : ports) {
        const auto port = value.toObject();
        if (port.value("name").toString() == active)
            return port.value("availability").toString() != "not available";
    }
    // USB adapters often cannot detect whether a microphone is plugged into them.
    return ports.isEmpty() || std::any_of(ports.begin(), ports.end(), [](const auto &value) {
        return value.toObject().value("availability").toString() != "not available";
    });
}

QList<InputDevice> inputDevices() {
    QList<InputDevice> result;
    for (const auto &item : pactlList("sources")) {
        const auto source = item.toObject();
        const auto name = source.value("name").toString();
        if (name == kMicSource || name.endsWith(".monitor")) continue;
        if (!inputPortAvailable(source)) continue;
        const auto channels = source.value("channel_map").toString().split(',', Qt::SkipEmptyParts).size();
        if (channels < 1 || channels > 2) continue;
        const auto properties = source.value("properties").toObject();
        if (properties.value("node.virtual").toVariant().toString() == "true") continue;
        InputDevice device;
        device.name = name;
        device.description = source.value("description").toString(name);
        device.index = source.value("index").toInt(-1);
        device.priority = properties.value("priority.session").toVariant().toString().toInt();
        device.channels = channels;
        result.append(device);
    }
    return result;
}

void moveCaptureStreams(int fromIndex, const QString &toName) {
    for (const auto &item : pactlList("source-outputs")) {
        const auto stream = item.toObject();
        if (stream.value("source").toInt(-1) != fromIndex ||
            stream.value("properties").toObject().value("node.name").toString() == kMicInput) continue;
        try { command("pactl", {"move-source-output", QString::number(stream.value("index").toInt()), toName}); }
        catch (const std::exception &) {}
    }
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

#else
QList<Device> devices();
QList<InputDevice> inputDevices();
QString defaultSink();
QString defaultSource();
#endif

double responseDb(const Bands &bands, double frequency) {
    double total = 0;
    for (const auto &b : bands)
        total += soundcurrent::filterResponseDb({b.frequency, b.gain, b.q, b.type}, 48000, frequency);
    return total;
}

double headroom(const Bands &bands) {
    double peak = 0.0;
    for (int i = 0; i <= 512; ++i) {
        const auto frequency = 20.0 * std::pow(1000.0, double(i) / 512.0);
        peak = std::max(peak, responseDb(bands, frequency));
    }
    return peak > 0.01 ? -(peak + 1.0) : 0.0;
}

std::array<double, 2> balanceFactors(int balancePercent) {
    const double balance = std::clamp(balancePercent, -100, 100) / 100.0;
    return {std::min(1.0, 1.0 - balance), std::min(1.0, 1.0 + balance)};
}

QString quote(const QString &value) {
    const auto encoded = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(encoded.mid(1, encoded.size() - 2));
}

QString filterConfig(const QString &target, const Bands &bands, double outputGainDb = 0.0,
                     int balancePercent = 0, bool smartFilter = false) {
    QStringList nodes;
    QStringList links;
    const auto factors = balanceFactors(balancePercent);
    const auto preamp = QString::number(std::pow(10.0, headroom(bands) / 20.0), 'f', 8);
    const double postGain = std::pow(10.0, outputGainDb / 20.0);
    constexpr std::array<const char *, 2> channels = {"left", "right"};
    for (size_t channelIndex = 0; channelIndex < channels.size(); ++channelIndex) {
        const QString channel = channels[channelIndex];
        nodes << QString("{ type = builtin name = %1_preamp label = linear control = { \"Mult\" = %2 \"Add\" = 0.0 } }")
                     .arg(channel, preamp);
        links << QString("{ output = \"%1_preamp:Out\" input = \"%1_band_1:In\" }").arg(channel);
        for (int i = 0; i < kProcessingBands; ++i) {
            const auto band = i < bands.size() ? bands[i] : Band{};
            if (i < kMaxBands) {
            nodes << QString("{ type = builtin name = %1_band_%2 label = bq_peaking control = { \"Freq\" = %3 \"Q\" = %4 \"Gain\" = %5 } }")
                         .arg(channel).arg(i + 1).arg(band.frequency, 0, 'f', 1)
                         .arg(band.q, 0, 'f', 2).arg(band.gain, 0, 'f', 2);
            } else {
                const auto c = soundcurrent::filterCoefficients({band.frequency, band.gain, band.q, band.type}, 48000);
                nodes << QString("{ type = builtin name = %1_band_%2 label = bq_raw config = { coefficients = [ { rate = 48000 b0 = 1 b1 = 0 b2 = 0 a0 = 1 a1 = 0 a2 = 0 } ] } control = { \"b0\" = %3 \"b1\" = %4 \"b2\" = %5 \"a0\" = 1 \"a1\" = %6 \"a2\" = %7 } }")
                    .arg(channel).arg(i + 1).arg(c.b0, 0, 'g', 16).arg(c.b1, 0, 'g', 16)
                    .arg(c.b2, 0, 'g', 16).arg(c.a1, 0, 'g', 16).arg(c.a2, 0, 'g', 16);
            }
            if (i > 0)
                links << QString("{ output = \"%1_band_%2:Out\" input = \"%1_band_%3:In\" }")
                             .arg(channel).arg(i).arg(i + 1);
        }
        nodes << QString("{ type = builtin name = %1_output_gain label = linear control = { \"Mult\" = %2 \"Add\" = 0.0 } }")
                     .arg(channel, QString::number(postGain * factors[channelIndex], 'f', 8));
        links << QString("{ output = \"%1_band_%2:Out\" input = \"%1_output_gain:In\" }")
                     .arg(channel).arg(kProcessingBands);
    }
    const auto buildPlugin=QCoreApplication::applicationDirPath()+"/effects/soundcurrent-enhancements.so";
    QString plugin=QStringLiteral(SOUNDCURRENT_EFFECTS_DIR "/soundcurrent-enhancements.so");
    for(const auto &relative:{QString("../lib/x86_64-linux-gnu/soundcurrent-eq/"),QString("../lib64/soundcurrent-eq/"),QString("../lib/soundcurrent-eq/")}) {
        const auto candidate=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(relative+"soundcurrent-enhancements.so");
        if(QFileInfo::exists(candidate)){plugin=candidate;break;}
    }
    if(QFileInfo::exists(buildPlugin))plugin=buildPlugin;
    nodes << QString("{ type = ladspa name = enhancements plugin = %1 label = soundcurrent_enhancements control = { Clarity = 0 Ambience = 0 Surround = 0 Dynamic = 0 Bass = 0 } }").arg(quote(plugin));
    for(const QString &channel:{QString("left"),QString("right")}) {
        const QString side=channel=="left"?"Left":"Right";
        links.removeAll(QString("{ output = \"%1_band_%2:Out\" input = \"%1_output_gain:In\" }").arg(channel).arg(kProcessingBands));
        links << QString("{ output = \"%1_band_%2:Out\" input = \"enhancements:%3 input\" }").arg(channel).arg(kProcessingBands).arg(side);
        links << QString("{ output = \"enhancements:%2 output\" input = \"%1_output_gain:In\" }").arg(channel,side);
    }
    const auto smartProperties = smartFilter
        ? QString("filter.smart = true filter.smart.target = { node.name = %1 }").arg(quote(target))
        : QString();
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
      audio.rate = 48000
      audio.channels = 2
      audio.position = [ FL FR ]
      filter.graph = {
        nodes = [ %1 ]
        links = [ %2 ]
        inputs = [ "left_preamp:In" "right_preamp:In" ]
        outputs = [ "left_output_gain:Out" "right_output_gain:Out" ]
      }
      capture.props = { node.name = "%3" media.class = Audio/Sink %6 }
      playback.props = {
        node.name = "%4"
        target.object = %5
        node.passive = true
        state.restore-props = false
        state.default-volume = 1.0
      }
    }
  }
]
)").arg(nodes.join('\n'), links.join('\n'), kSink, kOutput, quote(target), smartProperties);
}

struct MicTuning : std::array<double, 4> {
    MicTuning(double a=0, double b=0, double c=0, double d=0) : std::array<double,4>{a,b,c,d} {}
    Bands correction;
};
constexpr std::array<double, 4> kNaturalMic = {-1.5, -1.0, 1.5, -1.0};
constexpr std::array<int, 4> kMicFrequencies = {180, 350, 2800, 10000};
constexpr std::array<const char *, 4> kMicLabels = {"bq_lowshelf", "bq_peaking", "bq_peaking", "bq_highshelf"};

QString micControls(const MicTuning &adjustments, double gainDb, int channels) {
    QStringList controls;
    for (int channel = 0; channel < channels; ++channel) {
        const QString prefix = channel ? "right" : "left";
        for (int band = 0; band < 4; ++band)
            controls << QString("\"%1_mic_%2:Gain\" %3").arg(prefix).arg(band + 1)
                            .arg((adjustments.correction.isEmpty() ? kNaturalMic[band] : 0.0) + adjustments[band], 0, 'f', 2);
        for (int i = 0; i < 16; ++i) {
            const auto band = i < adjustments.correction.size() ? adjustments.correction[i] : Band{};
            const auto c = soundcurrent::filterCoefficients({band.frequency, band.gain, band.q, band.type}, 48000);
            const auto name = prefix + QString("_profile_%1:").arg(i);
            controls << quote(name + "b0") << QString::number(c.b0, 'g', 16)
                     << quote(name + "b1") << QString::number(c.b1, 'g', 16)
                     << quote(name + "b2") << QString::number(c.b2, 'g', 16)
                     << quote(name + "a0") << "1"
                     << quote(name + "a1") << QString::number(c.a1, 'g', 16)
                     << quote(name + "a2") << QString::number(c.a2, 'g', 16);
        }
        controls << QString("\"%1_mic_gain:Mult\" %2").arg(prefix)
                        .arg(std::pow(10.0, gainDb / 20.0), 0, 'f', 8);
    }
    return "{ params = [ " + controls.join(' ') + " ] }";
}

QString micConfig(const QString &target, int channels, const MicTuning &adjustments,
                  double gainDb, bool smart) {
    QStringList nodes, links, inputs, outputs;
    for (int channel = 0; channel < channels; ++channel) {
        const QString prefix = channel ? "right" : "left";
        nodes << QString("{ type = builtin name = %1_mic_highpass label = bq_highpass control = { \"Freq\" = 80 \"Q\" = 0.707 } }")
                     .arg(prefix);
        inputs << QString("\"%1_mic_highpass:In\"").arg(prefix);
        for (int band = 0; band < 4; ++band) {
            nodes << QString("{ type = builtin name = %1_mic_%2 label = %3 control = { \"Freq\" = %4 \"Q\" = 0.8 \"Gain\" = %5 } }")
                         .arg(prefix).arg(band + 1).arg(kMicLabels[band]).arg(kMicFrequencies[band])
                         .arg((adjustments.correction.isEmpty() ? kNaturalMic[band] : 0.0) + adjustments[band], 0, 'f', 2);
            links << QString("{ output = \"%1_mic_%2:Out\" input = \"%1_mic_%3:In\" }")
                         .arg(prefix).arg(band == 0 ? "highpass" : QString::number(band)).arg(band + 1);
        }
        nodes << QString("{ type = builtin name = %1_mic_gain label = linear control = { \"Mult\" = %2 \"Add\" = 0.0 } }")
                     .arg(prefix).arg(std::pow(10.0, gainDb / 20.0), 0, 'f', 8);
        QString previous = prefix + "_mic_4";
        for (int i = 0; i < 16; ++i) {
            const auto band = i < adjustments.correction.size() ? adjustments.correction[i] : Band{};
            const auto c = soundcurrent::filterCoefficients({band.frequency, band.gain, band.q, band.type}, 48000);
            const auto name = prefix + QString("_profile_%1").arg(i);
            nodes << QString("{ type = builtin name = %1 label = bq_raw control = { b0 = %2 b1 = %3 b2 = %4 a0 = 1 a1 = %5 a2 = %6 } }")
                .arg(name).arg(c.b0,0,'g',16).arg(c.b1,0,'g',16).arg(c.b2,0,'g',16).arg(c.a1,0,'g',16).arg(c.a2,0,'g',16);
            links << QString("{ output = \"%1:Out\" input = \"%2:In\" }").arg(previous,name);
            previous = name;
        }
        links << QString("{ output = \"%1:Out\" input = \"%2_mic_gain:In\" }").arg(previous,prefix);
        outputs << QString("\"%1_mic_gain:Out\"").arg(prefix);
    }
    const auto positions = channels == 1 ? "[ MONO ]" : "[ FL FR ]";
    const auto smartProperties = smart
        ? QString("filter.smart = true filter.smart.target = { node.name = %1 }").arg(quote(target))
        : QString();
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
      audio.rate = 48000
      node.description = "SoundCurrent Natural Microphone"
      media.name = "SoundCurrent Natural Microphone"
      audio.channels = %1
      audio.position = %2
      filter.graph = {
        nodes = [ %3 ]
        links = [ %4 ]
        inputs = [ %5 ]
        outputs = [ %6 ]
      }
      capture.props = {
        node.name = "%7"
        audio.channels = %1
        audio.position = %2
        target.object = %9
        node.passive = true
      }
      playback.props = {
        node.name = "%8"
        media.class = Audio/Source
        audio.channels = %1
        audio.position = %2
        state.restore-props = false
        state.default-volume = 1.0
        %10
      }
    }
  }
]
)").arg(QString::number(channels), positions, nodes.join('\n'), links.join('\n'),
           inputs.join(' '), outputs.join(' '), kMicInput, kMicSource, quote(target), smartProperties);
}

#ifndef Q_OS_WIN
bool smartFiltersAvailable() {
    if (qEnvironmentVariableIsSet("SOUNDCURRENT_FORCE_LEGACY_FILTER")) return false;
    try {
        const auto version = command("wireplumber", {"--version"}, 2000);
        const auto match = QRegularExpression(R"(libwireplumber\s+(\d+)\.(\d+))").match(version);
        if (!match.hasMatch()) return false;
        const int major = match.captured(1).toInt();
        const int minor = match.captured(2).toInt();
        return major > 0 || minor >= 5;
    } catch (const std::exception &) { return false; }
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

#endif

QString filterControls(const Bands &bands, double outputGainDb = 0.0, int balancePercent = 0) {
    QStringList controls;
    const auto factors = balanceFactors(balancePercent);
    const auto preamp = QString::number(std::pow(10.0, headroom(bands) / 20.0), 'f', 8);
    const double postGain = std::pow(10.0, outputGainDb / 20.0);
    constexpr std::array<const char *, 2> channels = {"left", "right"};
    for (size_t channelIndex = 0; channelIndex < channels.size(); ++channelIndex) {
        const QString channel = channels[channelIndex];
        controls << quote(channel + "_preamp:Mult") << preamp;
        for (int i = 0; i < kProcessingBands; ++i) {
            const auto band = i < bands.size() ? bands[i] : Band{};
            const auto name = channel + QString("_band_%1:").arg(i + 1);
            if (i < kMaxBands) {
            controls << quote(name + "Freq") << QString::number(band.frequency, 'f', 1)
                     << quote(name + "Q") << QString::number(band.q, 'f', 2)
                     << quote(name + "Gain") << QString::number(band.gain, 'f', 2);
            } else {
                const auto c = soundcurrent::filterCoefficients({band.frequency, band.gain, band.q, band.type}, 48000);
                controls << quote(name + "b0") << QString::number(c.b0, 'g', 16)
                         << quote(name + "b1") << QString::number(c.b1, 'g', 16)
                         << quote(name + "b2") << QString::number(c.b2, 'g', 16)
                         << quote(name + "a0") << "1"
                         << quote(name + "a1") << QString::number(c.a1, 'g', 16)
                         << quote(name + "a2") << QString::number(c.a2, 'g', 16);
            }
        }
        controls << quote(channel + "_output_gain:Mult")
                 << QString::number(postGain * factors[channelIndex], 'f', 8);
    }
    return "{ params = [ " + controls.join(' ') + " ] }";
}

QString gainControls(double outputGainDb, int balancePercent) {
    const auto factors = balanceFactors(balancePercent);
    const double postGain = std::pow(10.0, outputGainDb / 20.0);
    return QString("{ params = [ \"left_output_gain:Mult\" %1 \"right_output_gain:Mult\" %2 ] }")
        .arg(QString::number(postGain * factors[0], 'f', 8),
             QString::number(postGain * factors[1], 'f', 8));
}

#ifndef Q_OS_WIN
class AudioEngine {
public:
    void setEnhancements(const soundcurrent::EnhancementSettings &s) { effects_=s; }
    bool active() const { return process_.state() != QProcess::NotRunning; }
    QString target() const { return target_.name; }
    bool smart() const { return smart_; }
    bool legacyVolumeManaged() const { return legacyVolumeManaged_; }

    void start(const Device &device, const Bands &bands, double outputGainDb = 0.0,
               int balancePercent = 0) {
        stop();
        const auto previousDefault = defaultSink();
        bool found = false;
        for (const auto &available : devices()) if (available.name == device.name) found = true;
        if (!found) throw std::runtime_error(SC_TR("Selected output device is no longer available").toStdString());
        if (nodeId(kSink) >= 0) throw std::runtime_error(SC_TR("Another SoundCurrent EQ sink is already running").toStdString());
        if (!directory_.isValid()) throw std::runtime_error(SC_TR("Could not create temporary audio configuration").toStdString());
        const auto path = directory_.filePath("filter.conf");
        QFile config(path);
        if (!config.open(QIODevice::WriteOnly | QIODevice::Truncate))
            throw std::runtime_error(SC_TR("Could not write temporary audio configuration").toStdString());
        const bool smart = smartFiltersAvailable();
        config.write(filterConfig(device.name, bands, outputGainDb, balancePercent, smart).toUtf8());
        config.close();
        process_.setProgram("pipewire");
        process_.setArguments({"-c", path});
        process_.setProcessChannelMode(QProcess::MergedChannels);
        process_.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGTERM); });
        process_.start();
        if (!process_.waitForStarted(2000)) throw std::runtime_error(SC_TR("Could not start PipeWire filter").toStdString());
        QElapsedTimer clock;
        clock.start();
        while (clock.elapsed() < 3000) {
            if (process_.state() == QProcess::NotRunning)
                throw std::runtime_error(QString::fromUtf8(process_.readAll()).trimmed().toStdString());
            try { if (nodeId(kSink) >= 0) break; } catch (const std::exception &) {}
            QThread::msleep(100);
        }
        const int id = nodeId(kSink);
        if (id < 0) {
            const auto details = QString::fromUtf8(process_.readAll()).trimmed();
            stop();
            throw std::runtime_error(SC_TR("Timed out waiting for the equalizer sink: %1").arg(details).toStdString());
        }
        try {
            if (smart) {
                command("pactl", {"set-sink-volume", kSink, "100%"});
                command("pactl", {"set-sink-mute", kSink, "0"});
                if (previousDefault != device.name) {
                    command("pactl", {"set-default-sink", device.name});
                    for (const auto &available : devices())
                        if (available.name == previousDefault)
                            moveStreams(available.index, device.name);
                }
            } else {
                originalState_ = sinkState(device.name);
                guardian_.setProgram(QCoreApplication::applicationFilePath());
                guardian_.setArguments({"--volume-guardian", device.name,
                                        originalState_.volumes.join(','), originalState_.muted ? "1" : "0"});
                guardian_.start();
                if (!guardian_.waitForStarted(2000))
                    throw std::runtime_error(SC_TR("Could not start output volume safety guard").toStdString());
                target_ = device;
                legacyVolumeManaged_ = true;
                setSinkState(kSink, originalState_);
                command("pactl", {"set-sink-volume", device.name, "100%"});
                command("pactl", {"set-sink-mute", device.name, "0"});
                command("pactl", {"set-default-sink", kSink});
                moveStreams(device.index, kSink);
            }
            sinkId_ = id;
            smart_ = smart;
            target_ = device;
            update(bands,outputGainDb,balancePercent);
        } catch (const std::exception &) {
            stop();
            throw;
        }
    }

    void update(const Bands &bands, double outputGainDb = 0.0, int balancePercent = 0) {
        if (!active()) return;
        if (sinkId_ < 0) throw std::runtime_error(SC_TR("Equalizer sink disappeared").toStdString());
        command("pw-cli", {"set-param", QString::number(sinkId_), "Props", effectControls(filterControls(bands, outputGainDb, balancePercent))});
    }

    void updateGain(double outputGainDb, int balancePercent) {
        if (!active()) return;
        if (sinkId_ < 0) throw std::runtime_error(SC_TR("Equalizer sink disappeared").toStdString());
        command("pw-cli", {"set-param", QString::number(sinkId_), "Props", gainControls(outputGainDb, balancePercent)});
    }

    void stop() {
        bool volumeRestored = !legacyVolumeManaged_;
        if (legacyVolumeManaged_ && !target_.name.isEmpty()) {
            try {
                const auto current = nodeId(kSink) >= 0 ? sinkState(kSink) : originalState_;
                setSinkState(target_.name, current);
                volumeRestored = true;
            } catch (const std::exception &) {
                try {
                    setSinkState(target_.name, originalState_);
                    volumeRestored = true;
                } catch (const std::exception &) {}
            }
        }
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
        if (guardian_.state() != QProcess::NotRunning) {
            if (volumeRestored) guardian_.write("Q", 1);
            guardian_.closeWriteChannel();
            if (!guardian_.waitForFinished(2000)) guardian_.kill();
        }
        target_ = {};
        sinkId_ = -1;
        smart_ = false;
        legacyVolumeManaged_ = false;
        originalState_ = {};
    }

    ~AudioEngine() { stop(); }

private:
    QString effectControls(QString controls) const {
        QStringList values;constexpr const char *names[]{"Clarity","Ambience","Surround","Dynamic","Bass"};
        for(std::size_t i=0;i<5;++i)values << quote(QString("enhancements:")+names[i]) << QString::number(effects_.values[i],'g',8);
        controls.replace(" ] }"," "+values.join(' ')+" ] }");return controls;
    }
    soundcurrent::EnhancementSettings effects_;

    QTemporaryDir directory_{QDir::tempPath() + "/soundcurrent-eq-XXXXXX"};
    QProcess process_;
    QProcess guardian_;
    Device target_;
    SinkState originalState_;
    int sinkId_ = -1;
    bool smart_ = false;
    bool legacyVolumeManaged_ = false;
};

class MicrophoneEngine {
public:
    bool active() const { return process_.state() != QProcess::NotRunning; }
    QString target() const { return target_.name; }

    void start(const InputDevice &device, const MicTuning &adjustments, double gainDb) {
        stop();
        if (device.name.isEmpty() || device.channels < 1 || device.channels > 2)
            throw std::runtime_error(SC_TR("Unsupported microphone channel layout").toStdString());
        if (nodeId(kMicSource) >= 0) throw std::runtime_error(SC_TR("Another SoundCurrent microphone filter is running").toStdString());
        if (!directory_.isValid()) throw std::runtime_error(SC_TR("Could not create microphone configuration folder").toStdString());
        originalDefault_ = defaultSource();
        const bool smart = smartFiltersAvailable();
        QFile config(directory_.filePath("microphone.conf"));
        if (!config.open(QIODevice::WriteOnly | QIODevice::Truncate))
            throw std::runtime_error(SC_TR("Could not write microphone configuration").toStdString());
        config.write(micConfig(device.name, device.channels, adjustments, gainDb, smart).toUtf8());
        config.close();
        process_.setProgram("pipewire");
        process_.setArguments({"-c", config.fileName()});
        process_.setProcessChannelMode(QProcess::MergedChannels);
        process_.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGTERM); });
        process_.start();
        if (!process_.waitForStarted(2000)) throw std::runtime_error(SC_TR("Could not start microphone filter").toStdString());
        QElapsedTimer clock;
        clock.start();
        while (clock.elapsed() < 3000) {
            if (process_.state() == QProcess::NotRunning)
                throw std::runtime_error(QString::fromUtf8(process_.readAll()).trimmed().toStdString());
            try { if (nodeId(kMicSource) >= 0) break; } catch (const std::exception &) {}
            QThread::msleep(100);
        }
        const int id = nodeId(kMicSource);
        if (id < 0) { stop(); throw std::runtime_error(SC_TR("Microphone filter did not appear").toStdString()); }
        try {
            command("pactl", {"set-source-volume", kMicSource, "100%"});
            command("pactl", {"set-source-mute", kMicSource, "0"});
            target_ = device;
            smart_ = smart;
            sourceId_ = id;
            if (!smart) {
                command("pactl", {"set-default-source", kMicSource});
                if (originalDefault_ == device.name) moveCaptureStreams(device.index, kMicSource);
            } else if (originalDefault_ != device.name) {
                command("pactl", {"set-default-source", device.name});
                for (const auto &available : inputDevices())
                    if (available.name == originalDefault_)
                        moveCaptureStreams(available.index, device.name);
            }
        } catch (const std::exception &) { stop(); throw; }
    }

    void update(const MicTuning &adjustments, double gainDb) {
        if (!active()) return;
        if (sourceId_ < 0) throw std::runtime_error(SC_TR("Microphone filter disappeared").toStdString());
        command("pw-cli", {"set-param", QString::number(sourceId_), "Props",
                           micControls(adjustments, gainDb, target_.channels)});
    }

    void stop() {
        if (!target_.name.isEmpty()) {
            try {
                if (!smart_ && defaultSource() == kMicSource) {
                    auto restore = target_.name;
                    for (const auto &device : inputDevices())
                        if (device.name == originalDefault_) restore = device.name;
                    command("pactl", {"set-default-source", restore});
                    for (const auto &source : pactlList("sources")) {
                        const auto object = source.toObject();
                        if (object.value("name").toString() == kMicSource)
                            moveCaptureStreams(object.value("index").toInt(-1), restore);
                    }
                } else if (smart_ && defaultSource() == target_.name && originalDefault_ != target_.name) {
                    for (const auto &device : inputDevices())
                        if (device.name == originalDefault_) {
                            command("pactl", {"set-default-source", originalDefault_});
                            moveCaptureStreams(target_.index, originalDefault_);
                        }
                }
            } catch (const std::exception &) {}
        }
        if (active()) {
            process_.terminate();
            if (!process_.waitForFinished(2000)) {
                process_.kill();
                process_.waitForFinished(2000);
            }
        }
        target_ = {};
        smart_ = false;
        sourceId_ = -1;
        originalDefault_.clear();
    }

    ~MicrophoneEngine() { stop(); }

private:
    QTemporaryDir directory_{QDir::tempPath() + "/soundcurrent-mic-XXXXXX"};
    QProcess process_;
    InputDevice target_;
    QString originalDefault_;
    int sourceId_ = -1;
    bool smart_ = false;
};

#else
#include "windows_platform.inc"
#endif

constexpr int kCalibrationRate = 96000;
constexpr std::array<int, 12> kCalibrationFrequencies = {
    20, 40, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000, 20000
};
constexpr double kSweepFirstFrequency = 20.0;
constexpr double kSweepLastFrequency = 25000.0;
constexpr double kSweepHoldSeconds = 0.5;
constexpr double kSweepDurationSeconds = 10.0;

double toneAmplitude(const QByteArray &pcm, int frequency) {
    constexpr int sampleRate = kCalibrationRate;
    constexpr int window = kCalibrationRate / 4;
    const int samples = int(pcm.size() / 2);
    double maximum = 0.0;
    const double coefficient = 2.0 * std::cos(2.0 * std::numbers::pi * frequency / sampleRate);
    for (int start = 0; start + window <= samples; start += window / 2) {
        double previous = 0.0, beforePrevious = 0.0;
        for (int i = 0; i < window; ++i) {
            const int offset = 2 * (start + i);
            const auto low = static_cast<unsigned char>(pcm[offset]);
            const auto high = static_cast<unsigned char>(pcm[offset + 1]);
            const auto sample = static_cast<int16_t>(uint16_t(low | (high << 8)));
            const double current = sample + coefficient * previous - beforePrevious;
            beforePrevious = previous;
            previous = current;
        }
        const double power = previous * previous + beforePrevious * beforePrevious -
                             coefficient * previous * beforePrevious;
        maximum = std::max(maximum, 2.0 * std::sqrt(std::max(0.0, power)) / window);
    }
    return maximum;
}

void writeCalibrationTone(const QString &path, int frequency, int levelDb) {
    constexpr int rate = kCalibrationRate;
    constexpr int frames = rate;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw std::runtime_error(SC_TR("Could not create test tone").toStdString());
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + frames * 4);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(2) << quint32(rate)
        << quint32(rate * 4) << quint16(4) << quint16(16);
    out.writeRawData("data", 4);
    out << quint32(frames * 4);
    const double amplitude = 32767.0 * std::pow(10.0, levelDb / 20.0);
    for (int i = 0; i < frames; ++i) {
        const double envelope = std::min({1.0, i / 2400.0, (frames - i - 1) / 2400.0});
        const auto value = qint16(std::lround(amplitude * envelope *
                              std::sin(2.0 * std::numbers::pi * frequency * i / rate)));
        out << value << value;
    }
    if (out.status() != QDataStream::Ok) throw std::runtime_error(SC_TR("Could not write test tone").toStdString());
}

QVector<int16_t> writeCalibrationSweep(const QString &path, int levelDb) {
    constexpr int rate = kCalibrationRate;
    constexpr int frames = int(rate * (kSweepHoldSeconds + kSweepDurationSeconds));
    const double logarithm = std::log(kSweepLastFrequency / kSweepFirstFrequency);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw std::runtime_error(SC_TR("Could not create quiet frequency sweep").toStdString());
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.writeRawData("RIFF", 4);
    out << quint32(36 + frames * 4);
    out.writeRawData("WAVEfmt ", 8);
    out << quint32(16) << quint16(1) << quint16(2) << quint32(rate)
        << quint32(rate * 4) << quint16(4) << quint16(16);
    out.writeRawData("data", 4);
    out << quint32(frames * 4);
    const double amplitude = 32767.0 * std::pow(10.0, levelDb / 20.0);
    QVector<int16_t> reference;
    reference.reserve(frames);
    for (int i = 0; i < frames; ++i) {
        const double time = double(i) / rate;
        const double sweepTime = std::max(0.0, time - kSweepHoldSeconds);
        const double phase = 2.0 * std::numbers::pi * kSweepFirstFrequency *
                             (time < kSweepHoldSeconds ? time :
                              kSweepHoldSeconds + kSweepDurationSeconds / logarithm *
                              (std::exp(sweepTime * logarithm / kSweepDurationSeconds) - 1.0));
        const double envelope = std::min({1.0, i / double(rate / 10),
                                         (frames - i - 1) / double(rate / 10)});
        const auto value = int16_t(std::lround(amplitude * envelope * std::sin(phase)));
        reference.append(value);
        out << qint16(value) << qint16(value);
    }
    if (out.status() != QDataStream::Ok) throw std::runtime_error(SC_TR("Could not write frequency sweep").toStdString());
    return reference;
}

QVector<int16_t> pcmSamples(const QByteArray &pcm) {
    QVector<int16_t> samples;
    samples.reserve(pcm.size() / 2);
    for (int i = 0; i + 1 < pcm.size(); i += 2) {
        const auto low = static_cast<unsigned char>(pcm[i]);
        const auto high = static_cast<unsigned char>(pcm[i + 1]);
        samples.append(static_cast<int16_t>(uint16_t(low | (high << 8))));
    }
    return samples;
}

void checkCalibrationClipping(const QByteArray &pcm) {
    const auto samples = pcmSamples(pcm);
    const auto clipped = std::count_if(samples.begin(), samples.end(), [](int16_t value) {
        return std::abs(int(value)) >= 32760;
    });
    if (!samples.isEmpty() && double(clipped) / samples.size() >= 0.001)
        throw std::runtime_error(SC_TR("Microphone recording is clipping. Lower microphone gain or boost and repeat the measurement.").toStdString());
}

double sweepFrequencyAmplitude(const QVector<int16_t> &samples, int frequency,
                               int firstAllowed = 0, int lastAllowed = std::numeric_limits<int>::max()) {
    const int window = std::clamp(kCalibrationRate * 10 / frequency,
                                  kCalibrationRate / 100, kCalibrationRate * 8 / 100);
    const double coefficient = 2.0 * std::cos(2.0 * std::numbers::pi * frequency / kCalibrationRate);
    double maximum = 0.0;
    for (int start = std::max(0, firstAllowed);
         start + window <= samples.size() && start <= lastAllowed; start += window / 2) {
        double previous = 0.0, beforePrevious = 0.0;
        for (int i = 0; i < window; ++i) {
            const double current = samples[start + i] + coefficient * previous - beforePrevious;
            beforePrevious = previous;
            previous = current;
        }
        const double power = previous * previous + beforePrevious * beforePrevious -
                             coefficient * previous * beforePrevious;
        maximum = std::max(maximum, 2.0 * std::sqrt(std::max(0.0, power)) / window);
    }
    return maximum;
}

QJsonArray analyzeSweep(const QByteArray &pcm, const QByteArray &noise,
                        const QVector<int16_t> &reference) {
    const auto recorded = pcmSamples(pcm);
    const auto backgroundSamples = pcmSamples(noise);
    QJsonArray levels;
    for (const int frequency : kCalibrationFrequencies) {
        const double expectedSeconds = kSweepHoldSeconds +
            kSweepDurationSeconds * std::log(frequency / kSweepFirstFrequency) /
            std::log(kSweepLastFrequency / kSweepFirstFrequency);
        const int first = frequency == 20 ? int(0.12 * kCalibrationRate)
                                          : int((expectedSeconds - 0.15) * kCalibrationRate);
        const int last = frequency == 20 ? int(0.35 * kCalibrationRate)
                                         : int((expectedSeconds + 0.8) * kCalibrationRate);
        const double heard = sweepFrequencyAmplitude(recorded, frequency, first, last);
        const double background = sweepFrequencyAmplitude(backgroundSamples, frequency);
        const double expected = sweepFrequencyAmplitude(reference, frequency, first, last);
        if (qEnvironmentVariableIsSet("SOUNDCURRENT_CALIBRATION_DEBUG"))
            QTextStream(stderr) << frequency << " Hz: signal " << heard
                                << ", background " << background << Qt::endl;
        if (heard >= std::max(1.0, background * 3.2) && expected > 0.0)
            levels.append(std::sqrt(std::max(0.0, heard * heard - background * background)) / expected);
        else levels.append(QJsonValue::Null);
    }
    return levels;
}

int runCalibration(const QString &output, const QString &input, int levelDb, bool sweep) {
    try {
        if (levelDb < -54 || levelDb > -5) throw std::runtime_error(SC_TR("Test level is outside the allowed range").toStdString());
        bool outputFound = false, inputFound = false;
        for (const auto &device : devices()) if (device.name == output) outputFound = true;
#ifndef Q_OS_WIN
        if (output == kSink && nodeId(kSink) >= 0) outputFound = true;
#else
        if (output == kSink) { standardCable(false); outputFound = true; }
#endif
        for (const auto &device : inputDevices()) if (device.name == input) inputFound = true;
        if (!outputFound || !inputFound) throw std::runtime_error(SC_TR("Selected audio device is unavailable").toStdString());
        QTemporaryDir directory(QDir::tempPath() + "/soundcurrent-calibration-XXXXXX");
        if (!directory.isValid()) throw std::runtime_error(SC_TR("Could not create a private test folder").toStdString());
#ifndef Q_OS_WIN
        QProcess recorder;
        recorder.setProgram("parec");
        recorder.setArguments({"--raw", "-d", input, "--format=s16le", "--rate=96000",
                               "--channels=1", "--latency-msec=10", "--process-time-msec=5"});
        recorder.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGTERM); });
        recorder.start();
        if (!recorder.waitForStarted(2000)) throw std::runtime_error(SC_TR("Could not start microphone capture").toStdString());
        auto collect = [&recorder](int milliseconds) {
            QByteArray pcm;
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < milliseconds) {
                recorder.waitForReadyRead(20);
                pcm.append(recorder.readAllStandardOutput());
                if (recorder.state() == QProcess::NotRunning)
                    throw std::runtime_error(SC_TR("Microphone capture stopped during the test").toStdString());
            }
            return pcm;
        };
        collect(350);
        auto playAndRecord = [&](const QString &path) {
            QProcess player;
            player.setProgram("paplay");
            player.setArguments({"-d", output, path});
            player.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGTERM); });
            player.start();
            if (!player.waitForStarted(2000)) throw std::runtime_error(SC_TR("Could not play quiet test audio").toStdString());
            QByteArray recorded;
            while (player.state() != QProcess::NotRunning) {
                player.waitForFinished(20);
                recorded.append(recorder.readAllStandardOutput());
                if (recorder.state() == QProcess::NotRunning)
                    throw std::runtime_error(SC_TR("Microphone capture stopped during playback").toStdString());
            }
            recorded.append(collect(180));
            if (player.exitStatus() != QProcess::NormalExit || player.exitCode() != 0)
                throw std::runtime_error(SC_TR("Could not play test audio through the selected output").toStdString());
            return recorded;
        };
#else
        soundcurrent::WindowsRecorder recorder;
        recorder.start(input.toStdWString(), kCalibrationRate, 1);
        auto take = [&recorder]() {
            const auto pcm = recorder.take();
            return QByteArray(reinterpret_cast<const char *>(pcm.data()), qsizetype(pcm.size() * sizeof(std::int16_t)));
        };
        auto collect = [&](int milliseconds) {
            QByteArray pcm;
            QElapsedTimer clock; clock.start();
            while (clock.elapsed() < milliseconds) {
                QThread::msleep(10); pcm.append(take());
                if (!recorder.running()) throw std::runtime_error(recorder.error());
            }
            return pcm;
        };
        collect(350);
        auto playAndRecord = [&](const QString &path) {
            QFile wav(path);
            if (!wav.open(QIODevice::ReadOnly)) throw std::runtime_error(SC_TR("Could not open test waveform").toStdString());
            const auto bytes = wav.readAll().mid(44); // our own stereo PCM16 WAV writers
            std::vector<std::int16_t> signal(bytes.size() / 2);
            std::memcpy(signal.data(), bytes.constData(), signal.size() * 2);
            recorder.take();
            const auto endpoint = output == kSink ? standardCable(false) : output;
            auto player = std::async(std::launch::async, [endpoint, signal = std::move(signal)] {
                soundcurrent::windowsPlayPcm(endpoint.toStdWString(), signal, kCalibrationRate, 2);
            });
            QByteArray pcm;
            while (player.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
                pcm.append(take());
                if (!recorder.running()) throw std::runtime_error(recorder.error());
            }
            player.get();
            pcm.append(take()); pcm.append(collect(180));
            return pcm;
        };
#endif
        QJsonArray levels;
        int valid = 0;
        if (sweep) {
            QTextStream(stderr) << "Playing a logarithmic sweep from 20 Hz to 25 kHz" << Qt::endl;
            const auto noise = collect(1500);
            const auto path = directory.filePath("quiet-sweep.wav");
            const auto reference = writeCalibrationSweep(path, levelDb);
            const auto recorded = playAndRecord(path);
            checkCalibrationClipping(recorded);
            if (qEnvironmentVariableIsSet("SOUNDCURRENT_CALIBRATION_DEBUG"))
                QTextStream(stderr) << "Sweep capture bytes: " << recorded.size()
                                    << ", noise bytes: " << noise.size() << Qt::endl;
            levels = analyzeSweep(recorded, noise, reference);
            for (const auto &value : levels) if (value.isDouble()) ++valid;
        } else {
            for (const auto frequency : kCalibrationFrequencies) {
                QTextStream(stderr) << "Checking " << frequency << " Hz" << Qt::endl;
                const auto noise = collect(500);
                const auto path = directory.filePath(QString("tone-%1.wav").arg(frequency));
                writeCalibrationTone(path, frequency, levelDb);
                const auto recorded = playAndRecord(path);
                checkCalibrationClipping(recorded);
                if (qEnvironmentVariableIsSet("SOUNDCURRENT_CALIBRATION_DEBUG"))
                    QTextStream(stderr) << "Capture bytes: " << recorded.size()
                                        << ", noise bytes: " << noise.size() << Qt::endl;
                const double heard = toneAmplitude(recorded, frequency);
                const double background = toneAmplitude(noise, frequency);
                if (qEnvironmentVariableIsSet("SOUNDCURRENT_CALIBRATION_DEBUG"))
                    QTextStream(stderr) << frequency << " Hz: signal " << heard
                                        << ", background " << background << Qt::endl;
                if (heard >= std::max(1.0, background * 3.2)) {
                    levels.append(std::sqrt(std::max(0.0, heard * heard - background * background)));
                    ++valid;
                } else levels.append(QJsonValue::Null);
            }
        }
#ifndef Q_OS_WIN
        recorder.terminate();
        recorder.waitForFinished(1000);
#else
        recorder.stop();
#endif
        if (valid < 4) throw std::runtime_error(SC_TR("Too little test audio reached the microphone. Move it closer or raise the test level slightly.").toStdString());
        QJsonObject result{{"levels", levels}, {"testLevelDb", levelDb},
                           {"mode", sweep ? "sweep" : "tones"}};
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << Qt::endl;
        return 0;
    } catch (const std::exception &error) {
        QTextStream(stderr) << "Measurement failed: " << error.what() << Qt::endl;
        return 1;
    }
}

struct CalibrationSuggestion {
    Bands bands;
    QString preview;
    int changed = 0;
};

std::optional<CalibrationSuggestion> calibrationSuggestion(const QJsonObject &result,
                                                            const Bands &current) {
    const auto levels = result.value("levels").toArray();
    if (levels.size() != int(kCalibrationFrequencies.size()) || current.isEmpty()) return std::nullopt;
    QVector<double> db;
    for (const auto &value : levels)
        if (value.isDouble() && value.toDouble() > 0.0)
            db.append(20.0 * std::log10(value.toDouble()));
    if (db.size() < 4) return std::nullopt;
    std::sort(db.begin(), db.end());
    const double reference = db[db.size() / 2];
    CalibrationSuggestion suggestion{current, QString(), 0};
    QStringList rows;
    for (int i = 0; i < levels.size(); ++i) {
        if (!levels[i].isDouble() || levels[i].toDouble() <= 0.0) {
            rows << SC_TR("%1 Hz: too quiet to measure").arg(kCalibrationFrequencies[i]);
            continue;
        }
        const double relative = 20.0 * std::log10(levels[i].toDouble()) - reference;
        const double rawChange = std::clamp(-relative * 0.4, -3.0, 3.0);
        const double change = std::round(rawChange * 2.0) / 2.0;
        int nearest = 0;
        double distance = std::numeric_limits<double>::infinity();
        for (int band = 0; band < suggestion.bands.size(); ++band) {
            const double candidate = std::abs(std::log(suggestion.bands[band].frequency /
                                                        kCalibrationFrequencies[i]));
            if (candidate < distance) { distance = candidate; nearest = band; }
        }
        const double before = suggestion.bands[nearest].gain;
        suggestion.bands[nearest].gain = std::clamp(before + change, -12.0, 12.0);
        if (std::abs(suggestion.bands[nearest].gain - before) > 0.01) ++suggestion.changed;
        rows << SC_TR("%1 Hz: measured %2%3 dB; suggested %4%5 dB")
                    .arg(kCalibrationFrequencies[i])
                    .arg(relative > 0 ? "+" : "").arg(relative, 0, 'f', 1)
                    .arg(change > 0 ? "+" : "").arg(change, 0, 'f', 1);
    }
    suggestion.preview = rows.join('\n');
    return suggestion;
}

const QMap<QString, std::array<double, 9>> &builtinShapes() {
    static const QMap<QString, std::array<double, 9>> shapes = {
        {"Flat", {0, 0, 0, 0, 0, 0, 0, 0, 0}},
        {"Balanced", {1, 1, 0.5, 0, -0.5, 0, 0.5, 1, 1}},
        {"Loudness", {5, 5, 3, 1, 0, -1, 0, 2, 3}},
        {"Bass Boost", {5, 4, 3, 1.5, 0, 0, 0, 0, 0}},
        {"Deep Bass", {7, 6, 4, 2, 0, -1, -1, -1, -1}},
        {"Punchy Bass", {2, 3, 5, 4, 1, -1, 0, 1, 1}},
        {"Bass Cut", {-6, -5, -4, -2, 0, 0, 0, 0, 0}},
        {"Clear Voice", {-3, -2, -1, 0, 1, 2.5, 3, 1.5, 0}},
        {"Podcast", {-4, -3, -1, 0, 2, 3, 2.5, 0, -1}},
        {"TV Dialogue", {-4, -3, -2, 0, 1.5, 3.5, 4, 1, -1}},
        {"Vocal Focus", {-2, -1, 0, 1, 2, 3, 3, 1, 0}},
        {"Warm", {2.5, 2, 1.5, 0.5, 0, -0.5, -1, -1, -1.5}},
        {"Bright", {-1, -1, -0.5, 0, 0.5, 1, 2, 2.5, 2.5}},
        {"Soft Treble", {0, 0, 0, 0, 0, -0.5, -1.5, -3, -4}},
        {"Treble Detail", {-1, -1, -1, 0, 0, 1, 2.5, 4, 3}},
        {"Movies", {3, 2.5, 1.5, 0, -1, 0, 1, 2, 2}},
        {"Gaming", {3, 2, 0, -2, -1, 1, 3, 2, 0}},
        {"FPS Footsteps", {-5, -4, -3, -2, 0, 2, 4, 3, 1}},
        {"Night Listening", {-6, -5, -3, 0, 2, 3, 1, -3, -5}},
        {"Small Speakers", {-4, -2, 0, 2, 2, 1, 1, 0, -1}},
        {"Headphones", {1, 1, 0, -1, -1.5, 0, 1.5, 2, 1}},
        {"Rock", {3, 2, 1, -1, -2, 0, 2, 3, 2}},
        {"Pop", {2, 2, 1, 0, 1, 2, 2, 2, 1}},
        {"Jazz", {2, 1.5, 1, 0, -1, 0, 1.5, 2, 1}},
        {"Classical", {1, 1, 0, -1, -1, 0, 1, 2, 2}},
        {"Electronic", {4, 4, 3, 0, -2, 0, 2, 3, 3}},
        {"Dance", {4, 4, 2, 0, -1, 0, 2, 3, 2}},
        {"Hip-Hop", {5, 5, 3, 1, -1, 0, 1, 1, 0}},
        {"R&B", {3, 3, 2, 1, 0, 1, 2, 1, 0}},
        {"Acoustic", {1, 1, 0, 1, 2, 2, 1, 1, 0}},
        {"Piano", {0, 0, 0, 1, 2, 2, 1, 0, -1}},
        {"Metal", {4, 3, 1, -2, -2, 1, 3, 2, 1}},
        {"Lo-Fi", {2, 2, 1, 0, -1, -2, -3, -5, -6}},
        {"Live", {2, 1, 0, -1, -1, 1, 2, 2, 1}},
    };
    return shapes;
}

Bands builtinProfile(const QString &name, int count) {
    Bands anchors;
    const auto shape = builtinShapes().value(name);
    for (size_t i = 0; i < shape.size(); ++i) anchors.append({double(kLegacyFrequencies[i]), shape[i], 1.0});
    return remapBands(anchors, count);
}

QJsonArray serializeBands(const Bands &bands) {
    QJsonArray result;
    for (const auto &band : bands)
        result.append(QJsonObject{{"frequency", band.frequency}, {"gain", band.gain}, {"q", band.q}});
    return result;
}

std::optional<Bands> parseBands(const QJsonValue &value) {
    const auto array = value.isObject() ? value.toObject().value("bands").toArray() : value.toArray();
    if (value.isArray() && array.size() == 9) {
        Bands legacy;
        for (int i = 0; i < 9; ++i) {
            if (!array[i].isDouble() || array[i].toDouble() < -12 || array[i].toDouble() > 12) return std::nullopt;
            legacy.append({double(kLegacyFrequencies[i]), array[i].toDouble(), 1.0});
        }
        return legacy;
    }
    if (array.size() < kMinBands || array.size() > kMaxBands) return std::nullopt;
    Bands result;
    for (const auto &item : array) {
        if (!item.isObject()) return std::nullopt;
        const auto band = item.toObject();
        const auto frequency = band.value("frequency");
        const auto gain = band.value("gain");
        const auto q = band.value("q");
        if (!frequency.isDouble() || !gain.isDouble() || !q.isDouble() ||
            frequency.toDouble() < 20 || frequency.toDouble() > 20000 ||
            gain.toDouble() < -12 || gain.toDouble() > 12 ||
            q.toDouble() < 0.3 || q.toDouble() > 10 ||
            (!result.isEmpty() && frequency.toDouble() <= result.last().frequency)) return std::nullopt;
        result.append({frequency.toDouble(), gain.toDouble(), q.toDouble()});
    }
    return result;
}

QString presetsPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/presets.json";
}

struct AmplifierProfile {
    QString id, name, source, conditions;
    Bands filters;
    QJsonObject json;
};
std::optional<AmplifierProfile> parseAmplifierProfile(const QJsonObject &json) {
    if (json.value("schema").toInt() != 1) return std::nullopt;
    AmplifierProfile profile;
    profile.name = json.value("model").toString().trimmed();
    profile.source = json.value("measurementSource").toString();
    profile.conditions = json.value("conditions").toString().trimmed();
    const QUrl source(profile.source);
    if (profile.name.isEmpty() || profile.name.size() > 120 || profile.conditions.isEmpty() ||
        profile.conditions.size() > 500 || profile.source.size() > 2048 ||
        !source.isValid() || source.scheme() != "https" || source.host().isEmpty()) return std::nullopt;
    for (const auto &value : json.value("filters").toArray()) {
        const auto f = value.toObject();
        const auto type = f.value("type").toString();
        using T = soundcurrent::FilterType;
        if (type != "PK" && type != "LS" && type != "HS") return std::nullopt;
        Band band{f.value("frequency").toDouble(-1), f.value("gain").toDouble(999), f.value("q").toDouble(-1),
                  type == "LS" ? T::LowShelf : type == "HS" ? T::HighShelf : T::Peaking};
        if (!std::isfinite(band.frequency) || !std::isfinite(band.gain) || !std::isfinite(band.q) ||
            band.frequency < 20 || band.frequency > 20000 || std::abs(band.gain) > 6 || band.q < 0.1 || band.q > 6)
            return std::nullopt;
        profile.filters.append(band);
    }
    if (profile.filters.isEmpty() || profile.filters.size() > 16) return std::nullopt;
    profile.json = json;
    profile.id = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(json).toJson(QJsonDocument::Compact),
                                                            QCryptographicHash::Sha256).toHex());
    return profile;
}
QString amplifierProfilesPath() {
    if (qApp->arguments().contains("--ui-self-test")) return QFileInfo(QSettings().fileName()).absolutePath() + "/amplifiers.json";
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/amplifiers.json";
}

class CurveWidget : public QWidget {
public:
    std::function<void(int)> onSelect;
    std::function<void(int, double, double)> onMove;

    explicit CurveWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumHeight(150);
        setAccessibleName(SC_TR("Equalizer curve. Select a point or drag it to adjust frequency and gain."));
        setMouseTracking(true);
    }

    void setBands(const Bands &bands, int selected) {
        bands_ = bands;
        selected_ = selected;
        update();
    }

    void setCorrection(const Bands &bands) { correction_ = bands; update(); }

    void setLocked(bool locked) { locked_ = locked; dragging_ = -1; }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setLayoutDirection(Qt::LeftToRight);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#172337"));
        const QRectF plot(38, 12, width() - 54, height() - 31);
        painter.setPen(QPen(QColor("#334862"), 1));
        for (const auto gain : {-12.0, 0.0, 12.0}) {
            const auto y = yForGain(gain, plot);
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            painter.setPen(QColor("#8fa2bb"));
            painter.drawText(QRectF(1, y - 9, 33, 18), Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(gain, 'g', 2));
            painter.setPen(QPen(QColor("#334862"), 1));
        }
        for (const auto frequency : {100.0, 1000.0, 10000.0}) {
            const auto x = xForFrequency(frequency, plot);
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            painter.setPen(QColor("#8fa2bb"));
            painter.drawText(QRectF(x - 22, plot.bottom() + 2, 44, 17), Qt::AlignCenter,
                             frequency >= 1000 ? QString::number(frequency / 1000, 'g', 2) + "k"
                                               : QString::number(frequency, 'g', 3));
            painter.setPen(QPen(QColor("#334862"), 1));
        }
        if (bands_.isEmpty()) return;
        QPainterPath curve;
        for (int x = 0; x <= int(plot.width()); x += 3) {
            const auto frequency = frequencyForX(plot.left() + x, plot);
            const auto y = yForGain(std::clamp(responseDb(bands_, frequency) + responseDb(correction_, frequency), -12.0, 12.0), plot);
            if (x == 0) curve.moveTo(plot.left(), y);
            else curve.lineTo(plot.left() + x, y);
        }
        painter.setPen(QPen(QColor("#55d7c3"), 2.5));
        painter.drawPath(curve);
        for (qsizetype i = 0; i < bands_.size(); ++i) {
            const auto point = QPointF(xForFrequency(bands_[i].frequency, plot), yForGain(bands_[i].gain, plot));
            painter.setPen(QPen(i == selected_ ? QColor("#ffffff") : QColor("#55d7c3"), 2));
            painter.setBrush(i == selected_ ? QColor("#55d7c3") : QColor("#172337"));
            painter.drawEllipse(point, i == selected_ ? 6.5 : 4.5, i == selected_ ? 6.5 : 4.5);
        }
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton || bands_.isEmpty()) return;
        const QRectF plot(38, 12, width() - 54, height() - 31);
        int closest = -1;
        double distance = 22;
        for (qsizetype i = 0; i < bands_.size(); ++i) {
            const auto point = QPointF(xForFrequency(bands_[i].frequency, plot), yForGain(bands_[i].gain, plot));
            const auto candidate = std::hypot(event->position().x() - point.x(), event->position().y() - point.y());
            if (candidate < distance) { distance = candidate; closest = int(i); }
        }
        if (closest >= 0) {
            dragging_ = closest;
            if (onSelect) onSelect(closest);
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (locked_ || dragging_ < 0 || !onMove) return;
        const QRectF plot(38, 12, width() - 54, height() - 31);
        auto frequency = frequencyForX(event->position().x(), plot);
        const auto minimum = dragging_ > 0 ? bands_[dragging_ - 1].frequency * 1.02 : 20.0;
        const auto maximum = dragging_ + 1 < bands_.size() ? bands_[dragging_ + 1].frequency / 1.02 : 20000.0;
        frequency = std::clamp(frequency, minimum, maximum);
        const auto gain = std::clamp(std::round(gainForY(event->position().y(), plot) * 2) / 2, -12.0, 12.0);
        onMove(dragging_, std::round(frequency), gain);
    }

    void mouseReleaseEvent(QMouseEvent *) override { dragging_ = -1; }

private:
    static double xForFrequency(double frequency, const QRectF &plot) {
        return plot.left() + std::log(frequency / 20.0) / std::log(1000.0) * plot.width();
    }
    static double frequencyForX(double x, const QRectF &plot) {
        return 20.0 * std::pow(1000.0, std::clamp((x - plot.left()) / plot.width(), 0.0, 1.0));
    }
    static double yForGain(double gain, const QRectF &plot) {
        return plot.center().y() - gain / 12.0 * plot.height() / 2.0;
    }
    static double gainForY(double y, const QRectF &plot) {
        return (plot.center().y() - y) / (plot.height() / 2.0) * 12.0;
    }
    Bands bands_;
    Bands correction_;
    int selected_ = 0;
    int dragging_ = -1;
    bool locked_ = false;
};

class BandLevelMeter : public QWidget {
public:
    explicit BandLevelMeter(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedWidth(11);
        setMinimumHeight(140);
    }

    void setLevel(double db) {
        const double elapsed = peakClock_.isValid() ? peakClock_.restart() / 1000.0 : 0.0;
        if (!peakClock_.isValid()) peakClock_.start();
        levelDb_ = std::clamp(db, -60.0, 12.0);
        peakDb_ = std::max(levelDb_, peakDb_ - 24.0 * elapsed);
        update();
    }

    void setPeakMarkersEnabled(bool enabled) {
        peakMarkersEnabled_ = enabled;
        update();
    }

    void reset() {
        levelDb_ = -60.0;
        peakDb_ = -60.0;
        peakClock_.invalidate();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setLayoutDirection(Qt::LeftToRight);
        painter.fillRect(rect(), QColor("#30425c"));
        const auto colorFor = [](double db) {
            return QColor(db >= -3.0 ? "#f16b76" : db >= -12.0 ? "#e6b450" : "#50d1ba");
        };
        const auto heightFor = [this](double db) {
            return std::clamp(int(std::lround((db + 60.0) * height() / 60.0)), 0, height());
        };
        const int filled = heightFor(levelDb_);
        if (filled > 0) painter.fillRect(0, height() - filled, width(), filled, colorFor(levelDb_));
        if (peakMarkersEnabled_ && peakDb_ > -60.0) {
            const int y = std::clamp(height() - heightFor(peakDb_), 1, height() - 1);
            painter.setPen(QPen(QColor("#f4f8ff"), 2));
            painter.drawLine(0, y, width() - 1, y);
        }
    }

private:
    QElapsedTimer peakClock_;
    double levelDb_ = -60.0;
    double peakDb_ = -60.0;
    bool peakMarkersEnabled_ = false;
};

class OverallLevelMeter : public QWidget {
public:
    explicit OverallLevelMeter(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumSize(180, 16);
        setMaximumHeight(16);
    }

    void setLevel(double db) {
        const double elapsed = peakClock_.isValid() ? peakClock_.restart() / 1000.0 : 0.0;
        if (!peakClock_.isValid()) peakClock_.start();
        levelDb_ = std::clamp(db, -60.0, 12.0);
        peakDb_ = std::max(levelDb_, peakDb_ - 24.0 * elapsed);
        update();
    }

    void setPeakMarkersEnabled(bool enabled) { peakMarkersEnabled_ = enabled; update(); }

    void reset() {
        levelDb_ = peakDb_ = -60.0;
        peakClock_.invalidate();
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setLayoutDirection(Qt::LeftToRight);
        painter.fillRect(rect(), QColor("#30425c"));
        const auto xFor = [this](double db) {
            return std::clamp(int(std::lround((db + 60.0) * width() / 60.0)), 0, width());
        };
        const int filled = xFor(levelDb_);
        const int normalEnd = std::min(filled, xFor(-12.0));
        const int warningEnd = std::min(filled, xFor(-3.0));
        if (normalEnd > 0) painter.fillRect(0, 0, normalEnd, height(), QColor("#50d1ba"));
        if (warningEnd > normalEnd)
            painter.fillRect(normalEnd, 0, warningEnd - normalEnd, height(), QColor("#e6b450"));
        if (filled > warningEnd)
            painter.fillRect(warningEnd, 0, filled - warningEnd, height(), QColor("#f16b76"));
        if (peakMarkersEnabled_ && peakDb_ > -60.0) {
            const int x = std::clamp(xFor(peakDb_), 1, width() - 1);
            painter.setPen(QPen(QColor("#f4f8ff"), 2));
            painter.drawLine(x, 0, x, height() - 1);
        }
    }

private:
    QElapsedTimer peakClock_;
    double levelDb_ = -60.0;
    double peakDb_ = -60.0;
    bool peakMarkersEnabled_ = false;
};

class SpectrumMonitor : public QObject {
public:
    std::function<void(const QVector<double> &, double)> onLevels;
    bool active() const {
#ifdef Q_OS_WIN
        return timer_.isActive();
#else
        return process_.state() != QProcess::NotRunning;
#endif
    }

    explicit SpectrumMonitor(bool testing = false) : testing_(testing) {
        timer_.setTimerType(Qt::PreciseTimer);
        timer_.setInterval(16);
        connect(&process_, &QProcess::readyReadStandardOutput, this, [this] {
            appendPcm(process_.readAllStandardOutput());
        });
        connect(&timer_, &QTimer::timeout, this, [this] {
#ifdef Q_OS_WIN
            if (playbackMeterSource) {
                const auto pcm = playbackMeterSource->takeMeterPcm();
                appendPcm(QByteArray(reinterpret_cast<const char *>(pcm.data()), qsizetype(pcm.size() * 2)));
            }
#endif
            analyze();
        });
    }

    void setInterval(int milliseconds) { timer_.setInterval(std::clamp(milliseconds, 1, 100)); }
    int interval() const { return timer_.interval(); }

    void setProfile(const Bands &bands, double outputGainDb, int balancePercent = 0, const Bands &correction = {}) {
        bands_ = bands;
        Bands effective = bands; effective.append(correction);
        outputGainDb_ = outputGainDb;
        headroomDb_ = headroom(effective);
        balanceFactors_ = balanceFactors(balancePercent);
        bandEdges_.clear();
        bandGains_.clear();
        for (qsizetype i = 0; i < bands_.size(); ++i) {
            if (i + 1 < bands_.size())
                bandEdges_.append(std::sqrt(bands_[i].frequency * bands_[i + 1].frequency));
            bandGains_.append(std::pow(10.0, (responseDb(effective, bands_[i].frequency) +
                                                   headroomDb_ + outputGainDb_) / 20.0));
        }
        const auto maxEqBoost = headroomDb_ < 0.0 ? -headroomDb_ - 1.0 : 0.0;
        peakGain_ = std::pow(10.0, (maxEqBoost + headroomDb_ + outputGainDb_) / 20.0);
#ifdef Q_OS_WIN
        if (!testing_) {
            std::fill(bandGains_.begin(), bandGains_.end(), 1.0);
            balanceFactors_ = {1.0, 1.0}; peakGain_ = 1.0;
        }
#endif
    }

    void analyzePcmForTest(const QByteArray &pcm) {
        appendPcm(pcm);
        analyze();
    }

    void start() {
        stop();
#ifndef Q_OS_WIN
        process_.setProgram("parec");
        process_.setArguments({"--raw", "-d", QString(kSink) + ".monitor", "--format=s16le",
                               "--rate=48000", "--channels=2", "--latency-msec=10",
                               "--process-time-msec=5"});
        process_.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGTERM); });
        process_.start();
#endif
        timer_.start();
    }

    void stop() {
        timer_.stop();
        if (process_.state() != QProcess::NotRunning) {
            process_.terminate();
            if (!process_.waitForFinished(500)) process_.kill();
        }
        pcm_.clear();
        bytesSinceAnalysis_ = 0;
        if (onLevels) onLevels(QVector<double>(bands_.size(), 0.0), 0.0);
    }

private:
    bool testing_ = false;
    void appendPcm(const QByteArray &pcm) {
        pcm_.append(pcm);
        bytesSinceAnalysis_ += pcm.size();
        if (pcm_.size() > 131072) {
            const auto excess = pcm_.size() - 131072;
            pcm_.remove(0, (excess + 3) & ~3);
        }
    }

    void analyze() {
        constexpr int n = 4096;
        constexpr int frameBytes = 4;
        if (pcm_.size() < n * frameBytes || bytesSinceAnalysis_ < frameBytes) return;
        const auto *data = reinterpret_cast<const unsigned char *>(pcm_.constData());
        const int frameCount = pcm_.size() / frameBytes;
        std::array<double, 2> peak = {0.0, 0.0};
        const int firstNewFrame = std::max(0, frameCount - int(bytesSinceAnalysis_ / frameBytes));
        for (int i = firstNewFrame; i < frameCount; ++i) {
            for (int channel = 0; channel < 2; ++channel) {
                const auto offset = frameBytes * i + 2 * channel;
                const auto raw = uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
                const auto sample = int16_t(raw) / 32768.0;
                peak[channel] = std::max(peak[channel], std::abs(sample));
            }
        }
        const int first = (frameCount - n) * frameBytes;
        static const auto window = [] {
            std::array<double, n> values{};
            for (int i = 0; i < n; ++i)
                values[i] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * i / (n - 1));
            return values;
        }();
        std::array<std::complex<double>, n> spectrum;
        for (int i = 0; i < n; ++i) {
            const int offset = first + i * frameBytes;
            const auto left = int16_t(uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8));
            const auto right = int16_t(uint16_t(data[offset + 2]) | (uint16_t(data[offset + 3]) << 8));
            spectrum[i] = (double(left) + double(right)) / 65536.0 * window[i];
        }
        if (pcm_.size() > n * frameBytes)
            pcm_.remove(0, (pcm_.size() - n * frameBytes) & ~qsizetype(3));
        bytesSinceAnalysis_ = 0;
        for (int i = 1, j = 0; i < n; ++i) {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(spectrum[i], spectrum[j]);
        }
        for (int length = 2; length <= n; length <<= 1) {
            const auto step = std::polar(1.0, -2.0 * std::numbers::pi / length);
            for (int start = 0; start < n; start += length) {
                std::complex<double> factor{1.0, 0.0};
                for (int j = 0; j < length / 2; ++j) {
                    const auto even = spectrum[start + j];
                    const auto odd = spectrum[start + j + length / 2] * factor;
                    spectrum[start + j] = even + odd;
                    spectrum[start + j + length / 2] = even - odd;
                    factor *= step;
                }
            }
        }
        QVector<double> levels(bands_.size(), 0.0);
        if (!bands_.isEmpty()) {
            int band = 0;
            for (int bin = 1; bin < n / 2; ++bin) {
                const auto frequency = 48000.0 * bin / n;
                while (band < bandEdges_.size() && frequency > bandEdges_[band])
                    ++band;
                levels[band] = std::max(levels[band], std::abs(spectrum[bin]) * 4.0 / n);
            }
            for (qsizetype i = 0; i < bands_.size(); ++i) levels[i] *= bandGains_[i];
        }
        const auto estimatedPeak = std::max(peak[0] * balanceFactors_[0],
                                            peak[1] * balanceFactors_[1]) * peakGain_;
#ifdef Q_OS_WIN
        if (onLevels) onLevels(levels, playbackMeterSource ? std::max(estimatedPeak, double(playbackMeterSource->peak())) : estimatedPeak);
#else
        if (onLevels) onLevels(levels, estimatedPeak);
#endif
    }

    QProcess process_;
    QTimer timer_;
    QByteArray pcm_;
    qsizetype bytesSinceAnalysis_ = 0;
    Bands bands_;
    QVector<double> bandEdges_;
    QVector<double> bandGains_;
    double headroomDb_ = 0.0;
    double outputGainDb_ = 0.0;
    double peakGain_ = 1.0;
    std::array<double, 2> balanceFactors_ = {1.0, 1.0};
};

class PresetComboBox : public QComboBox {
protected:
    void showPopup() override {
        QComboBox::showPopup();
        auto *popup = view()->window();
        const auto rowHeight = std::max(18, view()->sizeHintForRow(0));
        const auto height = std::min(count(), maxVisibleItems()) * rowHeight + 16;
        popup->setFixedHeight(height);
        if (auto *screen = QGuiApplication::screenAt(mapToGlobal(rect().center()))) {
            const auto area = screen->availableGeometry();
            auto position = mapToGlobal(rect().bottomLeft());
            if (position.y() + height > area.bottom())
                position.setY(mapToGlobal(rect().topLeft()).y() - height);
            popup->move(std::clamp(position.x(), area.left(), area.right() - popup->width()),
                        std::clamp(position.y(), area.top(), area.bottom() - height));
        }
    }
};

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(bool startEnabled = true) : bands_(builtinProfile("Flat", kDefaultBands)) {
        setWindowTitle("SoundCurrent EQ");
        setWindowIcon(QIcon::fromTheme("io.github.rhamenator.SoundCurrentEQ", QIcon(":/app.ico")));
        setMinimumSize(480, 320);
        if (auto *display = QGuiApplication::primaryScreen()) {
            const auto available = display->availableGeometry();
            resize(std::min(1050, std::max(480, available.width() - 48)),
                   std::min(1060, std::max(320, available.height() - 48)));
        } else resize(1050, 920);

        scroll_ = new QScrollArea;
        scroll_->setLayoutDirection(Qt::LeftToRight);
        scroll_->setWidgetResizable(true);
        tabs_ = new QTabWidget;
        tabs_->setAccessibleName(SC_TR("Equalizer and configuration pages"));
        tabs_->addTab(scroll_, SC_TR("Equalizer"));
        setCentralWidget(tabs_);
        auto *container = new QWidget;
        auto *root = new QVBoxLayout(container);
        root->setContentsMargins(26, 22, 26, 24);
        root->setSpacing(16);
        scroll_->setWidget(container);
        auto *settingsScroll = new QScrollArea;
        settingsScroll->setWidgetResizable(true);
        auto *settingsContainer = new QWidget;
        auto *settingsRoot = new QVBoxLayout(settingsContainer);
        settingsRoot->setContentsMargins(26, 22, 26, 24);
        settingsRoot->setSpacing(16);
        settingsScroll->setWidget(settingsContainer);
        tabs_->addTab(settingsScroll, SC_TR("Settings && calibration"));
        tabs_->setCurrentIndex(0);
        settingsRoot->addWidget(soundcurrent::i18n::settingsPanel());

        auto *deviceBox = new QGroupBox(SC_TR("Output device"));
        auto *deviceLayout = new QVBoxLayout(deviceBox);
        settingsRoot->addWidget(deviceBox);
        auto *outputBox = new QGroupBox(SC_TR("Playback"));
        auto *outputLayout = new QVBoxLayout(outputBox);
        auto *outputRow = new QHBoxLayout;
        outputCombo_ = new QComboBox;
        outputCombo_->setAccessibleName(SC_TR("Output device"));
        outputCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto *deviceRow = new QHBoxLayout;
        deviceRow->addWidget(outputCombo_, 1);
        auto *refresh = new QPushButton(SC_TR("Refresh devices"));
        deviceRow->addWidget(refresh);
        deviceLayout->addLayout(deviceRow);
        power_ = new QCheckBox(SC_TR("Equalizer off"));
        power_->setObjectName("powerToggle");
        power_->setAccessibleName(SC_TR("Equalizer on or off"));
        power_->setToolTip(SC_TR("Click to turn the equalizer on or off"));
        outputRow->addWidget(power_);
        outputRow->addStretch();
        auto *quit = new QPushButton(SC_TR("Quit app"));
        quit->setAccessibleName(SC_TR("Quit SoundCurrent EQ"));
        quit->setToolTip(SC_TR("Exit SoundCurrent EQ and restore normal audio"));
        outputRow->addWidget(quit);
        outputLayout->addLayout(outputRow);
        outputCombo_->setLayoutDirection(Qt::LeftToRight);
        auto *gainRow = new QHBoxLayout;
        gainRow->addWidget(new QLabel(SC_TR("Post gain")));
        outputGain_ = new QSlider(Qt::Horizontal);
        outputGain_->setLayoutDirection(Qt::LeftToRight);
        outputGain_->setRange(int(soundcurrent::kMinPostGainDb * 2), 24);
        outputGain_->setSingleStep(1);
        outputGain_->setPageStep(2);
        outputGain_->setTickPosition(QSlider::TicksBelow);
        outputGain_->setTickInterval(12);
        outputGain_->setAccessibleName(SC_TR("Post gain after equalization"));
        outputGain_->setToolTip(SC_TR("Adjust the output from -60 to +12 dB after the EQ. Higher gain can cause clipping."));
        const double savedGain = QSettings().value("outputGainDb", 0.0).toDouble();
        outputGain_->setValue(std::isfinite(savedGain)
                                  ? std::lround(std::clamp(savedGain, soundcurrent::kMinPostGainDb, 12.0) * 2.0) : 0);
        gainRow->addWidget(outputGain_, 1);
        outputGainValue_ = new QLabel;
        outputGainValue_->setMinimumWidth(58);
        outputGainValue_->setAccessibleName(SC_TR("Post gain value in decibels"));
        outputGainValue_->setText(QString(SC_TR("%1%2 dB")).arg(outputGainDb() > 0 ? "+" : "")
                                      .arg(QLocale().toString(outputGainDb(), 'f', 1)));
        gainRow->addWidget(outputGainValue_);
        gainRow->addSpacing(18);
        gainRow->addWidget(new QLabel(SC_TR("Balance")));
        gainRow->addWidget(new QLabel(SC_TR("L")));
        balance_ = new QSlider(Qt::Horizontal);
        balance_->setLayoutDirection(Qt::LeftToRight);
        balance_->setRange(-100, 100);
        balance_->setSingleStep(1);
        balance_->setPageStep(10);
        balance_->setTickPosition(QSlider::TicksBelow);
        balance_->setTickInterval(50);
        balance_->setAccessibleName(SC_TR("Left right balance"));
        balance_->setToolTip(SC_TR("Move toward L or R to reduce the opposite channel; center keeps both at full level"));
        balance_->setValue(std::clamp(QSettings().value("balancePercent", 0).toInt(), -100, 100));
        gainRow->addWidget(balance_, 1);
        gainRow->addWidget(new QLabel(SC_TR("R")));
        balanceValue_ = new QLabel;
        balanceValue_->setMinimumWidth(62);
        balanceValue_->setAccessibleName(SC_TR("Balance position"));
        balanceValue_->setText(balance_->value() == 0 ? SC_TR("Center")
                               : QString("%1 %2%").arg(balance_->value() < 0 ? SC_TR("L") : SC_TR("R"))
                                     .arg(std::abs(balance_->value())));
        gainRow->addWidget(balanceValue_);
        outputLayout->addLayout(gainRow);
        auto *meterRow = new QHBoxLayout;
        meterRow->addWidget(new QLabel(SC_TR("Overall output")));
        overallLevel_ = new OverallLevelMeter;
        overallLevel_->setAccessibleName(SC_TR("Estimated overall output level"));
        overallLevel_->setToolTip(SC_TR("Estimated post-EQ output peak, including post gain and balance"));
        meterRow->addWidget(overallLevel_, 1);
        meterRow->addSpacing(8);
        peakStatus_ = new QLabel(SC_TR("Estimated peak: waiting for audio"));
        peakStatus_->setAccessibleName(SC_TR("Estimated output peak and clipping risk"));
        peakStatus_->setObjectName("peakStatus");
        meterRow->addWidget(peakStatus_);
        meterRow->addSpacing(8);
        meterRow->addStretch();
        meterRow->addWidget(new QLabel(SC_TR("Level refresh")));
        levelRefresh_ = new soundcurrent::AcceleratingSpinBox;
        levelRefresh_->setRange(1, 100);
        levelRefresh_->setSingleStep(1);
        levelRefresh_->setSuffix(" ms");
        levelRefresh_->setAccessibleName(SC_TR("Level indicator refresh interval"));
        levelRefresh_->setToolTip(SC_TR("Shorter intervals update levels more often and use more CPU; audio delivery may limit the actual rate"));
        levelRefresh_->setValue(std::clamp(QSettings().value("levelRefreshMs", 16).toInt(), 1, 100));
        meterRow->addWidget(levelRefresh_);
        peakMarkers_ = new QCheckBox(SC_TR("Peak markers"));
        peakMarkers_->setAccessibleName(SC_TR("Show peak markers on frequency levels"));
        peakMarkers_->setToolTip(SC_TR("Show a falling peak hold line on each frequency level"));
        peakMarkers_->setChecked(QSettings().value("showPeakMarkers", false).toBool());
        meterRow->addWidget(peakMarkers_);
        overallLevel_->setPeakMarkersEnabled(peakMarkers_->isChecked());
        outputLayout->addLayout(meterRow);
        status_ = new QLabel(SC_TR("Equalizer is off. Your audio uses its normal output."));
        status_->setWordWrap(true);
        status_->setObjectName("status");
        outputLayout->addWidget(status_);
#ifdef Q_OS_WIN
        auto *driverSetup = new QPushButton(SC_TR("Audio driver setup"));
        driverSetup->setObjectName("audioDriverSetup");
        deviceLayout->addWidget(driverSetup);
        auto *cableSettings = new QPushButton(SC_TR("VB-CABLE settings"));
        cableSettings->setObjectName("cableSettings");
        cableSettings->setEnabled(QFileInfo::exists(QDir(QCoreApplication::applicationDirPath()).filePath("cable-setup.ps1")));
        cableSettings->setAccessibleName(SC_TR("Open VB-CABLE control panel"));
        cableSettings->setToolTip(SC_TR("Open VB-Audio's control panel for cable latency and internal sample rate. Changing these while audio is running can interrupt playback."));
        deviceLayout->addWidget(cableSettings);
        driverSetup->setToolTip(SC_TR("Pause processing and open audio setup. The app stays open and reports the result. Restart Windows after installing the driver."));
        auto *audioSetup = new QProcess(this);
        audioSetup->setProcessChannelMode(QProcess::MergedChannels);
        auto output = std::make_shared<QByteArray>();
        auto action = std::make_shared<bool>(false);
        auto finish = [this, driverSetup, cableSettings, output](const QString &message, bool error) {
            centralWidget()->setEnabled(true);
            if (trayToggle_) trayToggle_->setEnabled(true);
            driverSetup->setEnabled(true); cableSettings->setEnabled(true);
            if (message.isEmpty()) return;
            status_->setText(message);
            if (error) QMessageBox::warning(this, SC_TR("Audio setup could not finish"), message);
            else QMessageBox::information(this, SC_TR("Audio setup"), message);
            output->clear();
        };
        connect(audioSetup, &QProcess::readyReadStandardOutput, this, [audioSetup, output] {
            output->append(audioSetup->readAllStandardOutput());
            if (output->size() > 65536) output->remove(0, output->size() - 65536);
        });
        connect(audioSetup, &QProcess::errorOccurred, this, [audioSetup, finish](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
                finish("Could not start audio setup: " + audioSetup->errorString() + ". The app remains open.", true);
        });
        connect(audioSetup, &QProcess::finished, this, [audioSetup, output, action, finish](int code, QProcess::ExitStatus exitStatus) {
            output->append(audioSetup->readAllStandardOutput());
            auto message = QString::fromLocal8Bit(*output).trimmed();
            if (exitStatus != QProcess::NormalExit || (code != 0 && code != 3010)) {
                if (message.isEmpty()) message = "Audio setup failed. Restart Windows if VB-CABLE was just installed, then try again.";
                finish(message + "\nThe app remains open; your settings have been kept.", true);
            } else if (code == 3010) {
                finish(message.isEmpty() ? "Restart Windows before using the equalizer or VB-CABLE settings. Audio driver changes need a system restart." : message, false);
            } else if (*action && !message.isEmpty()) finish(message, false);
            else finish({}, false);
        });
        auto launch = [this, audioSetup, output, action, finish, driverSetup, cableSettings](bool install) {
            if (audioSetup->state() != QProcess::NotRunning) return;
            const auto script = QDir(QCoreApplication::applicationDirPath()).filePath(install ? "audio-setup.ps1" : "cable-setup.ps1");
            if (!QFileInfo::exists(script)) { finish("Audio setup is missing. Repair or reinstall SoundCurrent.", true); return; }
            if (install) {
                // Release our processing/guardians while retaining the UI.
                if (calibrating_) { finish("Stop the microphone calibration before changing the audio driver.", true); return; }
                power_->setChecked(false); micPower_->setChecked(false);
                meter_.stop(); audio_.stop(); microphone_.stop();
                status_->setText(SC_TR("Audio setup is running. Processing is paused; the app remains open."));
            }
            output->clear(); *action = install;
            centralWidget()->setEnabled(false);
            if (trayToggle_) trayToggle_->setEnabled(false);
            driverSetup->setEnabled(false); cableSettings->setEnabled(false);
            audioSetup->setProgram("powershell.exe");
            audioSetup->setArguments({"-NoProfile", "-ExecutionPolicy", "RemoteSigned", "-File", script,
                install ? "-Install" : "-Settings", "-Quiet", "-RequestingProcessId", QString::number(QCoreApplication::applicationPid())});
            audioSetup->start();
        };
        connect(driverSetup, &QPushButton::clicked, this, [launch] { launch(true); });
        connect(cableSettings, &QPushButton::clicked, this, [launch] { launch(false); });
#endif
        auto *speakerBox = new QGroupBox(SC_TR("Speaker model correction"));
        auto *speakerLayout = new QVBoxLayout(speakerBox);
        auto *speakerRow = new QHBoxLayout;
        speakerCombo_ = new PresetComboBox;
        speakerCombo_->setAccessibleName(SC_TR("Speaker model profile"));
        speakerCombo_->setMaxVisibleItems(12);
        speakerCombo_->addItem(SC_TR("None — use my own EQ"), QString());
        for (const auto &profile : speakerProfiles()) speakerCombo_->addItem(profile.name, profile.id);
        const int savedSpeaker = speakerCombo_->findData(QSettings().value("speakerModelId").toString());
        speakerCombo_->setCurrentIndex(std::max(0, savedSpeaker));
        speakerRow->addWidget(speakerCombo_, 1);
        auto *speakerDetails = new QPushButton(SC_TR("Profile details"));
        speakerRow->addWidget(speakerDetails);
        auto *taxonomy=new QHBoxLayout;
        taxonomy->addWidget(new QLabel(SC_TR("Manufacturer")));speakerBrand_=new PresetComboBox;speakerBrand_->setAccessibleName(SC_TR("Speaker manufacturer"));speakerBrand_->addItem(SC_TR("All manufacturers"));
        taxonomy->addWidget(speakerBrand_,1);taxonomy->addWidget(new QLabel(SC_TR("Type")));speakerType_=new PresetComboBox;speakerType_->setAccessibleName(SC_TR("Speaker type"));speakerType_->addItem(SC_TR("All speaker types"));taxonomy->addWidget(speakerType_,1);
        QStringList brands,types;for(const auto &p:speakerProfiles()){if(!brands.contains(p.brand))brands<<p.brand;if(!types.contains(p.equipmentType))types<<p.equipmentType;}
        brands.sort(Qt::CaseInsensitive);types.sort(Qt::CaseInsensitive);speakerBrand_->addItems(brands);speakerType_->addItems(types);
        speakerLayout->addLayout(taxonomy);speakerLayout->addLayout(speakerRow);
        connect(speakerBrand_,&QComboBox::currentIndexChanged,this,[this]{filterSpeakers();});
        connect(speakerType_,&QComboBox::currentIndexChanged,this,[this]{filterSpeakers();});
        auto *speakerHelp = new QLabel(SC_TR("Measured model correction is added to your listening EQ. You can still add bass or adjust any band. Includes conservative gain limits; room and amplifier effects require a system measurement."));
        speakerHelp->setWordWrap(true);
        speakerLayout->addWidget(speakerHelp);
        connect(speakerDetails, &QPushButton::clicked, this, [this] { showSpeakerDetails(); });
        auto *equipmentButton = new QPushButton(SC_TR("Browse all equipment profiles / editor"));
        equipmentButton->setAccessibleName(SC_TR("Import create and edit equipment profiles"));
        speakerLayout->addWidget(equipmentButton);
        equipmentStatus_ = new QLabel;
        equipmentStatus_->setWordWrap(true);
        speakerLayout->addWidget(equipmentStatus_);
        auto *clearEquipment = new QPushButton(SC_TR("Clear imported equipment corrections"));
        speakerLayout->addWidget(clearEquipment);
        connect(equipmentButton, &QPushButton::clicked, this, [this] {
            if (lockButton_->isChecked() || calibrating_) { showError(SC_TR("Unlock controls and finish measurement before editing profiles.")); return; }
            try { soundcurrent::equipment::openLibrary(this, [this](const auto &profile) { setEquipment(profile); }); }
            catch (const std::exception &e) { showError(QString::fromUtf8(e.what())); }
        });
        connect(clearEquipment, &QPushButton::clicked, this, [this] {
            if (lockButton_->isChecked() || calibrating_) return;
            recordChange(equipmentStatus_);
            for (const auto &kind : {"speaker", "amplifier", "microphone"}) QSettings().remove(QString("equipment/") + kind);
            refreshEquipmentStatus(); syncBandControls(); applyChanges(); commitChange();
            try { microphone_.update(micAdjustments(), micGain_->value() / 2.0); } catch (const std::exception &e) { showError(e.what()); }
        });
        refreshEquipmentStatus();
        settingsRoot->addWidget(speakerBox);
        auto *ampRow = new QHBoxLayout;
        ampRow->addWidget(new QLabel(SC_TR("Amplifier / receiver")));
        ampCombo_ = new PresetComboBox;
        ampCombo_->setAccessibleName(SC_TR("Amplifier model profile"));
        ampCombo_->addItem(SC_TR("None — use my own EQ"), QString());
        loadAmplifierProfiles();
        for (const auto &profile : amplifierProfiles_) ampCombo_->addItem(profile.name, profile.id);
        ampCombo_->setCurrentIndex(std::max(0, ampCombo_->findData(QSettings().value("amplifierModelId").toString())));
        ampRow->addWidget(ampCombo_, 1);
        ampImport_ = new QPushButton(SC_TR("Import measured profile"));
        ampRow->addWidget(ampImport_);
        auto *ampDetails = new QPushButton(SC_TR("Amp details"));
        ampRow->addWidget(ampDetails);
        speakerLayout->addLayout(ampRow);
        auto *ampHelp = new QLabel(SC_TR("Amplifier profiles require electrical measurements with known speaker load, input, and tone settings. Import a measured correction file; no amplifier curves are assumed from marketing specifications."));
        ampHelp->setWordWrap(true);
        speakerLayout->addWidget(ampHelp);
        connect(ampImport_, &QPushButton::clicked, this, [this] { importAmplifierProfile(); });
        connect(ampDetails, &QPushButton::clicked, this, [this] { showAmplifierDetails(); });



        auto *inputBox = new QGroupBox(SC_TR("Microphone"));
        auto *inputLayout = new QVBoxLayout(inputBox);
        auto *inputRow = new QHBoxLayout;
        inputCombo_ = new QComboBox;
        inputCombo_->setAccessibleName(SC_TR("Microphone input device"));
        inputCombo_->addItem(SC_TR("Plug in your microphone to select a microphone profile"), QString());
        inputRow->addWidget(inputCombo_, 1);
        micPower_ = new QCheckBox(SC_TR("Natural mic EQ"));
        micPower_->setAccessibleName(SC_TR("Natural microphone equalizer on or off"));
        micPower_->setToolTip(SC_TR("Automatically shape a connected microphone; click to bypass the microphone EQ"));
        micPower_->setChecked(QSettings().value("microphoneEnabled", true).toBool());
        inputRow->addWidget(micPower_);
        inputLayout->addLayout(inputRow);
        auto *toneRow = new QHBoxLayout;
        const QStringList micNames = {"Warmth", "Boxiness", "Clarity", "Air"};
        for (int i = 0; i < 4; ++i) {
            micLabels_[i] = new QLabel(soundcurrent::i18n::text(micNames[i].toUtf8().constData()));
            toneRow->addWidget(micLabels_[i]);
            micSliders_[i] = new QSlider(Qt::Horizontal);
            micSliders_[i]->setRange(-24, 24);
            micSliders_[i]->setValue(std::clamp(QSettings().value(QString("micBand%1").arg(i), 0).toInt(), -24, 24));
            micSliders_[i]->setAccessibleName(SC_TR("Microphone %1 adjustment").arg(soundcurrent::i18n::text(micNames[i].toUtf8().constData())));
            micSliders_[i]->setToolTip(SC_TR("Adjust this tone band around the natural voice profile"));
            toneRow->addWidget(micSliders_[i], 1);
            micLabels_[i]->setText(QString(SC_TR("%1 %2%3 dB")).arg(soundcurrent::i18n::text(micNames[i].toUtf8().constData()))
                                      .arg(micSliders_[i]->value() > 0 ? "+" : "")
                                      .arg(micSliders_[i]->value() / 2.0, 0, 'f', 1));
        }
        inputLayout->addLayout(toneRow);
        auto *micGainRow = new QHBoxLayout;
        micGainRow->addWidget(new QLabel(SC_TR("Mic gain")));
        micGain_ = new QSlider(Qt::Horizontal);
        micGain_->setRange(-24, 24);
        micGain_->setValue(std::clamp(QSettings().value("micGain", 0).toInt(), -24, 24));
        micGain_->setAccessibleName(SC_TR("Microphone gain adjustment"));
        micGainRow->addWidget(micGain_, 1);
        micGainValue_ = new QLabel;
        micGainValue_->setText(QString(SC_TR("%1%2 dB")).arg(micGain_->value() > 0 ? "+" : "")
                                   .arg(QLocale().toString(micGain_->value() / 2.0, 'f', 1)));
        micGainRow->addWidget(micGainValue_);
        auto *micReset = new QPushButton(SC_TR("Reset mic tone"));
        micGainRow->addWidget(micReset);
        inputLayout->addLayout(micGainRow);
        micStatus_ = new QLabel(SC_TR("Waiting for a microphone."));
        micStatus_->setWordWrap(true);
        inputLayout->addWidget(micStatus_);
#ifdef Q_OS_WIN
        auto *cableRow = new QHBoxLayout;
        cableRow->addWidget(new QLabel(SC_TR("Microphone route")));
        micCableCombo_ = new QComboBox;
        micCableCombo_->setAccessibleName(SC_TR("Second virtual cable for microphone EQ"));
        cableRow->addWidget(micCableCombo_, 1);
        inputLayout->addLayout(cableRow);
        auto *cableHelp = new QLabel(SC_TR("SoundCurrent Audio provides its own microphone route when installed. With VB-CABLE, simultaneous microphone and speaker EQ needs a separately installed second cable (A or B). Select that cable in recording apps. Automatic prefers the SoundCurrent route when available."));
        cableHelp->setWordWrap(true);
        inputLayout->addWidget(cableHelp);
        refreshMicCables();
        connect(micCableCombo_, &QComboBox::currentIndexChanged, this, [this] {
            const auto id = micCableCombo_->currentData().toString();
            QSettings().setValue("micCableId", id);
            microphone_.setCable(id);
            refreshInputs();
        });
#endif

        settingsRoot->addWidget(inputBox);
        auto *calibrationBox = new QGroupBox(SC_TR("Speaker && room calibration"));
        auto *calibrationLayout = new QVBoxLayout(calibrationBox);
        auto *calibrationRow = new QHBoxLayout;
        calibrationRow->addWidget(new QLabel(SC_TR("Speaker + room check")));
        calibrationMode_ = new QComboBox;
        calibrationMode_->addItem(SC_TR("Quiet logarithmic sweep"), "sweep");
        calibrationMode_->addItem(SC_TR("Separate quiet tones"), "tones");
        calibrationMode_->setAccessibleName(SC_TR("Calibration test signal"));
        calibrationRow->addWidget(calibrationMode_);
        calibrationStart_ = new QPushButton(SC_TR("Measure"));
        calibrationStart_->setAccessibleName(SC_TR("Measure speaker room and microphone response"));
        calibrationStart_->setToolTip(SC_TR("Play quiet test audio and preview suggested playback EQ changes"));
        calibrationRow->addWidget(calibrationStart_);
        calibrationStop_ = new QPushButton(SC_TR("Stop tones"));
        calibrationStop_->setEnabled(false);
        calibrationRow->addWidget(calibrationStop_);
        calibrationRow->addWidget(new QLabel(SC_TR("Test level")));
        calibrationLevel_ = new soundcurrent::AcceleratingSpinBox;
        calibrationLevel_->setRange(-54, -5);
        calibrationLevel_->setValue(-24);
        calibrationLevel_->setSuffix(" dBFS");
        calibrationLevel_->setToolTip(SC_TR("Start quiet. Raise only if the microphone cannot hear the tones."));
        calibrationLevel_->setAccessibleName(SC_TR("Calibration tone level"));
        calibrationRow->addWidget(calibrationLevel_);
        calibrationRow->addStretch();
        calibrationLayout->addLayout(calibrationRow);
        calibrationStatus_ = new QLabel(SC_TR("Use a quiet room. Measures speakers, room, and microphone together; results include the mic response."));
        calibrationStatus_->setWordWrap(true);
        calibrationLayout->addWidget(calibrationStatus_);
        settingsRoot->addWidget(calibrationBox);
        auto *updates=new soundcurrent::UpdatePanel("soundcurrent-eq","SoundCurrent EQ",SOUNDCURRENT_VERSION,startEnabled);
        updates->onReminder=[this](const QString &message){if(tray_)tray_->showMessage(SC_TR("Application update"),message,QSystemTrayIcon::Information,10000);};
        settingsRoot->addWidget(updates);
        settingsRoot->addStretch();

        auto *presetBox = new QGroupBox(SC_TR("Listening preset"));
        auto *presetRow = new QHBoxLayout(presetBox);
        presetCombo_ = new PresetComboBox;
        presetCombo_->setObjectName("localizedPresetSelector");
        presetCombo_->setAccessibleName(SC_TR("Listening preset"));
        presetCombo_->setView(new QListView(presetCombo_));
        presetCombo_->setMaxVisibleItems(12);
        presetCombo_->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        presetRow->addWidget(presetCombo_, 1);
        auto *save = new QPushButton(SC_TR("Save preset"));
        presetRow->addWidget(save);
        auto *reset = new QPushButton(SC_TR("Reset to flat"));
        presetRow->addWidget(reset);
        savePresetButton_ = save;
        resetButton_ = reset;
        lockButton_ = new QPushButton(SC_TR("Lock EQ"));
        lockButton_->setCheckable(true);
        lockButton_->setChecked(startEnabled && QSettings().value("eqLocked", false).toBool());
        lockButton_->setAccessibleName(SC_TR("Lock equalizer settings"));
        lockButton_->setToolTip(SC_TR("Prevent changes to presets, EQ bands, post gain, and balance"));
        presetRow->addWidget(lockButton_);
        undoButton_ = new QPushButton(SC_TR("Undo"));
        undoButton_->setAccessibleName(SC_TR("Undo last equalizer change"));
        undoButton_->setToolTip(SC_TR("Restore the previous EQ setting (Ctrl+Z)"));
        undoButton_->setEnabled(false);
        presetRow->addWidget(undoButton_);
        root->addWidget(presetBox);

        auto *eqBox = new QGroupBox(SC_TR("Equalizer"));
        eqBox->setLayoutDirection(Qt::LeftToRight);
        auto *eqLayout = new QVBoxLayout(eqBox);
        auto *toolbar = new QHBoxLayout;
        toolbar->addWidget(new QLabel(SC_TR("Bands")));
        countBox_ = new soundcurrent::AcceleratingSpinBox;
        countBox_->setRange(kMinBands, kMaxBands);
        countBox_->setValue(kDefaultBands);
        countBox_->setAccessibleName(SC_TR("Number of equalizer bands"));
        toolbar->addWidget(countBox_);
        toolbar->addSpacing(14);
        toolbar->addWidget(new QLabel(SC_TR("Drag curve points or tune the selected band below.")));
        toolbar->addStretch();
        headroom_ = new QLabel;
        toolbar->addWidget(headroom_);
        eqLayout->addLayout(toolbar);

        auto *details = new QWidget;
        auto *detailsRow = new QHBoxLayout(details);
        detailsRow->setContentsMargins(0, 2, 0, 2);
        detailsRow->addWidget(new QLabel(SC_TR("Selected band")));
        detailsRow->addSpacing(8);
        detailsRow->addWidget(new QLabel(SC_TR("Frequency")));
        frequencyBox_ = new soundcurrent::AcceleratingDoubleSpinBox;
        frequencyBox_->setRange(20, 20000);
        frequencyBox_->setDecimals(0);
        frequencyBox_->setSingleStep(1);
        frequencyBox_->setSuffix(" Hz");
        frequencyBox_->setAccessibleName(SC_TR("Selected band frequency"));
        detailsRow->addWidget(frequencyBox_);
        detailsRow->addSpacing(12);
        detailsRow->addWidget(new QLabel(SC_TR("Gain")));
        gainBox_ = new soundcurrent::AcceleratingDoubleSpinBox;
        gainBox_->setRange(-12, 12);
        gainBox_->setDecimals(1);
        gainBox_->setSingleStep(0.5);
        gainBox_->setSuffix(" dB");
        gainBox_->setAccessibleName(SC_TR("Selected band gain"));
        detailsRow->addWidget(gainBox_);
        detailsRow->addSpacing(12);
        detailsRow->addWidget(new QLabel(SC_TR("Width (Q)")));
        qBox_ = new soundcurrent::AcceleratingDoubleSpinBox;
        qBox_->setRange(0.3, 10.0);
        qBox_->setDecimals(2);
        qBox_->setSingleStep(0.1);
        qBox_->setAccessibleName(SC_TR("Selected band filter Q"));
        detailsRow->addWidget(qBox_);
        detailsRow->addStretch();
        eqLayout->addWidget(details);

        curve_ = new CurveWidget;
        eqLayout->addWidget(curve_);

        bandScroll_ = new QScrollArea;
        bandScroll_->setLayoutDirection(Qt::LeftToRight);
        bandScroll_->setWidgetResizable(false);
        bandScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        bandScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        bandScroll_->setFixedHeight(235);
        eqLayout->addWidget(bandScroll_);
        eqLayout->addWidget(new QLabel(SC_TR("Bars beside the sliders show estimated post-EQ levels. Red peak text warns of possible clipping.")));
        root->insertWidget(0, eqBox);
        root->addWidget(outputBox);
        enhancements_=new soundcurrent::EnhancementControls(false);
        soundcurrent::EnhancementSettings savedEffects;
        for(std::size_t i=0;i<5;++i)savedEffects.values[i]=std::clamp(QSettings().value(QString("enhancements/%1").arg(i),0).toDouble(),0.,1.);
        if(!savedEffects.valid())savedEffects={};
        enhancements_->setSettings(savedEffects);audio_.setEnhancements(savedEffects);
        root->addWidget(enhancements_);
        enhancements_->onEdited=[this]{recordChange(enhancements_);markCustom();saveEnhancements();applyChanges();commitChange();};

        root->addStretch();

        loadCustomPresets();
        rebuildPresetList("Flat");
        rebuildBandControls();
        syncBandControls();
        if (!startEnabled) {
            refreshDevices();
            try {
                for (const auto &device : inputDevices())
                    inputCombo_->addItem(device.description + (device.channels == 1 ? SC_TR(" · mono") : SC_TR(" · stereo")), device.name);
            } catch (const std::exception &) {}
            if (inputCombo_->count() > 1) inputCombo_->setItemText(0, "Automatic (follow connected microphones)");
        }

        connect(refresh, &QPushButton::clicked, this, [this] { refreshDevices(); });
        connect(quit, &QPushButton::clicked, qApp, [] { qApp->quit(); });
        connect(power_, &QCheckBox::toggled, this, [this](bool on) { togglePower(on); });
        connect(micPower_, &QCheckBox::toggled, this, [this](bool on) {
            QSettings().setValue("microphoneEnabled", on);
            if (on) refreshInputs();
            else { microphone_.stop(); micStatus_->setText(SC_TR("Microphone EQ is off.")); }
        });
        connect(inputCombo_, &QComboBox::currentIndexChanged, this, [this] { refreshInputs(); });
        for (int i = 0; i < 4; ++i) {
            connect(micSliders_[i], &QSlider::valueChanged, this, [this, i](int value) {
                QSettings().setValue(QString("micBand%1").arg(i), value);
                const QStringList names = {"Warmth", "Boxiness", "Clarity", "Air"};
                micLabels_[i]->setText(QString(SC_TR("%1 %2%3 dB")).arg(names[i])
                                           .arg(value > 0 ? "+" : "").arg(QLocale().toString(value / 2.0, 'f', 1)));
                try { microphone_.update(micAdjustments(), micGain_->value() / 2.0); }
                catch (const std::exception &error) { micStatus_->setText(error.what()); }
            });
        }
        connect(micGain_, &QSlider::valueChanged, this, [this](int value) {
            QSettings().setValue("micGain", value);
            micGainValue_->setText(QString(SC_TR("%1%2 dB")).arg(value > 0 ? "+" : "")
                                       .arg(QLocale().toString(value / 2.0, 'f', 1)));
            try { microphone_.update(micAdjustments(), value / 2.0); }
            catch (const std::exception &error) { micStatus_->setText(error.what()); }
        });
        connect(micReset, &QPushButton::clicked, this, [this] {
            for (auto *slider : micSliders_) slider->setValue(0);
            micGain_->setValue(0);
        });
        connect(calibrationStart_, &QPushButton::clicked, this, [this] { startCalibration(); });
        connect(calibrationStop_, &QPushButton::clicked, this, [this] {
            calibrationCancelled_ = true;
            calibration_.kill();
        });
#ifndef Q_OS_WIN
        calibration_.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGTERM); });
#endif
        connect(&calibration_, &QProcess::readyReadStandardError, this, [this] {
            const auto message = QString::fromUtf8(calibration_.readAllStandardError()).trimmed();
            if (!message.isEmpty()) calibrationStatus_->setText(message.section('\n', -1));
        });
        connect(&calibration_, &QProcess::readyReadStandardOutput, this, [this] {
            calibrationOutput_.append(calibration_.readAllStandardOutput());
        });
        connect(&calibration_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
                [this](int code, QProcess::ExitStatus status) { finishCalibration(code, status); });
        connect(outputCombo_, &QComboBox::currentIndexChanged, this, [this] { outputChanged(); });
        connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this] { presetChanged(); });
        connect(speakerCombo_, &QComboBox::currentIndexChanged, this, [this] {
            recordChange(speakerCombo_);
            QSettings().remove("equipment/speaker"); refreshEquipmentStatus();
            QSettings().setValue("speakerModelId", speakerCombo_->currentData());
            syncBandControls(); applyChanges(); commitChange();
        });
        connect(ampCombo_, &QComboBox::currentIndexChanged, this, [this] {
            recordChange(ampCombo_);
            QSettings().remove("equipment/amplifier"); refreshEquipmentStatus();
            QSettings().setValue("amplifierModelId", ampCombo_->currentData());
            syncBandControls(); applyChanges(); commitChange();
        });
        connect(save, &QPushButton::clicked, this, [this] { savePreset(); });
        connect(reset, &QPushButton::clicked, this, [this] { presetCombo_->setCurrentIndex(presetCombo_->findData("Flat")); });
        connect(lockButton_, &QPushButton::toggled, this, [this, startEnabled](bool locked) {
            if (startEnabled) QSettings().setValue("eqLocked", locked);
            updateControlsLock();
        });
        connect(undoButton_, &QPushButton::clicked, this, [this] { undoChange(); });
        auto *undoShortcut = new QShortcut(QKeySequence::Undo, this);
        connect(undoShortcut, &QShortcut::activated, this, [this] { undoChange(); });
        connect(outputGain_, &QSlider::valueChanged, this, [this](int) {
            recordChange(outputGain_);
            const double value = outputGainDb();
            QSettings().setValue("outputGainDb", value);
            outputGainValue_->setText(QString(SC_TR("%1%2 dB")).arg(value > 0 ? "+" : "")
                                          .arg(value, 0, 'f', 1));
            meter_.setProfile(bands_, value, balance_->value(), speakerCorrection());
            try { audio_.updateGain(value, balance_->value()); }
            catch (const std::exception &error) { showError(error.what()); }
            commitChange();
        });
        connect(balance_, &QSlider::valueChanged, this, [this](int value) {
            recordChange(balance_);
            QSettings().setValue("balancePercent", value);
            balanceValue_->setText(value == 0 ? SC_TR("Center")
                                   : QString("%1 %2%").arg(value < 0 ? SC_TR("L") : SC_TR("R")).arg(std::abs(value)));
            meter_.setProfile(bands_, outputGainDb(), value, speakerCorrection());
            try { audio_.updateGain(outputGainDb(), value); }
            catch (const std::exception &error) { showError(error.what()); }
            commitChange();
        });
        connect(levelRefresh_, &QSpinBox::valueChanged, this, [this](int milliseconds) {
            QSettings().setValue("levelRefreshMs", milliseconds);
            meter_.setInterval(milliseconds);
        });
        connect(peakMarkers_, &QCheckBox::toggled, this, [this](bool enabled) {
            QSettings().setValue("showPeakMarkers", enabled);
            for (auto *level : levelBars_) level->setPeakMarkersEnabled(enabled);
            overallLevel_->setPeakMarkersEnabled(enabled);
        });
        connect(countBox_, &QSpinBox::valueChanged, this, [this](int count) { changeBandCount(count); });
        connect(frequencyBox_, &QDoubleSpinBox::valueChanged, this, [this] { detailChanged(); });
        connect(gainBox_, &QDoubleSpinBox::valueChanged, this, [this] { detailChanged(); });
        connect(qBox_, &QDoubleSpinBox::valueChanged, this, [this] { detailChanged(); });
        curve_->onSelect = [this](int index) { selectBand(index); };
        curve_->onMove = [this](int index, double frequency, double gain) {
            if (index < 0 || index >= bands_.size()) return;
            recordChange(curve_);
            bands_[index].frequency = frequency;
            bands_[index].gain = gain;
            selectBand(index);
            markCustom();
            syncBandControls();
            applyChanges();
            commitChange();
        };
        meter_.onLevels = [this](const QVector<double> &levels, double peak) { showLevels(levels, peak); };
        meter_.setProfile(bands_, outputGainDb(), balance_->value(), speakerCorrection());
        meter_.setInterval(levelRefresh_->value());
        monitor_.setInterval(1500);
        connect(&monitor_, &QTimer::timeout, this, [this] { refreshDevices(); refreshInputs(); });
        monitor_.start();
#ifndef Q_OS_WIN
        volumeEvents_.setProgram("pactl");
        volumeEvents_.setArguments({"subscribe"});
        volumeEvents_.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGTERM); });
        connect(&volumeEvents_, &QProcess::readyReadStandardOutput, this, [this] {
            volumeEventBuffer_.append(volumeEvents_.readAllStandardOutput());
            if (volumeEventBuffer_.size() > 4096) volumeEventBuffer_.remove(0, volumeEventBuffer_.size() - 4096);
            int newline = 0;
            while ((newline = volumeEventBuffer_.indexOf('\n')) >= 0) {
                const auto event = volumeEventBuffer_.left(newline);
                volumeEventBuffer_.remove(0, newline + 1);
                if (event.contains("on server") && power_->isChecked() && audio_.legacyVolumeManaged()) {
                    try { if (defaultSink() != kSink) power_->setChecked(false); }
                    catch (const std::exception &) {}
                }
            }
        });
        volumeEvents_.start();
#endif
        for (auto *widget : findChildren<QWidget *>())
            if (qobject_cast<QComboBox *>(widget) || qobject_cast<QAbstractSpinBox *>(widget) ||
                qobject_cast<QSlider *>(widget)) widget->installEventFilter(this);
        currentSnapshot_ = snapshot();
        snapshotReady_ = true;
        if (startEnabled) setupTray();
        if (startEnabled) QTimer::singleShot(100, this, [this] {
            refreshDevices();
            refreshInputs();
            if (power_->isEnabled()) power_->setChecked(true);
        });
    }

    ~MainWindow() override {
        if (calibration_.state() != QProcess::NotRunning) {
            calibration_.kill();
            calibration_.waitForFinished(1000);
        }
        if (volumeEvents_.state() != QProcess::NotRunning) {
            volumeEvents_.terminate();
            if (!volumeEvents_.waitForFinished(500)) volumeEvents_.kill();
        }
        meter_.stop();
        audio_.stop();
        microphone_.stop();
    }

    void stopForConflict() {
        if (calibrating_) { calibrationCancelled_=true; calibration_.kill(); calibration_.waitForFinished(1000); }
        meter_.stop(); audio_.stop(); microphone_.stop();
        const QSignalBlocker a(power_),b(micPower_);power_->setChecked(false);micPower_->setChecked(false);
    }
    void reopen() {
        showNormal();
        fitToDisplay();
        raise();
        activateWindow();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Wheel && scroll_) {
            auto *wheel = static_cast<QWheelEvent *>(event);
            if (wheel->angleDelta().y() || wheel->pixelDelta().y()) {
                auto *page = qobject_cast<QScrollArea *>(tabs_->currentWidget());
                auto *bar = (page ? page : scroll_)->verticalScrollBar();
                QWheelEvent forwarded(QPointF(bar->rect().center()), wheel->globalPosition(),
                                      wheel->pixelDelta(), wheel->angleDelta(), wheel->buttons(),
                                      wheel->modifiers(), wheel->phase(), wheel->inverted(),
                                      wheel->source());
                QCoreApplication::sendEvent(bar, &forwarded);
                return true;
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

    void showEvent(QShowEvent *event) override {
        QMainWindow::showEvent(event);
        QTimer::singleShot(0, this, [this] {
            fitToDisplay();
            if (isVisible() && power_->isChecked() && !meter_.active()) meter_.start();
        });
    }

    void closeEvent(QCloseEvent *event) override {
        if (calibrating_) {
            calibrationCancelled_ = true;
            calibration_.kill();
            calibration_.waitForFinished(1000);
        }
        if (tray_ && QSystemTrayIcon::isSystemTrayAvailable()) {
            meter_.stop();
            hide();
            event->ignore();
            if (!backgroundNoticeShown_) {
                tray_->showMessage("SoundCurrent EQ", SC_TR("Equalizer is still running. Use the tray icon to reopen or quit."));
                backgroundNoticeShown_ = true;
            }
            return;
        }
        QMainWindow::closeEvent(event);
    }

private:
    struct EqSnapshot {
        Bands bands;
        QString preset;
        QString speaker, amplifier;
        int selected = 0;
        int gain = 0;
        int balance = 0;
        std::array<QByteArray,3> equipment;
        soundcurrent::EnhancementSettings enhancements;
    };

    EqSnapshot snapshot() const {
        return {bands_, presetCombo_->currentData().toString(), speakerCombo_->currentData().toString(), ampCombo_->currentData().toString(), selected_, outputGain_->value(), balance_->value(), {QSettings().value("equipment/speaker").toByteArray(), QSettings().value("equipment/amplifier").toByteArray(), QSettings().value("equipment/microphone").toByteArray()}, enhancements_->settings()};
    }

    void recordChange(QObject *source) {
        if (!snapshotReady_ || restoring_) return;
        if (qobject_cast<QComboBox *>(source) || source != lastChangeSource_ || !changeClock_.isValid() || changeClock_.elapsed() > 450) {
            undoStack_.append(currentSnapshot_);
            if (undoStack_.size() > 50) undoStack_.removeFirst();
        }
        lastChangeSource_ = source;
        changeClock_.restart();
        undoButton_->setEnabled(true);
    }

    void commitChange() {
        if (snapshotReady_ && !restoring_) currentSnapshot_ = snapshot();
    }

    void undoChange() {
        if (undoStack_.isEmpty()) return;
        const auto previous = undoStack_.takeLast();
        restoring_ = true;
        {
            const QSignalBlocker presetBlock(presetCombo_);
            const QSignalBlocker speakerBlock(speakerCombo_);
            const QSignalBlocker amplifierBlock(ampCombo_);
            const QSignalBlocker countBlock(countBox_);
            const QSignalBlocker gainBlock(outputGain_);
            const QSignalBlocker balanceBlock(balance_);
            bands_ = previous.bands;
            selected_ = std::clamp(previous.selected, 0, int(bands_.size()) - 1);
            countBox_->setValue(int(bands_.size()));
            presetCombo_->setCurrentIndex(presetCombo_->findData(previous.preset));
            ensureSpeakerChoice(previous.speaker);
            speakerCombo_->setCurrentIndex(std::max(0, speakerCombo_->findData(previous.speaker)));
            QSettings().setValue("speakerModelId", previous.speaker);
            ampCombo_->setCurrentIndex(std::max(0, ampCombo_->findData(previous.amplifier)));
            QSettings().setValue("amplifierModelId", previous.amplifier);
            const QStringList kinds={"speaker","amplifier","microphone"};
            for(int i=0;i<3;++i) { if(previous.equipment[i].isEmpty())QSettings().remove("equipment/"+kinds[i]);else QSettings().setValue("equipment/"+kinds[i],previous.equipment[i]); }
            refreshEquipmentStatus();
            enhancements_->setSettings(previous.enhancements);saveEnhancements();
            outputGain_->setValue(previous.gain);
            balance_->setValue(previous.balance);
            rebuildBandControls();
            syncBandControls();
        }
        outputGainValue_->setText(QString(SC_TR("%1%2 dB")).arg(outputGainDb() > 0 ? "+" : "")
                                      .arg(QLocale().toString(outputGainDb(), 'f', 1)));
        balanceValue_->setText(balance_->value() == 0 ? SC_TR("Center")
                               : QString("%1 %2%").arg(balance_->value() < 0 ? SC_TR("L") : SC_TR("R"))
                                     .arg(std::abs(balance_->value())));
        QSettings().setValue("outputGainDb", outputGainDb());
        QSettings().setValue("balancePercent", balance_->value());
        applyChanges();
        try { microphone_.update(micAdjustments(), micGain_->value() / 2.0); } catch (const std::exception &e) { showError(e.what()); }
        restoring_ = false;
        currentSnapshot_ = snapshot();
        lastChangeSource_ = nullptr;
        changeClock_.invalidate();
        undoButton_->setEnabled(!undoStack_.isEmpty());
    }

    void updateControlsLock() {
        const bool editable = !lockButton_->isChecked();
        lockButton_->setText(editable ? SC_TR("Lock EQ") : SC_TR("Unlock EQ"));
        enhancements_->setEnabled(editable);
        outputGain_->setEnabled(editable);
        balance_->setEnabled(editable);
        speakerBrand_->setEnabled(editable);speakerType_->setEnabled(editable);
        presetCombo_->setEnabled(editable);
        speakerCombo_->setEnabled(editable);
        ampCombo_->setEnabled(editable);
        ampImport_->setEnabled(editable);
        savePresetButton_->setEnabled(editable);
        resetButton_->setEnabled(editable);
        countBox_->setEnabled(editable);
        frequencyBox_->setEnabled(editable);
        gainBox_->setEnabled(editable);
        qBox_->setEnabled(editable);
        curve_->setLocked(!editable);
        if (!calibrating_) calibrationStart_->setEnabled(editable);
        for (auto *slider : sliders_) slider->setEnabled(editable);
    }

    void fitToDisplay() {
        auto *display = screen() ? screen() : QGuiApplication::primaryScreen();
        if (!display) return;
        const auto area = display->availableGeometry().adjusted(24, 24, -24, -24);
        if (!area.isValid()) return;
        const auto frame = frameGeometry().size() - geometry().size();
        const auto clientWidth = std::max(1, area.width() - std::max(0, frame.width()));
        const auto clientHeight = std::max(1, area.height() - std::max(0, frame.height()));
        setMinimumSize(std::min(480, clientWidth), std::min(320, clientHeight));
        resize(std::min(width(), clientWidth), std::min(height(), clientHeight));
        if (!area.contains(frameGeometry())) {
            const auto maxX = std::max(area.left(), area.right() - frameGeometry().width() + 1);
            const auto maxY = std::max(area.top(), area.bottom() - frameGeometry().height() + 1);
            const auto x = std::clamp(frameGeometry().x(), area.left(), maxX);
            const auto y = std::clamp(frameGeometry().y(), area.top(), maxY);
            move(x, y);
        }
    }

    double outputGainDb() const { return outputGain_->value() / 2.0; }

    MicTuning micAdjustments() const {
        MicTuning tuning{};
        for (int i = 0; i < 4; ++i) tuning[i] = micSliders_[i]->value() / 2.0;
        tuning.correction = equipmentBands("microphone");
        return tuning;
    }

    void startCalibration() {
        if (calibrating_) return;
        QString output, input;
        bool found = false;
        try {
            output = audio_.active() ? (audio_.smart() ? audio_.target() : QString(kSink))
                                     : selectedDevice().name;
            input = microphone_.active() ? microphone_.target() : defaultSource();
            for (const auto &device : inputDevices()) if (device.name == input) found = true;
        } catch (const std::exception &error) {
            calibrationStatus_->setText(SC_TR("Cannot start measurement: %1").arg(QString::fromUtf8(error.what())));
            return;
        }
        if (output.isEmpty() || !found) {
            calibrationStatus_->setText(SC_TR("Connect an output and a microphone before measuring."));
            return;
        }
        calibrating_ = true;
        calibrationCancelled_ = false;
        calibrationOutput_.clear();
        calibrationStart_->setEnabled(false);
        calibrationStop_->setEnabled(true);
        calibrationLevel_->setEnabled(false);
        calibrationMode_->setEnabled(false);
        inputCombo_->setEnabled(false);
        outputCombo_->setEnabled(false);
        micPower_->setEnabled(false);
        microphone_.stop();
        calibrationStatus_->setText(SC_TR("Playing quiet test audio. Stop if it is uncomfortable."));
        calibration_.setProgram(QCoreApplication::applicationFilePath());
        calibration_.setArguments({"--calibration-worker", output, input,
                                   QString::number(calibrationLevel_->value()),
                                   calibrationMode_->currentData().toString()});
        calibration_.start();
        if (!calibration_.waitForStarted(2000)) {
            calibrating_ = false;
            calibrationStart_->setEnabled(!lockButton_->isChecked());
            calibrationStop_->setEnabled(false);
            calibrationLevel_->setEnabled(true);
            calibrationMode_->setEnabled(true);
            inputCombo_->setEnabled(true);
            outputCombo_->setEnabled(true);
            micPower_->setEnabled(true);
            refreshInputs();
            calibrationStatus_->setText(SC_TR("Could not start the measurement."));
        }
    }

    void finishCalibration(int code, QProcess::ExitStatus exitStatus) {
        if (!calibrating_) return;
        calibrationOutput_.append(calibration_.readAllStandardOutput());
        const bool cancelled = calibrationCancelled_;
        calibrating_ = false;
        calibrationStart_->setEnabled(!lockButton_->isChecked());
        calibrationStop_->setEnabled(false);
        calibrationLevel_->setEnabled(true);
        calibrationMode_->setEnabled(true);
        inputCombo_->setEnabled(true);
        outputCombo_->setEnabled(true);
        micPower_->setEnabled(true);
        refreshInputs();
        if (cancelled) { calibrationStatus_->setText(SC_TR("Measurement stopped.")); return; }
        if (exitStatus != QProcess::NormalExit || code != 0) {
            if (!calibrationStatus_->text().startsWith("Measurement failed"))
                calibrationStatus_->setText(SC_TR("Measurement failed. Try a higher test level or move the mic closer."));
            return;
        }
        const auto result = QJsonDocument::fromJson(calibrationOutput_).object();
        const auto suggestion = calibrationSuggestion(result, bands_);
        if (!suggestion) { calibrationStatus_->setText(SC_TR("Measurement data was incomplete.")); return; }
        QMessageBox preview(this);
        preview.setWindowTitle(SC_TR("Speaker and room measurement"));
        preview.setIcon(QMessageBox::Information);
        preview.setText(SC_TR("Suggested changes to the playback EQ"));
        preview.setInformativeText(SC_TR("Relative measurements include the speaker, room, and microphone response. The proposed changes are limited to 3 dB per measured frequency.\n\n%1").arg(suggestion->preview));
        auto *saveMeasured = preview.addButton(SC_TR("Save system response profile"), QMessageBox::ActionRole);
        auto *apply = preview.addButton(SC_TR("Apply suggested EQ"), QMessageBox::AcceptRole);
        preview.addButton(SC_TR("Keep current EQ"), QMessageBox::RejectRole);
        preview.exec();
        if (preview.clickedButton() == saveMeasured) {
            soundcurrent::equipment::Profile p;
            p.kind = "speaker"; p.brand = "Custom"; p.family = "Whole listening system"; p.model = "Measured listening position"; p.custom = true;
            p.conditions = "Combined speaker/amplifier/microphone/room response; not an isolated equipment measurement. " + inputCombo_->currentText() + " / " + outputCombo_->currentText();
            p.provenance = "SoundCurrent sweep or tone measurement; relative to median; microphone EQ bypassed. Playback EQ may be included.";
            const auto levels = result.value("levels").toArray(); QVector<double> db;
            for (const auto &v : levels) if (v.isDouble() && v.toDouble() > 0) db.append(20 * std::log10(v.toDouble()));
            std::sort(db.begin(),db.end()); const double reference = db[db.size()/2];
            for (int i=0;i<levels.size();++i) if (levels[i].isDouble() && levels[i].toDouble()>0) p.response.append({double(kCalibrationFrequencies[i]),20*std::log10(levels[i].toDouble())-reference});
            try { p.filters = soundcurrent::equipment::fitResponse(p.response); soundcurrent::equipment::saveNewProfile(this,p); calibrationStatus_->setText(SC_TR("System response profile editor opened. Saved profiles are available in the equipment library.")); }
            catch (const std::exception &e) { showError(e.what()); }
            return;
        }
        if (preview.clickedButton() == apply && suggestion->changed > 0) {
            recordChange(calibrationStart_);
            bands_ = suggestion->bands;
            markCustom();
            syncBandControls();
            applyChanges();
            commitChange();
            calibrationStatus_->setText(SC_TR("Suggested EQ applied. Use Save preset to keep it."));
        } else calibrationStatus_->setText(SC_TR("Current EQ kept."));
    }

    void refreshInputs() {
        if (calibrating_) return;
#ifdef Q_OS_WIN
        refreshMicCables();
#endif
        try {
            const auto latest = inputDevices();
            const auto manual = inputCombo_->currentData().toString();
            const auto previousTarget = microphone_.target();
            QString disconnected;
            for (const auto &old : inputs_)
                if (std::none_of(latest.begin(), latest.end(), [&](const InputDevice &now) {
                        return now.name == old.name;
                    }) && (old.name == previousTarget || old.name == manual))
                    disconnected = old.description;
            QList<InputDevice> added;
            for (const auto &device : latest) if (!knownInputNames_.contains(device.name)) added.append(device);
            if (std::any_of(added.begin(), added.end(), [](const InputDevice &device) {
                    return device.name.contains(".usb-");
                })) micDisconnectNotice_.clear();
            const bool changed = latest.size() != inputs_.size() ||
                !std::equal(latest.begin(), latest.end(), inputs_.begin(), [](const InputDevice &a, const InputDevice &b) {
                    return a.name == b.name && a.description == b.description && a.channels == b.channels;
                });
            inputs_ = latest;
            knownInputNames_.clear();
            for (const auto &device : inputs_) knownInputNames_.append(device.name);
            if (changed) {
                inputCombo_->blockSignals(true);
                inputCombo_->clear();
                inputCombo_->addItem(inputs_.isEmpty() ? SC_TR("Plug in your microphone to select a microphone profile")
                                                     : SC_TR("Automatic (follow connected microphones)"), QString());
                for (const auto &device : inputs_)
                    inputCombo_->addItem(device.description + (device.channels == 1 ? SC_TR(" · mono") : SC_TR(" · stereo")), device.name);
                const int index = inputCombo_->findData(manual);
                inputCombo_->setCurrentIndex(index < 0 ? 0 : index);
                inputCombo_->blockSignals(false);
            }
            micPower_->setEnabled(!inputs_.isEmpty());
            if (!disconnected.isEmpty()) micDisconnectNotice_ = SC_TR("%1 disconnected. ").arg(disconnected);
            if (!micPower_->isChecked()) {
                if (!micDisconnectNotice_.isEmpty()) micStatus_->setText(micDisconnectNotice_ + SC_TR("Microphone EQ is off."));
                return;
            }
            if (inputs_.isEmpty()) {
                microphone_.stop();
                micStatus_->setText(micDisconnectNotice_.isEmpty() ? SC_TR("No microphone connected.")
                                                               : micDisconnectNotice_ + SC_TR("No microphone connected."));
                return;
            }
            InputDevice desired;
            if (!manual.isEmpty())
                for (const auto &device : inputs_) if (device.name == manual) desired = device;
            if (desired.name.isEmpty() && !added.isEmpty())
                desired = *std::max_element(added.begin(), added.end(), [](const InputDevice &a, const InputDevice &b) {
                    return a.priority < b.priority;
                });
            if (desired.name.isEmpty()) {
                const auto current = defaultSource();
                for (const auto &device : inputs_) if (device.name == current) desired = device;
            }
            if (desired.name.isEmpty() && !microphone_.target().isEmpty())
                for (const auto &device : inputs_) if (device.name == microphone_.target()) desired = device;
            if (desired.name.isEmpty())
                desired = *std::max_element(inputs_.begin(), inputs_.end(), [](const InputDevice &a, const InputDevice &b) {
                    return a.priority < b.priority;
                });
            if (!microphone_.active() || microphone_.target() != desired.name) {
                microphone_.start(desired, micAdjustments(), micGain_->value() / 2.0);
            }
            const bool usbConnected = std::any_of(inputs_.begin(), inputs_.end(), [](const InputDevice &device) {
                return device.name.contains(".usb-");
            });
            micStatus_->setText(micDisconnectNotice_ + SC_TR("Natural mic EQ on · %1").arg(desired.description) +
                                (!usbConnected ? usbMicrophoneHint() : ""));
        } catch (const std::exception &error) { micStatus_->setText(SC_TR("Microphone error: %1").arg(QString::fromUtf8(error.what()))); }
    }

    void showPlaybackStatus(const Device &device) {
        status_->setText(SC_TR("On · Playing through %1").arg(device.description));
    }

    void setupTray() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
        tray_ = new QSystemTrayIcon(QIcon::fromTheme("io.github.rhamenator.SoundCurrentEQ", QIcon(":/app.ico")), this);
        tray_->setToolTip("SoundCurrent EQ");
        auto *menu = new QMenu(this);
        menu->addAction("Open SoundCurrent EQ", this, [this] { reopen(); });
        trayToggle_ = menu->addAction("Turn equalizer off", this, [this] { power_->setChecked(!power_->isChecked()); });
        connect(power_, &QCheckBox::toggled, this, [this](bool on) {
            trayToggle_->setText(on ? SC_TR("Turn equalizer off") : SC_TR("Turn equalizer on"));
            tray_->setToolTip(on ? "SoundCurrent EQ · On" : "SoundCurrent EQ · Off");
        });
        menu->addSeparator();
        menu->addAction("Quit SoundCurrent EQ", qApp, [] { qApp->quit(); });
        tray_->setContextMenu(menu);
        connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) reopen();
        });
        tray_->show();
        qApp->setQuitOnLastWindowClosed(false);
    }

    static QString frequencyLabel(double frequency) {
        return frequency >= 1000 ? QLocale().toString(frequency / 1000.0, 'g', 3) + "k"
                                 : QLocale().toString(frequency, 'g', 4);
    }

    void rebuildBandControls() {
        if (auto *old = bandScroll_->takeWidget()) old->deleteLater();
        sliders_.clear();
        gainLabels_.clear();
        frequencyButtons_.clear();
        levelBars_.clear();
        auto *container = new QWidget;
        container->setLayoutDirection(Qt::LeftToRight);
        auto *row = new QHBoxLayout(container);
        row->setContentsMargins(8, 4, 8, 8);
        row->setSpacing(4);
        for (qsizetype i = 0; i < bands_.size(); ++i) {
            auto *column = new QVBoxLayout;
            auto *value = new QLabel;
            value->setAlignment(Qt::AlignCenter);
            value->setObjectName("value");
            gainLabels_.append(value);
            column->addWidget(value);
            auto *slider = new QSlider(Qt::Vertical);
            slider->setRange(-24, 24);
            slider->setSingleStep(1);
            slider->setPageStep(2);
            slider->setMinimumHeight(140);
            slider->setAccessibleName(QString(SC_TR("Band %1 gain")).arg(i + 1));
            slider->installEventFilter(this);
            sliders_.append(slider);
            auto *sliderRow = new QHBoxLayout;
            sliderRow->setSpacing(3);
            sliderRow->addWidget(slider, 1, Qt::AlignHCenter);
            auto *level = new BandLevelMeter;
            level->setPeakMarkersEnabled(peakMarkers_->isChecked());
            level->setAccessibleName(QString(SC_TR("Estimated output level near band %1")).arg(i + 1));
            level->setToolTip(SC_TR("Estimated post-EQ level near this frequency"));
            levelBars_.append(level);
            sliderRow->addWidget(level);
            column->addLayout(sliderRow, 1);
            auto *frequency = new QPushButton;
            frequency->setToolTip(SC_TR("Select this band to edit frequency, gain, and Q"));
            frequency->setAccessibleName(QString(SC_TR("Select band %1")).arg(i + 1));
            frequencyButtons_.append(frequency);
            column->addWidget(frequency);
            row->addLayout(column);
            connect(slider, &QSlider::valueChanged, this, [this, i](int value) {
                if (changing_) return;
                recordChange(sliders_[i]);
                bands_[i].gain = value / 2.0;
                selectBand(int(i));
                markCustom();
                applyChanges();
                commitChange();
            });
            connect(frequency, &QPushButton::clicked, this, [this, i] { selectBand(int(i)); });
        }
        container->setFixedWidth(std::max(760, int(bands_.size()) * 63));
        container->setMinimumHeight(210);
        bandScroll_->setWidget(container);
        lastChangeSource_ = nullptr;
        if (lockButton_->isChecked()) updateControlsLock();
    }

    void showLevels(const QVector<double> &levels, double peak) {
        const auto count = std::min(levels.size(), levelBars_.size());
        for (qsizetype i = 0; i < count; ++i) {
            const double db = 20.0 * std::log10(std::max(levels[i], 0.000001));
            if (power_->isChecked()) levelBars_[i]->setLevel(db);
            else levelBars_[i]->reset();
            levelBars_[i]->setToolTip(QString(SC_TR("Estimated output near %1: %2 dBFS"))
                                     .arg(frequencyLabel(bands_[i].frequency)).arg(db, 0, 'f', 1));
        }
        if (!power_->isChecked()) {
            overallLevel_->reset();
            peakStatus_->setText(SC_TR("Estimated peak: EQ off"));
            peakStatus_->setStyleSheet("color:#8fa2bb;");
        } else if (peak <= 0.000001) {
            overallLevel_->setLevel(-60.0);
            peakStatus_->setText(SC_TR("Estimated peak: waiting for audio"));
            peakStatus_->setStyleSheet("color:#8fa2bb;");
        } else {
            const double db = 20.0 * std::log10(peak);
            overallLevel_->setLevel(db);
            overallLevel_->setToolTip(QString(SC_TR("Estimated overall output peak: %1 dBFS"))
                                          .arg(db, 0, 'f', 1));
            peakStatus_->setText(db >= -1.0
                                     ? QString(SC_TR("Clipping risk · estimated peak %1 dBFS")).arg(db, 0, 'f', 1)
                                     : QString(SC_TR("Estimated peak %1 dBFS")).arg(db, 0, 'f', 1));
            peakStatus_->setStyleSheet(db >= -1.0 ? "color:#f16b76;font-weight:700;"
                                                    : db >= -6.0 ? "color:#e6b450;" : "color:#50d1ba;");
        }
    }

    void syncBandControls() {
        changing_ = true;
        for (qsizetype i = 0; i < bands_.size(); ++i) {
            const auto &band = bands_[i];
            sliders_[i]->setValue(std::lround(band.gain * 2));
            gainLabels_[i]->setText((band.gain > 0 ? "+" : "") + QLocale().toString(band.gain, 'g', 3));
            frequencyButtons_[i]->setText(frequencyLabel(band.frequency));
            frequencyButtons_[i]->setStyleSheet(i == selected_ ? "background:#268f84;color:white;" : "");
        }
        changing_ = false;
        const auto &band = bands_[selected_];
        const QSignalBlocker frequencyBlock(frequencyBox_);
        const QSignalBlocker gainBlock(gainBox_);
        const QSignalBlocker qBlock(qBox_);
        const auto minimum = selected_ > 0 ? std::ceil(bands_[selected_ - 1].frequency * 1.02) : 20.0;
        const auto maximum = selected_ + 1 < bands_.size() ? std::floor(bands_[selected_ + 1].frequency / 1.02) : 20000.0;
        frequencyBox_->setRange(minimum, maximum);
        frequencyBox_->setValue(band.frequency);
        gainBox_->setValue(band.gain);
        qBox_->setValue(band.q);
        headroom_->setText(SC_TR("Auto headroom %1 dB").arg(QLocale().toString(headroom(processingBands()), 'f', 1)));
        curve_->setBands(bands_, selected_);
        curve_->setCorrection(speakerCorrection());
    }

    void selectBand(int index) {
        if (index < 0 || index >= bands_.size()) return;
        selected_ = index;
        syncBandControls();
        commitChange();
    }

    void markCustom() {
        changing_ = true;
        presetCombo_->setCurrentIndex(presetCombo_->findData("Custom"));
        changing_ = false;
    }

    static QString usbMicrophoneHint() {
#ifdef Q_OS_WIN
        return {};
#else
        return SC_TR(" · no USB microphone detected");
#endif
    }
    QLabel *equipmentStatus_ = nullptr;
    std::optional<soundcurrent::equipment::Profile> equipmentProfile(const QString &kind) const {
        const auto bytes = QSettings().value("equipment/" + kind).toByteArray();
        if (bytes.isEmpty()) return {};
        try { return soundcurrent::equipment::parse(bytes); } catch (const std::exception &) { return {}; }
    }
    Bands equipmentBands(const QString &kind) const {
        Bands result;
        if (const auto p = equipmentProfile(kind)) for (const auto &b : p->filters) result.append({b.frequency,b.gainDb,b.q,b.type});
        return result;
    }
    void refreshEquipmentStatus() {
        QStringList names;
        for (const auto &kind : {"speaker","amplifier","microphone"}) if (const auto p = equipmentProfile(kind)) names << QString(kind) + ": " + p->brand + " / " + p->family + " / " + p->model;
        equipmentStatus_->setText(names.isEmpty() ? SC_TR("No imported equipment correction selected.") : names.join("\n"));
    }
    void setEquipment(const soundcurrent::equipment::Profile &profile) {
        if (lockButton_->isChecked() || calibrating_) return;
        recordChange(equipmentStatus_);
        QSettings().setValue("equipment/" + profile.kind, QJsonDocument(soundcurrent::equipment::serialize(profile)).toJson(QJsonDocument::Compact));
        refreshEquipmentStatus(); syncBandControls(); applyChanges(); commitChange();
        try { microphone_.update(micAdjustments(), micGain_->value() / 2.0); } catch (const std::exception &e) { showError(e.what()); }
    }
    Bands speakerCorrection() const {
        const auto id = speakerCombo_->currentData().toString();
        Bands correction;
        for (const auto &p : speakerProfiles()) if (p.id == id) correction = p.filters;
        if (equipmentProfile("speaker")) correction = equipmentBands("speaker");
        const auto amplifier = ampCombo_->currentData().toString();
        if (equipmentProfile("amplifier")) correction.append(equipmentBands("amplifier"));
        else for (const auto &p : amplifierProfiles_) if (p.id == amplifier) correction.append(p.filters);
        return correction;
    }
    Bands processingBands() const {
        auto result = bands_;
        result.resize(kMaxBands);
        result.append(speakerCorrection());
        return result;
    }
    void loadAmplifierProfiles() {
        QFile file(amplifierProfilesPath());
        if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) return;
        const auto profiles = QJsonDocument::fromJson(file.readAll()).array();
        for (const auto &value : profiles) {
            if (amplifierProfiles_.size() >= 32) break;
            const auto parsed = parseAmplifierProfile(value.toObject());
            if (parsed) amplifierProfiles_.append(*parsed);
        }
    }
    void importAmplifierProfile() {
        const auto path = QFileDialog::getOpenFileName(this, SC_TR("Import measured amplifier correction"), {}, SC_TR("Correction profile (*.json)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) { showError(SC_TR("Profile must be readable and smaller than 64 KiB.")); return; }
        const auto profile = parseAmplifierProfile(QJsonDocument::fromJson(file.readAll()).object());
        if (!profile) { showError(SC_TR("Invalid measured amplifier profile. Requires model, HTTPS measurement source, conditions, and 1–16 bounded PK/LS/HS filters. See the profile format in the README.")); return; }
        QMessageBox preview(QMessageBox::Question, "Apply amplifier correction?",
                            profile->name + "\n\nMeasurement conditions: " + profile->conditions +
                            "\nSource: " + profile->source + "\n\nApply only if these conditions match your system.",
                            QMessageBox::Apply | QMessageBox::Cancel, this);
        preview.setTextFormat(Qt::PlainText);
        if (preview.exec() != QMessageBox::Apply) return;
        int index = ampCombo_->findData(profile->id);
        if (index < 0) {
            if (amplifierProfiles_.size() >= 32) { showError(SC_TR("Maximum of 32 amplifier profiles reached.")); return; }
            auto proposed = amplifierProfiles_; proposed.append(*profile);
            QJsonArray array; for (const auto &p : proposed) array.append(p.json);
            const auto destination = amplifierProfilesPath();
            if (!QDir().mkpath(QFileInfo(destination).absolutePath())) { showError(SC_TR("Cannot create amplifier profile folder.")); return; }
            QSaveFile save(destination);
            if (!save.open(QIODevice::WriteOnly)) { showError(SC_TR("Cannot save amplifier profile.")); return; }
            save.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            save.write(QJsonDocument(array).toJson());
            if (!save.commit()) { showError(SC_TR("Cannot finish saving amplifier profile.")); return; }
            amplifierProfiles_ = proposed;
            ampCombo_->addItem(profile->name, profile->id);
            index = ampCombo_->count() - 1;
        }
        ampCombo_->setCurrentIndex(index);
    }
    void showAmplifierDetails() {
        const auto id = ampCombo_->currentData().toString();
        for (const auto &p : amplifierProfiles_) if (p.id == id) {
            QMessageBox details(QMessageBox::Information, "Amplifier profile details",
                p.name + "\nConditions: " + p.conditions + "\nSource: " + p.source +
                "\n\nCorrection filters:\n" + QString::fromUtf8(QJsonDocument(p.json.value("filters").toArray()).toJson()),
                QMessageBox::Ok, this);
            details.setTextFormat(Qt::PlainText); details.exec(); return;
        }
        QMessageBox::information(this, SC_TR("Amplifier profile details"), SC_TR("No measured amplifier correction is selected. Marketing frequency-range specifications are insufficient to derive a correction curve."));
    }
    QVector<AmplifierProfile> amplifierProfiles_;
    QComboBox *ampCombo_ = nullptr;
    QPushButton *ampImport_ = nullptr;

    QComboBox *speakerBrand_=nullptr,*speakerType_=nullptr;
    void ensureSpeakerChoice(const QString &id){
        if(id.isEmpty() || speakerCombo_->findData(id)>=0)return;
        for(const auto &p:speakerProfiles())if(p.id==id){speakerCombo_->addItem(p.name+SC_TR(" (restored selection)"),p.id);return;}
    }
    void filterSpeakers(){
        const auto selected=speakerCombo_->currentData().toString();const QSignalBlocker block(speakerCombo_);
        speakerCombo_->clear();speakerCombo_->addItem(SC_TR("None — use my own EQ"),QString());
        for(const auto &p:speakerProfiles()){
            const bool matches=(speakerBrand_->currentIndex()==0 || p.brand==speakerBrand_->currentText()) && (speakerType_->currentIndex()==0 || p.equipmentType==speakerType_->currentText());
            if(matches || p.id==selected)speakerCombo_->addItem(p.name+(matches?QString():SC_TR(" (currently selected)")),p.id);
        }
        speakerCombo_->setCurrentIndex(std::max(0,speakerCombo_->findData(selected)));
    }
    void showSpeakerDetails() {
        const auto id = speakerCombo_->currentData().toString();
        for (const auto &p : speakerProfiles()) if (p.id == id) {
            QString text = p.name + "\nMeasurement: " + p.attribution +
                "\n\nSpinorama AutoEQ adapted with gain capped at ±6 dB, Q capped at 6, and positive filters below 80 Hz omitted. Your listening preset is added separately.\n\n";
            for (const auto &b : p.filters) {
                using T = soundcurrent::FilterType;
                text += QString("%1 Hz · %2 dB · Q %3 · %4\n").arg(b.frequency).arg(b.gain).arg(b.q)
                    .arg(b.type == T::LowShelf ? "low shelf" : b.type == T::HighShelf ? "high shelf" : "peak");
            }
            text += "\nSources:\n" + p.links.join('\n');
            QMessageBox::information(this, SC_TR("Speaker profile details"), text);
            return;
        }
        QMessageBox::information(this, SC_TR("Speaker profile details"), SC_TR("No model correction selected. Your listening EQ works normally."));
    }
#ifdef Q_OS_WIN
    void refreshMicCables() {
        const auto current = micCableCombo_->count() ? micCableCombo_->currentData().toString()
                                                   : QSettings().value("micCableId").toString();
        const auto cables = microphoneCables();
        QStringList ids;
        for (const auto &c : cables) ids.append(c.render);
        if (ids != micCableIds_ || !micCableCombo_->count()) {
            const QSignalBlocker block(micCableCombo_);
            micCableCombo_->clear();
            micCableCombo_->addItem(SC_TR("Automatic (SoundCurrent Microphone)"), QString());
            for (const auto &c : cables) micCableCombo_->addItem(c.description, c.render);
            micCableCombo_->setCurrentIndex(std::max(0, micCableCombo_->findData(current)));
            micCableIds_ = ids;
        }
        microphone_.setCable(micCableCombo_->currentData().toString());
    }
    QComboBox *micCableCombo_ = nullptr;
    QStringList micCableIds_;
#endif
    QComboBox *speakerCombo_ = nullptr;

    soundcurrent::EnhancementControls *enhancements_ = nullptr;
    QMap<QString,soundcurrent::EnhancementSettings> customEnhancements_;
    void saveEnhancements(){for(std::size_t i=0;i<5;++i)QSettings().setValue(QString("enhancements/%1").arg(i),enhancements_->settings().values[i]);}
    void applyChanges() {
        audio_.setEnhancements(enhancements_->settings());
        meter_.setProfile(bands_, outputGainDb(), balance_->value(), speakerCorrection());
        try { audio_.update(processingBands(), outputGainDb(), balance_->value()); }
        catch (const std::exception &error) { showError(error.what()); }
    }

    void detailChanged() {
        if (changing_ || bands_.isEmpty()) return;
        recordChange(sender());
        auto &band = bands_[selected_];
        band.frequency = frequencyBox_->value();
        band.gain = gainBox_->value();
        band.q = qBox_->value();
        markCustom();
        syncBandControls();
        applyChanges();
        commitChange();
    }

    void changeBandCount(int count) {
        if (changing_ || count == bands_.size()) return;
        recordChange(countBox_);
        bands_ = remapBands(bands_, count);
        selected_ = std::min(selected_, count - 1);
        rebuildBandControls();
        markCustom();
        syncBandControls();
        applyChanges();
        commitChange();
    }

    void presetChanged() {
        if (changing_) return;
        const auto name = presetCombo_->currentData().toString();
        if (!builtinShapes().contains(name) && !custom_.contains(name)) return;
        recordChange(presetCombo_);
        enhancements_->setSettings(customEnhancements_.value(name,{}));saveEnhancements();
        if (builtinShapes().contains(name)) bands_ = builtinProfile(name, countBox_->value());
        else bands_ = custom_[name];
        selected_ = std::min(selected_, int(bands_.size()) - 1);
        const QSignalBlocker blocker(countBox_);
        countBox_->setValue(int(bands_.size()));
        rebuildBandControls();
        syncBandControls();
        applyChanges();
        commitChange();
    }

    void loadCustomPresets() {
        QFile file(presetsPath());
        if (!file.open(QIODevice::ReadOnly)) return;
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key().trimmed().isEmpty() || it.key() == "Custom" || builtinShapes().contains(it.key())) continue;
            const auto parsed = parseBands(it.value());
            if (parsed) {
                custom_.insert(it.key(), *parsed);soundcurrent::EnhancementSettings s;
                const auto values=it.value().toObject().value("enhancements").toArray();
                if(values.size()==5)for(int i=0;i<5;++i)s.values[std::size_t(i)]=values[i].toDouble(-1);
                if(s.valid())customEnhancements_.insert(it.key(),s);
            }
        }
    }

    void rebuildPresetList(const QString &selected) {
        const QSignalBlocker blocker(presetCombo_);
        presetCombo_->clear();
        auto addGroup = [this](const QStringList &names) {
            if (presetCombo_->count()) presetCombo_->insertSeparator(presetCombo_->count());
            for (const auto &name : names) presetCombo_->addItem(soundcurrent::i18n::text(name.toUtf8().constData()),name);
        };
        addGroup({"Balanced", "Flat", "Loudness", "Warm", "Bright", "Soft Treble", "Treble Detail",
                  "Headphones", "Small Speakers", "Night Listening"});
        addGroup({"Bass Boost", "Deep Bass", "Punchy Bass", "Bass Cut",
                  "Clear Voice", "Podcast", "TV Dialogue", "Vocal Focus"});
        addGroup({"Movies", "Gaming", "FPS Footsteps", "Live"});
        addGroup({"Rock", "Pop", "Jazz", "Classical", "Electronic", "Dance", "Hip-Hop",
                  "R&B", "Acoustic", "Piano", "Metal", "Lo-Fi"});
        if (!custom_.isEmpty()) {
            presetCombo_->insertSeparator(presetCombo_->count());
            for (auto it = custom_.begin(); it != custom_.end(); ++it) presetCombo_->addItem(it.key(),it.key());
        }
        presetCombo_->addItem(SC_TR("Custom"));
        presetCombo_->setCurrentIndex(presetCombo_->findData(selected));
    }

    void savePreset() {
        bool ok = false;
        const auto name = QInputDialog::getText(this, SC_TR("Save EQ preset"), SC_TR("Preset name:"), QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok) return;
        if (name.isEmpty() || name == "Custom" || builtinShapes().contains(name)) {
            showError(SC_TR("Choose a name that is not a built-in preset."));
            return;
        }
        custom_[name] = bands_;
        customEnhancements_[name]=enhancements_->settings();
        QJsonObject object;
        for (auto it = custom_.begin(); it != custom_.end(); ++it) {
            QJsonArray values;const auto settings=customEnhancements_.value(it.key(),{});for(std::size_t i=0;i<5;++i)values.append(settings.values[i]);
            object.insert(it.key(), QJsonObject{{"bands", serializeBands(it.value())},{"enhancements",values}});
        }
        const auto path = presetsPath();
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) { showError(SC_TR("Could not create preset folder.")); return; }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) { showError(SC_TR("Could not save preset.")); return; }
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        file.write(QJsonDocument(object).toJson());
        if (!file.commit()) { showError(SC_TR("Could not finish saving preset.")); return; }
        rebuildPresetList(name);
        commitChange();
        status_->setText(SC_TR("Saved preset “%1”.").arg(name));
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
            for (const auto &device : latest) if (!knownNames_.contains(device.name)) added.append(device);
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
                outputCombo_->addItem(SC_TR("Automatic (follow connected devices)"), QString());
                for (const auto &device : devices_) outputCombo_->addItem(device.description, device.name);
                index = outputCombo_->findData(selectedName);
                outputCombo_->setCurrentIndex(index >= 0 ? index : 0);
                outputCombo_->blockSignals(false);
            }
            power_->setEnabled(!devices_.isEmpty());
            if (!power_->isChecked()) return;
            bool reconnectDisconnectedOutput = false;
#ifdef Q_OS_WIN
            // Device invalidation may stop WASAPI before this refresh observes
            // the unplug. Keep the user's enabled state while choosing a new
            // physical output, provided crash recovery is still available.
            reconnectDisconnectedOutput = audio_.routeHealthy() &&
                !audio_.target().isEmpty() && findDevice(audio_.target()).name.isEmpty();
#endif
            if (!audio_.active() && !reconnectDisconnectedOutput) {
                power_->setChecked(false);
                showError(SC_TR("The audio processor stopped unexpectedly."));
                return;
            }
#ifdef Q_OS_WIN
            // A direct Windows choice bypasses the managed route. A pinned
            // output must show Off even when Windows selects another device.
            // Leave unplug recovery and automatic-follow switching available.
            const auto windowsOutput = defaultSink();
            const bool pinnedOutputBypassed = !selectedName.isEmpty() && index >= 0 &&
                !findDevice(audio_.target()).name.isEmpty() &&
                !findDevice(windowsOutput).name.isEmpty();
            if (windowsOutput == audio_.target() || pinnedOutputBypassed) {
                power_->setChecked(false);
                status_->setText(SC_TR("Equalizer is off. Windows selected the physical output directly."));
                return;
            }
#endif
            if (audio_.legacyVolumeManaged() && defaultSink() != kSink) {
                power_->setChecked(false);
                return;
            }
#ifndef Q_OS_WIN
            if (audio_.smart() && defaultSink() == kSink)
                command("pactl", {"set-default-sink", audio_.target()});
#endif
            Device desired;
            if (!selectedName.isEmpty() && index >= 0) desired = findDevice(selectedName);
            else if (!selectedName.isEmpty() && index < 0) {
                desired = bestDevice(devices_);
                status_->setText(SC_TR("Selected output was unplugged. Switched to automatic output."));
            } else if (!initial && !added.isEmpty()) desired = bestDevice(added);
            else if (!findDevice(audio_.target()).name.isEmpty()) {
                desired = findDevice(audio_.target());
                const auto current = defaultSink();
                if (current != kSink && !findDevice(current).name.isEmpty()) desired = findDevice(current);
            } else desired = bestDevice(devices_);
            if (desired.name != audio_.target()) {
                if (desired.name.isEmpty()) {
                    meter_.stop();
                    audio_.stop();
                    power_->setChecked(false);
                    status_->setText(SC_TR("No output device is connected."));
                } else {
                    meter_.stop();
                    audio_.start(desired, processingBands(), outputGainDb(), balance_->value());
                    if (isVisible()) meter_.start();
                    showPlaybackStatus(desired);
                }
            }
        } catch (const std::exception &error) { showError(error.what()); }
    }

    void outputChanged() {
        if (!power_->isChecked()) return;
        const auto desired = selectedDevice();
        if (desired.name.isEmpty() || desired.name == audio_.target()) return;
        try {
            meter_.stop();
            audio_.start(desired, processingBands(), outputGainDb(), balance_->value());
            if (isVisible()) meter_.start();
            showPlaybackStatus(desired);
        } catch (const std::exception &error) { showError(error.what()); }
    }

    void togglePower(bool on) {
        power_->setText(on ? SC_TR("Equalizer on") : SC_TR("Equalizer off"));
        if (on) {
            const auto device = selectedDevice();
            if (device.name.isEmpty()) { power_->setChecked(false); showError(SC_TR("No output device is available.")); return; }
            try {
                audio_.start(device, processingBands(), outputGainDb(), balance_->value());
                if (isVisible()) meter_.start();
                showPlaybackStatus(device);
            } catch (const std::exception &error) {
                power_->setChecked(false);
                showError(error.what());
            }
        } else {
            meter_.stop();
            audio_.stop();
            status_->setText(SC_TR("Equalizer is off. Your audio uses its normal output."));
        }
    }

    void showError(const QString &message) { status_->setText(SC_TR("Audio error: %1").arg(message)); }

    AudioEngine audio_;
    MicrophoneEngine microphone_;
    QProcess calibration_;
    QByteArray calibrationOutput_;
    bool calibrating_ = false;
    bool calibrationCancelled_ = false;
    SpectrumMonitor meter_;
    Bands bands_;
    QList<Device> devices_;
    QStringList knownNames_;
    QList<InputDevice> inputs_;
    QStringList knownInputNames_;
    QString micDisconnectNotice_;
    QMap<QString, Bands> custom_;
    QTabWidget *tabs_ = nullptr;
    QScrollArea *scroll_ = nullptr;
    QComboBox *outputCombo_ = nullptr;
    QComboBox *inputCombo_ = nullptr;
    QCheckBox *micPower_ = nullptr;
    std::array<QSlider *, 4> micSliders_{};
    std::array<QLabel *, 4> micLabels_{};
    QSlider *micGain_ = nullptr;
    QLabel *micGainValue_ = nullptr;
    QLabel *micStatus_ = nullptr;
    QPushButton *calibrationStart_ = nullptr;
    QPushButton *calibrationStop_ = nullptr;
    QComboBox *calibrationMode_ = nullptr;
    QSpinBox *calibrationLevel_ = nullptr;
    QLabel *calibrationStatus_ = nullptr;
    QComboBox *presetCombo_ = nullptr;
    QPushButton *savePresetButton_ = nullptr;
    QPushButton *resetButton_ = nullptr;
    QPushButton *lockButton_ = nullptr;
    QPushButton *undoButton_ = nullptr;
    QCheckBox *power_ = nullptr;
    QSlider *outputGain_ = nullptr;
    QLabel *outputGainValue_ = nullptr;
    QSlider *balance_ = nullptr;
    QLabel *balanceValue_ = nullptr;
    OverallLevelMeter *overallLevel_ = nullptr;
    QSpinBox *levelRefresh_ = nullptr;
    QCheckBox *peakMarkers_ = nullptr;
    QLabel *peakStatus_ = nullptr;
    QLabel *status_ = nullptr;
    QLabel *headroom_ = nullptr;
    QSpinBox *countBox_ = nullptr;
    QScrollArea *bandScroll_ = nullptr;
    CurveWidget *curve_ = nullptr;
    QDoubleSpinBox *frequencyBox_ = nullptr;
    QDoubleSpinBox *gainBox_ = nullptr;
    QDoubleSpinBox *qBox_ = nullptr;
    QVector<QSlider *> sliders_;
    QVector<QLabel *> gainLabels_;
    QVector<QPushButton *> frequencyButtons_;
    QVector<BandLevelMeter *> levelBars_;
    QTimer monitor_;
    QProcess volumeEvents_;
    QByteArray volumeEventBuffer_;
    QSystemTrayIcon *tray_ = nullptr;
    QAction *trayToggle_ = nullptr;
    int selected_ = 0;
    EqSnapshot currentSnapshot_;
    QVector<EqSnapshot> undoStack_;
    QElapsedTimer changeClock_;
    QObject *lastChangeSource_ = nullptr;
    bool snapshotReady_ = false;
    bool restoring_ = false;
    bool changing_ = false;
    bool backgroundNoticeShown_ = false;
};

} // namespace

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--windows-audio-setup-test") == 0 || std::strcmp(argv[i], "--ui-self-test") == 0 || std::strcmp(argv[i], "--windows-live-manual-route-test") == 0 || std::strcmp(argv[i], "--windows-live-conflict-test") == 0) {
            qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &message) {
                const auto utf8 = message.toUtf8();
                std::fprintf(stderr, "%s\n", utf8.constData());
                std::fflush(stderr);
            });
        }
    }
    if (argc == 6 && QString::fromLocal8Bit(argv[1]) == "--calibration-worker") {
        QCoreApplication workerApp(argc, argv);
        bool valid = false;
        const int level = QString::fromLocal8Bit(argv[4]).toInt(&valid);
        const auto mode = QString::fromLocal8Bit(argv[5]);
        if (!valid || (mode != "sweep" && mode != "tones")) return 2;
        return runCalibration(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]), level,
                              mode == "sweep");
    }
#ifndef Q_OS_WIN
    if (argc == 5 && QString::fromLocal8Bit(argv[1]) == "--volume-guardian") {
        QCoreApplication guardianApp(argc, argv);
        return guardOutputVolume(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]),
                                 QString::fromLocal8Bit(argv[4]));
    }
#endif
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("SoundCurrent");
    QCoreApplication::setApplicationName("soundcurrent-eq");
    QTemporaryDir testSettings;
    if (app.arguments().contains("--windows-audio-setup-test") || app.arguments().contains("--windows-live-conflict-test") || app.arguments().contains("--windows-live-manual-route-test") || app.arguments().contains("--windows-live-hotplug-test") || app.arguments().contains("--windows-live-ui-test") || app.arguments().contains("--ui-self-test") || app.arguments().contains("--localization-ui-test")) {
#ifndef Q_OS_WIN
        const QJsonArray unpluggedPorts{QJsonObject{{"name", "rear-mic"}, {"availability", "not available"}}};
        if (inputPortAvailable(QJsonObject{{"active_port", "rear-mic"}, {"ports", unpluggedPorts}}))
            qFatal("Disconnected microphone jack remains selectable");
        const QJsonArray usbPorts{QJsonObject{{"name", "mic"}, {"availability", "availability unknown"}}};
        if (!inputPortAvailable(QJsonObject{{"active_port", "mic"}, {"ports", usbPorts}}))
            qFatal("USB microphone without jack detection was hidden");
#endif
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, testSettings.path());
    }
    soundcurrent::i18n::Runtime localization;
    localization.initialize(app.arguments().contains("--ui-self-test"));
    QGuiApplication::setDesktopFileName("io.github.rhamenator.SoundCurrentEQ");
    app.setWindowIcon(QIcon::fromTheme("io.github.rhamenator.SoundCurrentEQ", QIcon(":/app.ico")));
    if (app.arguments().size() == 3 && app.arguments()[1] == "--check-amplifier-profile") {
        QFile file(app.arguments()[2]);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) return 2;
        return parseAmplifierProfile(QJsonDocument::fromJson(file.readAll()).object()) ? 0 : 2;
    }
    if ((app.arguments().size() == 4 && app.arguments()[1] == "--dump-speaker-controls") ||
        (app.arguments().size() == 5 && app.arguments()[1] == "--dump-speaker-filter")) {
        const bool graph = app.arguments()[1] == "--dump-speaker-filter";
        const auto model = app.arguments()[graph ? 3 : 2], preset = app.arguments()[graph ? 4 : 3];
        if (!builtinShapes().contains(preset)) return 2;
        auto bands = builtinProfile(preset, kDefaultBands); bands.resize(kMaxBands);
        bool found = model == "None";
        for (const auto &profile : speakerProfiles()) if (profile.id == model) { bands.append(profile.filters); found = true; }
        if (!found) return 2;
        QTextStream(stdout) << (graph ? filterConfig(app.arguments()[2], bands) : filterControls(bands));
        return 0;
    }
    if (app.arguments().size() == 3 && app.arguments()[1] == "--dump-filter-config") {
        QTextStream(stdout) << filterConfig(app.arguments()[2], defaultBands(kDefaultBands));
        return 0;
    }
    if (app.arguments().size() == 4 && app.arguments()[1] == "--dump-mic-config") {
        bool valid = false;
        const int channels = app.arguments()[3].toInt(&valid);
        if (!valid || channels < 1 || channels > 2) return 2;
        QTextStream(stdout) << micConfig(app.arguments()[2], channels, {}, 0.0, false);
        return 0;
    }
#ifndef Q_OS_WIN
    if (app.arguments().contains("--mic-self-test")) {
        try {
            const auto available = inputDevices();
            if (available.isEmpty()) throw std::runtime_error("No microphone is connected");
            const auto before = defaultSource();
            auto selected = available.front();
            for (const auto &device : available) if (device.name == before) selected = device;
            MicrophoneEngine test;
            test.start(selected, {}, 0.0);
            if (nodeId(kMicSource) < 0) throw std::runtime_error("Microphone filter is missing");
            if (!command("pw-link", {"-l"}).contains(selected.name + ":capture_"))
                throw std::runtime_error("Microphone filter did not connect to the selected device");
            test.update({0.0, 0.0, 3.0, 0.0}, 2.0);
            test.stop();
            if (defaultSource() != before) throw std::runtime_error("Original microphone was not restored");
            qInfo("Microphone routing self-test passed using %s", qPrintable(selected.description));
            return 0;
        } catch (const std::exception &error) {
            qCritical("Microphone routing self-test failed: %s", error.what());
            return 1;
        }
    }
#endif
    if (app.arguments().size() >= 3 && app.arguments().size() <= 5 &&
        app.arguments()[1] == "--dump-preset-controls") {
        const auto name = app.arguments()[2];
        if (!builtinShapes().contains(name)) return 2;
        bool valid = true;
        const double gain = app.arguments().size() >= 4 ? app.arguments()[3].toDouble(&valid) : 0.0;
        if (!valid || !std::isfinite(gain) || gain < soundcurrent::kMinPostGainDb || gain > 12.0) return 2;
        const int balance = app.arguments().size() == 5 ? app.arguments()[4].toInt(&valid) : 0;
        if (!valid || balance < -100 || balance > 100) return 2;
        QTextStream(stdout) << filterControls(builtinProfile(name, kDefaultBands), gain, balance);
        return 0;
    }
#ifndef Q_OS_WIN
    if (app.arguments().contains("--self-test")) {
        try {
            const auto standard = defaultBands(kDefaultBands);
            if (standard.size() != 15 || standard.first().frequency != 25 || standard.last().frequency != 16000)
                throw std::runtime_error("Default band layout is invalid");
            const auto full = remapBands(standard, kMaxBands);
            const auto parsed = parseBands(QJsonObject{{"bands", serializeBands(full)}});
            if (!parsed || parsed->size() != kMaxBands) throw std::runtime_error("Custom preset round trip failed");
            QJsonArray legacy;
            for (int i = 0; i < 9; ++i) legacy.append(double(i - 4));
            const auto migrated = parseBands(legacy);
            if (!migrated || migrated->size() != 9 || migrated->first().gain != -4)
                throw std::runtime_error("Nine-band preset migration failed");
            auto available = devices();
            if (available.isEmpty()) throw std::runtime_error("No audio output devices found");
            const auto current = defaultSink();
            auto selected = available.front();
            for (const auto &device : available) if (device.name == current) selected = device;
            const auto before = defaultSink();
            const auto volumeBefore = sinkState(selected.name);
            AudioEngine test;
            test.start(selected, builtinProfile("Bass Boost", kDefaultBands));
            auto adjusted = builtinProfile("Clear Voice", kMaxBands);
            adjusted[12].frequency = 320.0;
            adjusted[12].q = 2.0;
            test.update(adjusted);
            test.updateGain(3.0, -20);
            if (defaultSink() != (test.smart() ? selected.name : kSink))
                throw std::runtime_error("Equalizer chose the wrong system output");
            if (test.smart() && sinkState(kSink).volumes.first() != "65536")
                throw std::runtime_error("Transparent filter applies a second volume reduction");
            if (test.legacyVolumeManaged() && sinkState(selected.name).volumes.first() != "65536")
                throw std::runtime_error("Legacy filter still applies a second volume reduction");
            test.stop();
            if (defaultSink() != before) throw std::runtime_error("Original output was not restored");
            const auto volumeAfter = sinkState(selected.name);
            if (volumeAfter.volumes != volumeBefore.volumes || volumeAfter.muted != volumeBefore.muted)
                throw std::runtime_error("Original output volume was not restored");
            qInfo("Audio routing self-test passed using %s", qPrintable(selected.description));
            return 0;
        } catch (const std::exception &error) {
            qCritical("Audio routing self-test failed: %s", error.what());
            return 1;
        }
    }
#endif
    app.setStyleSheet(R"(
        QWidget { background: #111827; color: #e8edf6; font-size: 13px; }
        QGroupBox { background: #1c293c; border: 1px solid #33445e; border-radius: 12px;
                    margin-top: 14px; padding: 14px; font-weight: 700; }
        QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 5px; }
        QLabel { background: transparent; }
        QCheckBox { background: transparent; }
        QCheckBox#powerToggle { background: #2d405a; border: 1px solid #4a5d77;
                                border-radius: 7px; padding: 7px 10px; }
        QCheckBox#powerToggle:hover { background: #385572; }
        QCheckBox#powerToggle:checked { background: #1f746e; border-color: #55d7c3; }
        QCheckBox#powerToggle::indicator { width: 16px; height: 16px; margin-right: 4px; }
        QLabel#value { color: #90d9ce; font-weight: 700; }
        QLabel#status { color: #90d9ce; }
        QPushButton, QComboBox { background: #2d405a; border: 1px solid #4a5d77;
                                border-radius: 7px; padding: 7px 10px; }
        QPushButton:hover, QComboBox:hover { background: #385572; }
        QPushButton:checked { background: #1f746e; border-color: #55d7c3; }
        QComboBox QAbstractItemView { background: #26374d; selection-background-color: #2c9d91; }
        QSlider::groove:vertical { background: #344762; width: 7px; border-radius: 3px; }
        QSlider::handle:vertical { background: #eafbf7; height: 16px; margin: 0 -6px; border-radius: 8px; }
        QScrollArea { border: none; }
        QTabWidget::pane { border: none; }
        QTabBar::tab { background: #1c293c; border: 1px solid #33445e;
                      padding: 10px 20px; margin-right: 4px; }
        QTabBar::tab:selected { background: #1f746e; border-color: #55d7c3; }
        QTabBar::tab:hover { background: #385572; }
    )");
#ifdef Q_OS_WIN
#include "windows_audio_setup_test.inc"
#include "windows_live_ui_test.inc"
#endif
    if (app.arguments().contains("--ui-self-test")) {
        QJsonObject ampTest{{"schema", 1}, {"model", "Test fixture"},
            {"measurementSource", "https://example.invalid/test"}, {"conditions", "8 ohms; analog input; controls flat"},
            {"filters", QJsonArray{QJsonObject{{"type", "HS"}, {"frequency", 8000}, {"gain", -1.0}, {"q", 0.707}}}}};
        if (!parseAmplifierProfile(ampTest)) qFatal("Valid measured amplifier profile rejected");
        ampTest.insert("conditions", "");
        if (parseAmplifierProfile(ampTest)) qFatal("Amplifier profile without measurement conditions accepted");
        ampTest.insert("conditions", "8 ohms"); ampTest.insert("filters", QJsonArray{});
        if (parseAmplifierProfile(ampTest)) qFatal("Empty amplifier profile accepted");
        qInfo("UI self-test: constructing window");
        MainWindow testWindow(false);
        qInfo("UI self-test: window constructed");
        SpectrumMonitor spectrumTest(true);
        if (spectrumTest.interval() != 16) qFatal("Default level interval is not 16 ms");
        spectrumTest.setInterval(5);
        if (spectrumTest.interval() != 5) qFatal("Five millisecond level interval is unavailable");
        spectrumTest.setInterval(1);
        if (spectrumTest.interval() != 1) qFatal("One millisecond level interval is unavailable");
        spectrumTest.setInterval(0);
        if (spectrumTest.interval() != 1) qFatal("Level interval minimum is not enforced");
        spectrumTest.setInterval(500);
        if (spectrumTest.interval() != 100) qFatal("Level interval maximum is not enforced");
        spectrumTest.setProfile(defaultBands(kDefaultBands), 0.0);
        if (balanceFactors(0) != std::array<double, 2>{1.0, 1.0} ||
            balanceFactors(-100) != std::array<double, 2>{1.0, 0.0} ||
            balanceFactors(100) != std::array<double, 2>{0.0, 1.0})
            qFatal("Balance must attenuate only the opposite channel");
        const auto config = filterConfig("test_output", defaultBands(kDefaultBands));
        if (!config.contains("inputs = [ \"left_preamp:In\" \"right_preamp:In\" ]") ||
            !config.contains("outputs = [ \"left_output_gain:Out\" \"right_output_gain:Out\" ]"))
            qFatal("Filter graph channels are not mapped separately");
        const auto micMono = micConfig("test_microphone", 1, {}, 0.0, true);
        const auto micStereo = micConfig("test_microphone", 2, {}, 0.0, true);
        if (!micMono.contains("audio.position = [ MONO ]") ||
            !micMono.contains("label = bq_highpass") ||
            !micMono.contains("filter.smart = true") ||
            !micStereo.contains("audio.position = [ FL FR ]") ||
            !micStereo.contains("right_mic_4:Out"))
            qFatal("Microphone filter layouts are invalid");
        QByteArray calibrationPcm(kCalibrationRate / 4 * 2, '\0');
        for (int i = 0; i < kCalibrationRate / 4; ++i) {
            const auto sample = int16_t(std::lround(3000.0 *
                std::sin(2.0 * std::numbers::pi * 1000.0 * i / kCalibrationRate)));
            calibrationPcm[2 * i] = char(uint16_t(sample) & 0xff);
            calibrationPcm[2 * i + 1] = char(uint16_t(sample) >> 8);
        }
        if (std::abs(toneAmplitude(calibrationPcm, 1000) - 3000.0) > 5.0 ||
            toneAmplitude(calibrationPcm, 2000) > 10.0)
            qFatal("Calibration tone analysis is inaccurate");
        checkCalibrationClipping(calibrationPcm);
        QByteArray clippedCapture(4096, '\x7f');
        for (int i = 0; i < clippedCapture.size(); i += 2) clippedCapture[i] = '\xff';
        bool rejectedClipping = false;
        try { checkCalibrationClipping(clippedCapture); }
        catch (const std::runtime_error &) { rejectedClipping = true; }
        if (!rejectedClipping) qFatal("Clipped microphone capture was accepted for calibration");
        QTemporaryDir sweepTestDir;
        const auto sweepReference = writeCalibrationSweep(sweepTestDir.filePath("sweep.wav"), -48);
        QByteArray sweepRecording((sweepReference.size() + kCalibrationRate / 10) * 2, '\0');
        for (int i = 0; i < sweepReference.size(); ++i) {
            const auto sample = int16_t(std::lround(sweepReference[i] * 0.5));
            sweepRecording[2 * (i + kCalibrationRate / 10)] = char(uint16_t(sample) & 0xff);
            sweepRecording[2 * (i + kCalibrationRate / 10) + 1] = char(uint16_t(sample) >> 8);
        }
        const auto sweptLevels = analyzeSweep(sweepRecording, QByteArray(24000, '\0'), sweepReference);
        if (sweptLevels.size() != int(kCalibrationFrequencies.size()))
            qFatal("Sweep analysis returned the wrong band count");
        for (const auto &level : sweptLevels)
            if (!level.isDouble() || std::abs(level.toDouble() - 0.5) > 0.12) {
                qWarning("Unexpected synthetic sweep level: %s", qPrintable(QString::fromUtf8(QJsonDocument(sweptLevels).toJson(QJsonDocument::Compact))));
                qFatal("Sweep analysis did not recover a delayed quiet signal");
            }
        const auto rejectedLevels = analyzeSweep(sweepRecording, sweepRecording, sweepReference);
        for (const auto &level : rejectedLevels)
            if (!level.isNull()) qFatal("Background noise was mistaken for a calibration signal");
        QJsonArray trialLevels{50.0, 80.0, 100.0, 150.0, 200.0, 300.0, 400.0, 500.0,
                               600.0, 700.0, 800.0, 900.0};
        auto trialSuggestion = calibrationSuggestion(QJsonObject{{"levels", trialLevels}}, defaultBands(15));
        if (!trialSuggestion || trialSuggestion->changed < 2 ||
            trialSuggestion->bands[3].gain <= 0.0)
            qFatal("Calibration suggestion was not generated");
        QVector<double> testLevels;
        double testPeak = 0.0;
        int levelUpdates = 0;
        spectrumTest.onLevels = [&](const QVector<double> &levels, double peak) {
            testLevels = levels;
            testPeak = peak;
            ++levelUpdates;
        };
        QByteArray tone(4096 * 4, '\0');
        for (int i = 0; i < 4096; ++i) {
            const auto sample = int16_t(std::lround(8192.0 *
                std::sin(2.0 * std::numbers::pi * 100.0 * i / 48000.0)));
            for (int channel = 0; channel < 2; ++channel) {
                tone[i * 4 + channel * 2] = char(uint16_t(sample) & 0xff);
                tone[i * 4 + channel * 2 + 1] = char(uint16_t(sample) >> 8);
            }
        }
        spectrumTest.analyzePcmForTest(tone);
        if (testLevels.size() != kDefaultBands || testLevels[3] < 0.15 ||
            std::abs(testPeak - 0.25) > 0.01)
            qFatal("FFT level analysis failed");
        spectrumTest.analyzePcmForTest(tone.left(2048 * 4));
        if (levelUpdates != 2) qFatal("Overlapping FFT window did not refresh");
        spectrumTest.analyzePcmForTest(QByteArray{});
        if (levelUpdates != 2) qFatal("Level display refreshed without new audio");
        spectrumTest.setProfile(defaultBands(kDefaultBands), 6.0);
        spectrumTest.analyzePcmForTest(tone.left(2048 * 4));
        if (std::abs(testPeak - 0.5) > 0.03 || testLevels[3] < 0.3)
            qFatal("Output gain was not reflected in the level estimate");
        QByteArray leftOnly = tone;
        for (int i = 0; i < 4096; ++i) {
            leftOnly[i * 4 + 2] = '\0';
            leftOnly[i * 4 + 3] = '\0';
        }
        spectrumTest.setProfile(defaultBands(kDefaultBands), 0.0, 100);
        spectrumTest.analyzePcmForTest(leftOnly);
        if (testPeak > 0.001) qFatal("Muted left channel still raises the overall level");
        spectrumTest.setProfile(defaultBands(kDefaultBands), 0.0, -100);
        spectrumTest.analyzePcmForTest(leftOnly);
        if (std::abs(testPeak - 0.25) > 0.01)
            qFatal("Balance is missing from the overall level estimate");
        BandLevelMeter peakTest;
        peakTest.resize(11, 140);
        peakTest.setPeakMarkersEnabled(true);
        peakTest.setLevel(-3.0);
        peakTest.setLevel(-20.0);
        const auto withMarker = peakTest.grab().toImage();
        peakTest.setPeakMarkersEnabled(false);
        const auto withoutMarker = peakTest.grab().toImage();
        bool markerVisible = false;
        for (int y = 0; y < withMarker.height(); ++y)
            for (int x = 0; x < withMarker.width(); ++x) {
                const auto color = withMarker.pixelColor(x, y);
                if (color != withoutMarker.pixelColor(x, y) &&
                    color.red() > 200 && color.green() > 200 && color.blue() > 200)
                    markerVisible = true;
            }
        if (!markerVisible) qFatal("Peak marker did not follow its toggle");
        OverallLevelMeter overallTest;
        overallTest.resize(200, 16);
        overallTest.setPeakMarkersEnabled(true);
        overallTest.setLevel(-3.0);
        overallTest.setLevel(-20.0);
        const auto overallWithMarker = overallTest.grab().toImage();
        overallTest.setPeakMarkersEnabled(false);
        const auto overallWithoutMarker = overallTest.grab().toImage();
        if (overallWithMarker == overallWithoutMarker)
            qFatal("Overall meter peak marker did not follow its toggle");
        if (builtinShapes().size() < 30) qFatal("Preset library is incomplete");
        if (testWindow.windowTitle() != "SoundCurrent EQ") qFatal("Window title is missing");
        for (const auto *label : testWindow.findChildren<QLabel *>()) {
            if (label->text() == "SoundCurrent EQ" ||
                label->text() == "Shape your sound with an adjustable parametric equalizer.")
                qFatal("Removed in-window heading is still visible");
        }
        auto findSpin = [&testWindow](const QString &name) {
            for (auto *spin : testWindow.findChildren<QSpinBox *>())
                if (spin->accessibleName() == name) return spin;
            return static_cast<QSpinBox *>(nullptr);
        };
        auto findDouble = [&testWindow](const QString &name) {
            for (auto *spin : testWindow.findChildren<QDoubleSpinBox *>())
                if (spin->accessibleName() == name) return spin;
            return static_cast<QDoubleSpinBox *>(nullptr);
        };
        auto *count = findSpin("Number of equalizer bands");
        auto *frequency = findDouble("Selected band frequency");
        auto *gain = findDouble("Selected band gain");
        auto *q = findDouble("Selected band filter Q");
        if (!count || !frequency || !gain || !q) qFatal("UI controls missing");
        QComboBox *presets = nullptr;
        for (auto *combo : testWindow.findChildren<QComboBox *>())
            if (combo->accessibleName() == "Listening preset") presets = combo;
        if (!presets) qFatal("Preset menu is missing");
        QPushButton *quit = nullptr;
        for (auto *button : testWindow.findChildren<QPushButton *>())
            if (button->accessibleName() == "Quit SoundCurrent EQ") quit = button;
        if (!quit) qFatal("Quit button is missing");
        QCheckBox *power = nullptr;
        for (auto *check : testWindow.findChildren<QCheckBox *>())
            if (check->accessibleName() == "Equalizer on or off") power = check;
        if (!power || power->objectName() != "powerToggle") qFatal("Power cartouche is missing");
        QComboBox *microphoneInput = nullptr;
        for (auto *combo : testWindow.findChildren<QComboBox *>())
            if (combo->accessibleName() == "Microphone input device") microphoneInput = combo;
        if (!microphoneInput) qFatal("Microphone device selection is missing");
        if (microphoneInput->count() == 1 && microphoneInput->itemText(0) != "Plug in your microphone to select a microphone profile")
            qFatal("Missing microphone connection prompt");
        QPushButton *calibrationStart = nullptr, *calibrationStop = nullptr;
        for (auto *button : testWindow.findChildren<QPushButton *>()) {
            if (button->accessibleName() == "Measure speaker room and microphone response") calibrationStart = button;
            if (button->text() == "Stop tones") calibrationStop = button;
        }
        if (!calibrationStart || !calibrationStop || calibrationStop->isEnabled())
            qFatal("Calibration start or stop control is invalid");
        QComboBox *calibrationMode = nullptr;
        for (auto *combo : testWindow.findChildren<QComboBox *>())
            if (combo->accessibleName() == "Calibration test signal") calibrationMode = combo;
        if (!calibrationMode || calibrationMode->currentData().toString() != "sweep" ||
            calibrationMode->findData("tones") < 0)
            qFatal("Quiet sweep is not the default calibration signal");
        QSpinBox *calibrationLevel = nullptr;
        for (auto *spin : testWindow.findChildren<QSpinBox *>())
            if (spin->accessibleName() == "Calibration tone level") calibrationLevel = spin;
        if (!calibrationLevel || calibrationLevel->minimum() != -54 ||
            calibrationLevel->maximum() != -5 || calibrationLevel->value() != -24)
            qFatal("Calibration level limits or default are invalid");
        int micControlsFound = 0;
        for (auto *slider : testWindow.findChildren<QSlider *>())
            if (slider->accessibleName().startsWith("Microphone ")) ++micControlsFound;
        if (micControlsFound != 5) qFatal("Microphone tone and gain controls are missing");
        QSlider *outputGain = nullptr;
        QSlider *balance = nullptr;
        for (auto *slider : testWindow.findChildren<QSlider *>()) {
            if (slider->accessibleName() == "Post gain after equalization") outputGain = slider;
            if (slider->accessibleName() == "Left right balance") balance = slider;
        }
        if (!outputGain || outputGain->minimum() != -120 || outputGain->maximum() != 24 ||
            !balance || balance->minimum() != -100 || balance->maximum() != 100)
            qFatal("Post gain or balance slider is missing");
        bool overallMeter = false;
        for (auto *widget : testWindow.findChildren<QWidget *>())
            if (dynamic_cast<OverallLevelMeter *>(widget)) overallMeter = true;
        if (!overallMeter) qFatal("Overall level indicator is missing");
        auto *levelRefresh = findSpin("Level indicator refresh interval");
        if (!levelRefresh || levelRefresh->minimum() != 1 || levelRefresh->maximum() != 100 ||
            levelRefresh->singleStep() != 1)
            qFatal("Level refresh control is missing");
        QCheckBox *peakMarkers = nullptr;
        for (auto *check : testWindow.findChildren<QCheckBox *>())
            if (check->accessibleName() == "Show peak markers on frequency levels") peakMarkers = check;
        if (!peakMarkers) qFatal("Peak marker toggle is missing");
        if (presets->currentText() != "Flat") qFatal("Flat is not the default preset");
        if (presets->findText("Loudness") < 0) qFatal("Loudness preset is missing");
        if (presets->findText("Entertainment") >= 0) qFatal("Category title appears as a preset");
        for (auto it = builtinShapes().begin(); it != builtinShapes().end(); ++it) {
            if (presets->findText(it.key()) < 0) qFatal("A built-in preset is missing from the menu");
            presets->setCurrentText(it.key());
            if (std::abs(gain->value() - builtinProfile(it.key(), kDefaultBands).first().gain) > 0.11)
                qFatal("A built-in preset did not update the band controls");
        }
        presets->setCurrentText("Flat");
        testWindow.show();
        app.processEvents();
        if (auto *display = testWindow.screen()) {
            const auto available = display->availableGeometry();
            if (testWindow.width() > available.width() || testWindow.height() > available.height())
                qFatal("Window exceeds the display size");
        }
        if (qEnvironmentVariableIsSet("SOUNDCURRENT_SCREENSHOT"))
            testWindow.grab().save(qEnvironmentVariable("SOUNDCURRENT_SCREENSHOT"));
        presets->showPopup();
        app.processEvents();
        if (presets->maxVisibleItems() != 12 ||
            presets->view()->verticalScrollBar()->maximum() <= 0)
            qFatal("Preset menu does not scroll");
        presets->hidePopup();
        testWindow.hide();
        presets->setCurrentText("Deep Bass");
        if (gain->value() < 6.0) qFatal("Deep Bass preset did not change the bands");
        presets->setCurrentText("Flat");
        if (gain->value() != 0.0) qFatal("Flat preset did not reset the bands");
        auto bandSliderCount = [&testWindow] {
            int total = 0;
            for (auto *slider : testWindow.findChildren<QSlider *>())
                if (slider->accessibleName().startsWith("Band ") &&
                    slider->accessibleName().endsWith(" gain")) ++total;
            return total;
        };
        count->setValue(31);
        if (bandSliderCount() != 31) qFatal("31-band layout failed");
        int levelCount = 0;
        for (auto *widget : testWindow.findChildren<QWidget *>())
            if (dynamic_cast<BandLevelMeter *>(widget)) ++levelCount;
        if (levelCount != 31) qFatal("Level indicators are missing");
        frequency->setValue(22);
        gain->setValue(4);
        q->setValue(1.8);
        if (frequency->value() != 22 || gain->value() != 4 || q->value() != 1.8)
            qFatal("Selected-band editing failed");
        count->setValue(15);
        if (bandSliderCount() != 15) qFatal("Band-count change failed");
        QPushButton *lock = nullptr, *undo = nullptr;
        for (auto *button : testWindow.findChildren<QPushButton *>()) {
            if (button->accessibleName() == "Lock equalizer settings") lock = button;
            if (button->accessibleName() == "Undo last equalizer change") undo = button;
        }
        if (!lock || !undo) qFatal("Lock or Undo control is missing");
        auto firstBandSlider = [&testWindow] {
            for (auto *slider : testWindow.findChildren<QSlider *>())
                if (slider->accessibleName() == "Band 1 gain") return slider;
            return static_cast<QSlider *>(nullptr);
        };
        presets->setCurrentText("Flat");
        auto *band = firstBandSlider();
        if (!band) qFatal("First band slider is missing");
        band->setValue(2);
        band->setValue(4);
        band->setValue(6);
        if (!undo->isEnabled() || gain->value() != 3.0) qFatal("EQ change was not recorded for Undo");
        undo->click();
        band = firstBandSlider();
        if (!band || band->value() != 0 || presets->currentText() != "Flat")
            qFatal("Undo did not restore the previous band and preset");
        presets->setCurrentText("Deep Bass");
        undo->click();
        if (presets->currentText() != "Flat" || firstBandSlider()->value() != 0)
            qFatal("Undo did not restore the previous preset");
        soundcurrent::EnhancementControls *effects=nullptr;
        for(auto *widget:testWindow.findChildren<QWidget *>())if(auto *controls=dynamic_cast<soundcurrent::EnhancementControls *>(widget))effects=controls;
        if(!effects)qFatal("Enhancement controls missing");
        auto *bassEffect=effects->findChildren<QSlider *>().at(4);const int originalBass=bassEffect->value();
        bassEffect->setValue(50);undo->click();if(bassEffect->value()!=originalBass)qFatal("Enhancement undo failed");
        const int originalOutputGain = outputGain->value();
        outputGain->setValue(-120);
        if (QSettings().value("outputGainDb").toDouble() != -60.0)
            qFatal("Low post gain was not saved immediately");
        {
            auto reopened = std::make_unique<MainWindow>(false);
            QSlider *restored = nullptr;
            for (auto *slider : reopened->findChildren<QSlider *>())
                if (slider->accessibleName() == "Post gain after equalization") restored = slider;
            if (!restored || restored->value() != -120)
                qFatal("Low post gain did not survive reopening");
        }
        undo->click();
        if (outputGain->value() != originalOutputGain)
            qFatal("Undo did not restore post gain");
        const int originalBalance = balance->value();
        balance->setValue(originalBalance == balance->maximum()
                              ? originalBalance - 1 : originalBalance + 1);
        undo->click();
        if (balance->value() != originalBalance)
            qFatal("Undo did not restore balance");
        QComboBox *speakers = nullptr;
        for (auto *combo : testWindow.findChildren<QComboBox *>())
            if (combo->accessibleName() == "Speaker model profile") speakers = combo;
        if (!speakers || speakers->count() != speakerProfiles().size() + 1 || speakers->currentData().toString() != "")
            qFatal("Speaker profiles are missing or correction is enabled by default");
        QComboBox *manufacturer=nullptr,*speakerType=nullptr;
        for(auto *combo:testWindow.findChildren<QComboBox *>()){if(combo->accessibleName()=="Speaker manufacturer")manufacturer=combo;if(combo->accessibleName()=="Speaker type")speakerType=combo;}
        if(!manufacturer || !speakerType || manufacturer->count()<200 || speakerType->count()<10 || speakers->count()<1000)qFatal("Full speaker taxonomy missing");
        manufacturer->setCurrentText("JBL");if(speakers->count()<10 || !speakers->currentData().toString().isEmpty())qFatal("Speaker filtering applied a correction");
        speakerType->setCurrentText("Bookshelf");if(speakers->count()<2)qFatal("Speaker type filtering failed");
        manufacturer->setCurrentIndex(0);speakerType->setCurrentIndex(0);
        const int kali = speakers->findData("Kali LP-6v2");
        speakers->setCurrentIndex(kali);
        presets->setCurrentText("Bass Boost");
        if (speakers->currentIndex() != kali) qFatal("Listening preset removed speaker correction");
        undo->click();
        if (speakers->currentIndex() != kali) qFatal("Undo preset removed speaker correction");
        undo->click();
        if (!speakers->currentData().toString().isEmpty()) qFatal("Undo did not restore speaker selection");
        speakers->setCurrentIndex(speakers->findData("Kali LP-6v2"));
        manufacturer->setCurrentText("JBL");if(speakers->currentData().toString()!="Kali LP-6v2")qFatal("Browsing changed active correction");
        const int jbl=speakers->findData("JBL 305P Mark ii");if(jbl<0)qFatal("Filtered JBL speaker missing");speakers->setCurrentIndex(jbl);
        manufacturer->setCurrentText("KEF");undo->click();if(speakers->currentData().toString()!="Kali LP-6v2")qFatal("Undo lost filtered speaker");
        undo->click();manufacturer->setCurrentIndex(0);speakerType->setCurrentIndex(0);
        lock->click();
        if (!lock->isChecked() || firstBandSlider()->isEnabled() || presets->isEnabled() ||
            outputGain->isEnabled() || balance->isEnabled() || speakers->isEnabled() || bassEffect->isEnabled())
            qFatal("Lock did not protect playback EQ controls");
        lock->click();
        if (!firstBandSlider()->isEnabled()) qFatal("Unlock did not restore editing");
        testWindow.show();
        app.processEvents();
        auto *tabs = qobject_cast<QTabWidget *>(testWindow.centralWidget());
        if (!tabs || tabs->count() != 2 || tabs->currentIndex() != 0 || tabs->tabText(0) != "Equalizer")
            qFatal("Equalizer must be the first and initially selected tab");
        auto *outer = qobject_cast<QScrollArea *>(tabs->widget(0));
        auto *settingsPage = qobject_cast<QScrollArea *>(tabs->widget(1));
        if (!outer || !settingsPage || !outer->isAncestorOf(firstBandSlider()) ||
            !outer->isAncestorOf(presets) || !outer->isAncestorOf(outputGain) ||
            !settingsPage->isAncestorOf(speakers))
            qFatal("Equalizer and configuration controls are on the wrong tabs");
        auto *firstGroup = qobject_cast<QGroupBox *>(outer->widget()->layout()->itemAt(0)->widget());
        if (!firstGroup || firstGroup->title() != "Equalizer")
            qFatal("Equalizer must be at the top of the first page");
        for (auto *combo : testWindow.findChildren<QComboBox *>()) {
            if ((combo->accessibleName() == "Output device" ||
                 combo->accessibleName() == "Calibration test signal") &&
                !settingsPage->isAncestorOf(combo))
                qFatal("Device or sweep configuration is outside the settings tab");
        }
        const auto screenshotDirectory = qEnvironmentVariable("SOUNDCURRENT_UI_SCREENSHOT_DIR");
        if (!screenshotDirectory.isEmpty()) {
            testWindow.resize(1050, 920);
            app.processEvents();
            if (!QDir().mkpath(screenshotDirectory) ||
                !testWindow.grab().save(QDir(screenshotDirectory).filePath("equalizer.png")))
                qFatal("Cannot save equalizer UI test screenshot");
        }
        tabs->setCurrentIndex(1);
        app.processEvents();
        if (!screenshotDirectory.isEmpty() &&
            !testWindow.grab().save(QDir(screenshotDirectory).filePath("settings.png")))
            qFatal("Cannot save settings UI test screenshot");
        if (!speakers->isVisible()) qFatal("Settings tab does not display its controls");
        tabs->setCurrentIndex(0);
        testWindow.resize(testWindow.width(), 400);
        app.processEvents();
        if (outer->verticalScrollBar()->maximum() <= 0)
            qFatal("Window cannot scroll to the level indicators");
        outer->verticalScrollBar()->setValue(0);
        band = firstBandSlider();
        const int beforeWheel = band->value();
        QWheelEvent wheel(QPointF(5, 5), QPointF(band->mapToGlobal(QPoint(5, 5))),
                          QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(band, &wheel);
        if (band->value() != beforeWheel || outer->verticalScrollBar()->value() <= 0)
            qFatal("Mouse wheel changed an EQ band instead of scrolling the window");
        tabs->setCurrentIndex(1);
        app.processEvents();
        settingsPage->verticalScrollBar()->setValue(0);
        const int beforeCalibrationWheel = calibrationLevel->value();
        QWheelEvent settingsWheel(QPointF(5, 5),
                                 QPointF(calibrationLevel->mapToGlobal(QPoint(5, 5))),
                                 QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                                 Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(calibrationLevel, &settingsWheel);
        if (calibrationLevel->value() != beforeCalibrationWheel ||
            settingsPage->verticalScrollBar()->value() <= 0)
            qFatal("Mouse wheel changed sweep level instead of scrolling the settings tab");
        tabs->setCurrentIndex(0);
        bool quitRequested = false;
        QObject::connect(&app, &QCoreApplication::aboutToQuit, &testWindow,
                         [&quitRequested] { quitRequested = true; });
        testWindow.show();
        QTimer::singleShot(0, quit, &QPushButton::click);
        if (app.exec() != 0 || !quitRequested) qFatal("Quit button did not exit the application");
        qInfo("UI self-test passed with %lld presets and editable 5–31 band layout",
              static_cast<long long>(builtinShapes().size()));
        return 0;
    }
    if (app.arguments().contains("--localization-ui-test")) {
        auto ownedWindow=std::make_unique<MainWindow>(false);
        auto &window=*ownedWindow;
        auto *preset=window.findChild<QComboBox *>("localizedPresetSelector");
        if(!preset || preset->currentData().toString()!="Flat") qFatal("Localized preset lost its stable ID");
        QDoubleSpinBox *selectedGain=nullptr;
        for(auto *spin:window.findChildren<QDoubleSpinBox *>())
            if(spin->accessibleName()==SC_TR("Selected band gain"))selectedGain=spin;
        if(!selectedGain || selectedGain->value()!=0)qFatal("Localized Flat preset has nonzero gain");
        preset->setCurrentIndex(preset->findData("Night Listening"));
        if(preset->currentData().toString()!="Night Listening") qFatal("Localized preset selection changed its ID");
        const auto expected=builtinProfile("Night Listening",kDefaultBands).front().gain;
        if(std::abs(selectedGain->value()-expected)>0.1)qFatal("Translated preset name blocked its EQ change");
        preset->setCurrentIndex(preset->findData("Flat"));
        if(selectedGain->value()!=0) qFatal("Localized Flat reset failed");
        auto *language=window.findChild<QComboBox *>("uiLanguage");
        auto *format=window.findChild<QComboBox *>("formatLocale");
        if(!language || !format || format->count()<100) qFatal("Locale selection is missing");
        window.show();
        QTimer::singleShot(100, &app, [&] {
            const auto dir=qEnvironmentVariable("SOUNDCURRENT_UI_SCREENSHOT_DIR");
            if(!dir.isEmpty()){
                QDir().mkpath(dir);window.grab().save(dir+"/localized.png");
                auto *tabs=window.findChild<QTabWidget *>();
                for(int page=0;tabs && page<tabs->count();++page){
                    tabs->setCurrentIndex(page);QApplication::processEvents();
                    window.grab().save(dir+"/localized-tab-"+QString::number(page)+".png");
                }
            }
            qInfo("Localization UI: %s -> %s",qPrintable(localization.requested()),qPrintable(localization.loaded()));
            app.quit();
        });
        return app.exec();
    }
#ifdef Q_OS_WIN
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!QDir().mkpath(runtime)) { qCritical("Cannot create user settings directory"); return 1; }
#else
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#endif
    if (runtime.isEmpty() || !QFileInfo(runtime).isDir()) {
        qCritical("A private user runtime directory is required");
        return 1;
    }
    #ifdef Q_OS_WIN
    const auto socketPath = QString("SoundCurrentEQ-%1").arg(QString::number(qHash(runtime), 16));
#else
    const auto socketPath = QDir(runtime).filePath("soundcurrent-eq.sock");
#endif
    QLockFile instanceLock(QDir(runtime).filePath("soundcurrent-eq.lock"));
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock(200)) {
        if (instanceLock.error() == QLockFile::LockFailedError) {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 2000) {
                QLocalSocket client;
                client.connectToServer(socketPath);
                if (client.waitForConnected(200)) {
                    client.write(app.arguments().contains("--quit") ? "Q" : "S");
                    client.waitForBytesWritten(500);
                    client.disconnectFromServer();
                    return 0;
                }
                QThread::msleep(50);
            }
        }
        qCritical("SoundCurrent EQ is already running or its instance lock is unavailable");
        return 1;
    }
    if (app.arguments().contains("--quit")) return 0;
    QLocalServer instanceServer;
    instanceServer.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(socketPath);
    if (!instanceServer.listen(socketPath)) {
        qCritical("Could not create SoundCurrent EQ's local activation socket: %s", qPrintable(instanceServer.errorString()));
        return 1;
    }
    soundcurrent::ProcessingGuard processingGuard;
    auto showConflict=[&](const QString &reason){
        QMessageBox box(QMessageBox::Warning,"Equalizer conflict",reason,QMessageBox::Ok);
        box.setTextFormat(Qt::PlainText);
#ifdef Q_OS_WIN
        if(app.arguments().contains("--windows-live-conflict-test")) {
            bool foundPower=false;
            for(auto *widget:app.topLevelWidgets()) {
                if(auto *power=widget->findChild<QCheckBox *>("powerToggle")) {
                    foundPower=true;
                    if(power->isChecked()) qFatal("Conflict left processing enabled before warning");
                }
            }
            if(!foundPower) qFatal("Conflict test did not reach live window");
            const auto owned=standardCable(false).toStdWString();
            for(int role=0;role<3;++role)
                if(soundcurrent::windowsDefaultEndpointId(false,role)==owned)
                    qFatal("Conflict retained managed route before warning");
            qCritical("Live conflict: %s",qPrintable(reason));
            qInfo("PASS: processing off and route released before conflict dialog");
            // Dismiss only the opt-in fixture dialog; retain the production
            // conflict detection, processor shutdown and event-loop flow.
            QTimer::singleShot(0,&box,&QDialog::accept);
        }
#endif
        box.exec();
    };
    if (!processingGuard.acquire(soundcurrent::processingGuardDirectory())) { showConflict(processingGuard.error()); return 1; }
    const auto conflict=soundcurrent::otherEqualizerConflict(kSink);
    if (!conflict.isEmpty()) { showConflict(conflict); return 1; }
    MainWindow window;
#ifdef Q_OS_WIN
#include "windows_live_conflict_test.inc"
#endif
    QTimer conflictMonitor;
    conflictMonitor.setInterval(2000);
    QObject::connect(&conflictMonitor,&QTimer::timeout,&window,[&]{
        const auto reason=soundcurrent::otherEqualizerConflict(kSink);
        if(!reason.isEmpty()){conflictMonitor.stop();window.stopForConflict();showConflict(reason);qApp->quit();}
    });
    conflictMonitor.start();
    QObject::connect(&instanceServer, &QLocalServer::newConnection, &window, [&] {
        while (instanceServer.hasPendingConnections()) {
            auto *client = instanceServer.nextPendingConnection();
            QObject::connect(client, &QLocalSocket::readyRead, &window, [client, &window] {
                const auto request = client->readAll();
                if (request.startsWith('Q')) qApp->quit();
                else if (request.startsWith('H')) window.close();
                else if (request.startsWith('S')) window.reopen();
                client->disconnectFromServer();
            });
            QObject::connect(client, &QLocalSocket::disconnected, client, &QLocalSocket::deleteLater);
        }
    });
    window.show();
    return app.exec();
}
