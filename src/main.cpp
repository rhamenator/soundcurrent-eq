// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 rhamenator

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
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
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSaveFile>
#include <QSettings>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTemporaryDir>
#include <QThread>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <functional>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <csignal>
#include <sys/prctl.h>

namespace {

constexpr auto kSink = "soundcurrent_eq";
constexpr auto kOutput = "soundcurrent_eq_output";
constexpr int kMinBands = 5;
constexpr int kDefaultBands = 15;
constexpr int kMaxBands = 31;
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
};
using Bands = QVector<Band>;

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

double responseDb(const Bands &bands, double frequency) {
    constexpr double sampleRate = 48000.0;
    const auto sampleAngle = 2.0 * std::numbers::pi * frequency / sampleRate;
    const std::complex<double> z = std::polar(1.0, -sampleAngle);
    double total = 0.0;
    for (const auto &band : bands) {
        if (band.gain == 0) continue;
        const auto angle = 2.0 * std::numbers::pi * band.frequency / sampleRate;
        const auto cosine = std::cos(angle);
        const auto alpha = std::sin(angle) / (2.0 * band.q);
        const auto amplitude = std::pow(10.0, band.gain / 40.0);
        const auto b0 = 1.0 + alpha * amplitude;
        const auto b1 = -2.0 * cosine;
        const auto b2 = 1.0 - alpha * amplitude;
        const auto a0 = 1.0 + alpha / amplitude;
        const auto a1 = -2.0 * cosine;
        const auto a2 = 1.0 - alpha / amplitude;
        const auto numerator = b0 + b1 * z + b2 * z * z;
        const auto denominator = a0 + a1 * z + a2 * z * z;
        total += 20.0 * std::log10(std::abs(numerator / denominator));
    }
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

QString quote(const QString &value) {
    const auto encoded = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(encoded.mid(1, encoded.size() - 2));
}

QString filterConfig(const QString &target, const Bands &bands, double outputGainDb = 0.0) {
    QStringList nodes;
    QStringList links;
    nodes << QString("{ type = builtin name = preamp label = linear control = { \"Mult\" = %1 \"Add\" = 0.0 } }")
                 .arg(QString::number(std::pow(10.0, headroom(bands) / 20.0), 'f', 8));
    links << "{ output = \"preamp:Out\" input = \"band_1:In\" }";
    for (int i = 0; i < kMaxBands; ++i) {
        const auto band = i < bands.size() ? bands[i] : Band{};
        nodes << QString("{ type = builtin name = band_%1 label = bq_peaking control = { \"Freq\" = %2 \"Q\" = %3 \"Gain\" = %4 } }")
                     .arg(i + 1).arg(band.frequency, 0, 'f', 1).arg(band.q, 0, 'f', 2).arg(band.gain, 0, 'f', 2);
        if (i > 0) links << QString("{ output = \"band_%1:Out\" input = \"band_%2:In\" }").arg(i).arg(i + 1);
    }
    nodes << QString("{ type = builtin name = output_gain label = linear control = { \"Mult\" = %1 \"Add\" = 0.0 } }")
                 .arg(QString::number(std::pow(10.0, outputGainDb / 20.0), 'f', 8));
    links << QString("{ output = \"band_%1:Out\" input = \"output_gain:In\" }").arg(kMaxBands);
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

QString filterControls(const Bands &bands, double outputGainDb = 0.0) {
    QStringList controls = {quote("preamp:Mult"), QString::number(std::pow(10.0, headroom(bands) / 20.0), 'f', 8)};
    for (int i = 0; i < kMaxBands; ++i) {
        const auto band = i < bands.size() ? bands[i] : Band{};
        controls << quote(QString("band_%1:Freq").arg(i + 1)) << QString::number(band.frequency, 'f', 1)
                 << quote(QString("band_%1:Q").arg(i + 1)) << QString::number(band.q, 'f', 2)
                 << quote(QString("band_%1:Gain").arg(i + 1)) << QString::number(band.gain, 'f', 2);
    }
    controls << quote("output_gain:Mult") << QString::number(std::pow(10.0, outputGainDb / 20.0), 'f', 8);
    return "{ params = [ " + controls.join(' ') + " ] }";
}

class AudioEngine {
public:
    bool active() const { return process_.state() != QProcess::NotRunning; }
    QString target() const { return target_.name; }

    void start(const Device &device, const Bands &bands, double outputGainDb = 0.0) {
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
        config.write(filterConfig(device.name, bands, outputGainDb).toUtf8());
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

    void update(const Bands &bands, double outputGainDb = 0.0) {
        if (!active()) return;
        const auto id = nodeId(kSink);
        if (id < 0) throw std::runtime_error("Equalizer sink disappeared");
        command("pw-cli", {"set-param", QString::number(id), "Props", filterControls(bands, outputGainDb)});
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

class CurveWidget : public QWidget {
public:
    std::function<void(int)> onSelect;
    std::function<void(int, double, double)> onMove;

    explicit CurveWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumHeight(150);
        setAccessibleName("Equalizer curve. Select a point or drag it to adjust frequency and gain.");
        setMouseTracking(true);
    }

    void setBands(const Bands &bands, int selected) {
        bands_ = bands;
        selected_ = selected;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
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
            const auto y = yForGain(std::clamp(responseDb(bands_, frequency), -12.0, 12.0), plot);
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
        if (dragging_ < 0 || !onMove) return;
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
    int selected_ = 0;
    int dragging_ = -1;
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

class SpectrumMonitor : public QObject {
public:
    std::function<void(const QVector<double> &, double)> onLevels;
    bool active() const { return process_.state() != QProcess::NotRunning; }

    SpectrumMonitor() {
        timer_.setInterval(50);
        connect(&process_, &QProcess::readyReadStandardOutput, this, [this] {
            appendPcm(process_.readAllStandardOutput());
        });
        connect(&timer_, &QTimer::timeout, this, [this] { analyze(); });
    }

    void setInterval(int milliseconds) { timer_.setInterval(std::clamp(milliseconds, 25, 250)); }
    int interval() const { return timer_.interval(); }

    void setProfile(const Bands &bands, double outputGainDb) {
        bands_ = bands;
        outputGainDb_ = outputGainDb;
        headroomDb_ = headroom(bands_);
    }

    void analyzePcmForTest(const QByteArray &pcm) {
        appendPcm(pcm);
        analyze();
    }

    void start() {
        stop();
        process_.setProgram("parec");
        process_.setArguments({"--raw", "-d", QString(kSink) + ".monitor", "--format=s16le",
                               "--rate=48000", "--channels=2"});
        process_.start();
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
        double peak = 0.0;
        for (int i = 0; i < frameCount; ++i) {
            for (int channel = 0; channel < 2; ++channel) {
                const auto offset = frameBytes * i + 2 * channel;
                const auto raw = uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8);
                const auto sample = int16_t(raw) / 32768.0;
                peak = std::max(peak, std::abs(sample));
            }
        }
        const int first = (frameCount - n) * frameBytes;
        std::array<std::complex<double>, n> spectrum;
        for (int i = 0; i < n; ++i) {
            const int offset = first + i * frameBytes;
            const auto left = int16_t(uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8));
            const auto right = int16_t(uint16_t(data[offset + 2]) | (uint16_t(data[offset + 3]) << 8));
            const double window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * i / (n - 1));
            spectrum[i] = (double(left) + double(right)) / 65536.0 * window;
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
                while (band + 1 < bands_.size() &&
                       frequency > std::sqrt(bands_[band].frequency * bands_[band + 1].frequency))
                    ++band;
                levels[band] = std::max(levels[band], std::abs(spectrum[bin]) * 4.0 / n);
            }
            for (qsizetype i = 0; i < bands_.size(); ++i)
                levels[i] *= std::pow(10.0, (responseDb(bands_, bands_[i].frequency) +
                                              headroomDb_ + outputGainDb_) / 20.0);
        }
        const auto maxEqBoost = headroomDb_ < 0.0 ? -headroomDb_ - 1.0 : 0.0;
        const auto estimatedPeak = peak * std::pow(10.0, (maxEqBoost + headroomDb_ + outputGainDb_) / 20.0);
        if (onLevels) onLevels(levels, estimatedPeak);
    }

    QProcess process_;
    QTimer timer_;
    QByteArray pcm_;
    qsizetype bytesSinceAnalysis_ = 0;
    Bands bands_;
    double headroomDb_ = 0.0;
    double outputGainDb_ = 0.0;
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
        setWindowIcon(QIcon::fromTheme("io.github.rhamenator.SoundCurrentEQ"));
        setMinimumSize(760, 620);
        resize(1050, 920);

        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        setCentralWidget(scroll);
        auto *container = new QWidget;
        auto *root = new QVBoxLayout(container);
        root->setContentsMargins(26, 22, 26, 24);
        root->setSpacing(16);
        scroll->setWidget(container);

        auto *outputBox = new QGroupBox("Playback");
        auto *outputLayout = new QVBoxLayout(outputBox);
        auto *outputRow = new QHBoxLayout;
        outputCombo_ = new QComboBox;
        outputCombo_->setAccessibleName("Output device");
        outputCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        outputRow->addWidget(outputCombo_, 1);
        auto *refresh = new QPushButton("Refresh devices");
        outputRow->addWidget(refresh);
        power_ = new QCheckBox("Equalizer off");
        power_->setObjectName("powerToggle");
        power_->setAccessibleName("Equalizer on or off");
        power_->setToolTip("Click to turn the equalizer on or off");
        outputRow->addWidget(power_);
        auto *quit = new QPushButton("Quit app");
        quit->setAccessibleName("Quit SoundCurrent EQ");
        quit->setToolTip("Exit SoundCurrent EQ and restore normal audio");
        outputRow->addWidget(quit);
        outputLayout->addLayout(outputRow);
        auto *gainRow = new QHBoxLayout;
        gainRow->addWidget(new QLabel("Output gain"));
        outputGain_ = new QDoubleSpinBox;
        outputGain_->setRange(-12.0, 12.0);
        outputGain_->setDecimals(1);
        outputGain_->setSingleStep(0.5);
        outputGain_->setSuffix(" dB");
        outputGain_->setAccessibleName("Output gain after equalization");
        outputGain_->setToolTip("Raise the level after the EQ. Higher gain can cause clipping.");
        const double savedGain = QSettings().value("outputGainDb", 0.0).toDouble();
        outputGain_->setValue(std::isfinite(savedGain) ? std::clamp(savedGain, -12.0, 12.0) : 0.0);
        gainRow->addWidget(outputGain_);
        gainRow->addSpacing(18);
        peakStatus_ = new QLabel("Estimated peak: waiting for audio");
        peakStatus_->setAccessibleName("Estimated output peak and clipping risk");
        peakStatus_->setObjectName("peakStatus");
        gainRow->addWidget(peakStatus_);
        gainRow->addStretch();
        gainRow->addWidget(new QLabel("Level refresh"));
        levelRefresh_ = new QSpinBox;
        levelRefresh_->setRange(25, 250);
        levelRefresh_->setSingleStep(25);
        levelRefresh_->setSuffix(" ms");
        levelRefresh_->setAccessibleName("Level indicator refresh interval");
        levelRefresh_->setToolTip("Shorter intervals update levels more often and use more CPU");
        levelRefresh_->setValue(std::clamp(QSettings().value("levelRefreshMs", 50).toInt(), 25, 250));
        gainRow->addWidget(levelRefresh_);
        peakMarkers_ = new QCheckBox("Peak markers");
        peakMarkers_->setAccessibleName("Show peak markers on frequency levels");
        peakMarkers_->setToolTip("Show a falling peak hold line on each frequency level");
        peakMarkers_->setChecked(QSettings().value("showPeakMarkers", false).toBool());
        gainRow->addWidget(peakMarkers_);
        outputLayout->addLayout(gainRow);
        status_ = new QLabel("Equalizer is off. Your audio uses its normal output.");
        status_->setWordWrap(true);
        status_->setObjectName("status");
        outputLayout->addWidget(status_);
        root->addWidget(outputBox);

        auto *presetBox = new QGroupBox("Listening preset");
        auto *presetRow = new QHBoxLayout(presetBox);
        presetCombo_ = new PresetComboBox;
        presetCombo_->setAccessibleName("Listening preset");
        presetCombo_->setView(new QListView(presetCombo_));
        presetCombo_->setMaxVisibleItems(12);
        presetCombo_->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        presetRow->addWidget(presetCombo_, 1);
        auto *save = new QPushButton("Save preset");
        presetRow->addWidget(save);
        auto *reset = new QPushButton("Reset to flat");
        presetRow->addWidget(reset);
        root->addWidget(presetBox);

        auto *eqBox = new QGroupBox("Equalizer");
        auto *eqLayout = new QVBoxLayout(eqBox);
        auto *toolbar = new QHBoxLayout;
        toolbar->addWidget(new QLabel("Bands"));
        countBox_ = new QSpinBox;
        countBox_->setRange(kMinBands, kMaxBands);
        countBox_->setValue(kDefaultBands);
        countBox_->setAccessibleName("Number of equalizer bands");
        toolbar->addWidget(countBox_);
        toolbar->addSpacing(14);
        toolbar->addWidget(new QLabel("Drag curve points or tune the selected band below."));
        toolbar->addStretch();
        headroom_ = new QLabel;
        toolbar->addWidget(headroom_);
        eqLayout->addLayout(toolbar);

        auto *details = new QWidget;
        auto *detailsRow = new QHBoxLayout(details);
        detailsRow->setContentsMargins(0, 2, 0, 2);
        detailsRow->addWidget(new QLabel("Selected band"));
        detailsRow->addSpacing(8);
        detailsRow->addWidget(new QLabel("Frequency"));
        frequencyBox_ = new QDoubleSpinBox;
        frequencyBox_->setRange(20, 20000);
        frequencyBox_->setDecimals(0);
        frequencyBox_->setSingleStep(1);
        frequencyBox_->setSuffix(" Hz");
        frequencyBox_->setAccessibleName("Selected band frequency");
        detailsRow->addWidget(frequencyBox_);
        detailsRow->addSpacing(12);
        detailsRow->addWidget(new QLabel("Gain"));
        gainBox_ = new QDoubleSpinBox;
        gainBox_->setRange(-12, 12);
        gainBox_->setDecimals(1);
        gainBox_->setSingleStep(0.5);
        gainBox_->setSuffix(" dB");
        gainBox_->setAccessibleName("Selected band gain");
        detailsRow->addWidget(gainBox_);
        detailsRow->addSpacing(12);
        detailsRow->addWidget(new QLabel("Width (Q)"));
        qBox_ = new QDoubleSpinBox;
        qBox_->setRange(0.3, 10.0);
        qBox_->setDecimals(2);
        qBox_->setSingleStep(0.1);
        qBox_->setAccessibleName("Selected band filter Q");
        detailsRow->addWidget(qBox_);
        detailsRow->addStretch();
        eqLayout->addWidget(details);

        curve_ = new CurveWidget;
        eqLayout->addWidget(curve_);

        bandScroll_ = new QScrollArea;
        bandScroll_->setWidgetResizable(false);
        bandScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        bandScroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        bandScroll_->setFixedHeight(235);
        eqLayout->addWidget(bandScroll_);
        eqLayout->addWidget(new QLabel("Bars beside the sliders show estimated post-EQ levels. Red peak text warns of possible clipping."));
        root->addWidget(eqBox, 1);

        loadCustomPresets();
        rebuildPresetList("Flat");
        rebuildBandControls();
        syncBandControls();
        refreshDevices();

        connect(refresh, &QPushButton::clicked, this, [this] { refreshDevices(); });
        connect(quit, &QPushButton::clicked, qApp, [] { qApp->quit(); });
        connect(power_, &QCheckBox::toggled, this, [this](bool on) { togglePower(on); });
        connect(outputCombo_, &QComboBox::currentIndexChanged, this, [this] { outputChanged(); });
        connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this] { presetChanged(); });
        connect(save, &QPushButton::clicked, this, [this] { savePreset(); });
        connect(reset, &QPushButton::clicked, this, [this] { presetCombo_->setCurrentText("Flat"); });
        connect(outputGain_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
            QSettings().setValue("outputGainDb", value);
            meter_.setProfile(bands_, value);
            scheduleApply();
        });
        connect(levelRefresh_, &QSpinBox::valueChanged, this, [this](int milliseconds) {
            QSettings().setValue("levelRefreshMs", milliseconds);
            meter_.setInterval(milliseconds);
        });
        connect(peakMarkers_, &QCheckBox::toggled, this, [this](bool enabled) {
            QSettings().setValue("showPeakMarkers", enabled);
            for (auto *level : levelBars_) level->setPeakMarkersEnabled(enabled);
        });
        connect(countBox_, &QSpinBox::valueChanged, this, [this](int count) { changeBandCount(count); });
        connect(frequencyBox_, &QDoubleSpinBox::valueChanged, this, [this] { detailChanged(); });
        connect(gainBox_, &QDoubleSpinBox::valueChanged, this, [this] { detailChanged(); });
        connect(qBox_, &QDoubleSpinBox::valueChanged, this, [this] { detailChanged(); });
        curve_->onSelect = [this](int index) { selectBand(index); };
        curve_->onMove = [this](int index, double frequency, double gain) {
            if (index < 0 || index >= bands_.size()) return;
            bands_[index].frequency = frequency;
            bands_[index].gain = gain;
            selectBand(index);
            markCustom();
            syncBandControls();
            scheduleApply();
        };
        applyTimer_.setSingleShot(true);
        applyTimer_.setInterval(80);
        connect(&applyTimer_, &QTimer::timeout, this, [this] {
            meter_.setProfile(bands_, outputGain_->value());
            try { audio_.update(bands_, outputGain_->value()); } catch (const std::exception &error) { showError(error.what()); }
        });
        meter_.onLevels = [this](const QVector<double> &levels, double peak) { showLevels(levels, peak); };
        meter_.setProfile(bands_, outputGain_->value());
        meter_.setInterval(levelRefresh_->value());
        monitor_.setInterval(1500);
        connect(&monitor_, &QTimer::timeout, this, [this] { refreshDevices(); });
        monitor_.start();
        if (startEnabled) setupTray();
        if (startEnabled && power_->isEnabled()) power_->setChecked(true);
    }

    ~MainWindow() override { meter_.stop(); audio_.stop(); }

    void reopen() {
        showNormal();
        raise();
        activateWindow();
    }

protected:
    void showEvent(QShowEvent *event) override {
        QMainWindow::showEvent(event);
        QTimer::singleShot(0, this, [this] {
            if (isVisible() && power_->isChecked() && !meter_.active()) meter_.start();
        });
    }

    void closeEvent(QCloseEvent *event) override {
        if (tray_ && QSystemTrayIcon::isSystemTrayAvailable()) {
            meter_.stop();
            hide();
            event->ignore();
            if (!backgroundNoticeShown_) {
                tray_->showMessage("SoundCurrent EQ", "Equalizer is still running. Use the tray icon to reopen or quit.");
                backgroundNoticeShown_ = true;
            }
            return;
        }
        QMainWindow::closeEvent(event);
    }

private:
    void showPlaybackStatus(const Device &device) {
        status_->setText("On · Playing through " + device.description);
    }

    void setupTray() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
        tray_ = new QSystemTrayIcon(QIcon::fromTheme("io.github.rhamenator.SoundCurrentEQ"), this);
        tray_->setToolTip("SoundCurrent EQ");
        auto *menu = new QMenu(this);
        menu->addAction("Open SoundCurrent EQ", this, [this] { reopen(); });
        trayToggle_ = menu->addAction("Turn equalizer off", this, [this] { power_->setChecked(!power_->isChecked()); });
        connect(power_, &QCheckBox::toggled, this, [this](bool on) {
            trayToggle_->setText(on ? "Turn equalizer off" : "Turn equalizer on");
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
        return frequency >= 1000 ? QString::number(frequency / 1000.0, 'g', 3) + "k"
                                 : QString::number(frequency, 'g', 4);
    }

    void rebuildBandControls() {
        if (auto *old = bandScroll_->takeWidget()) old->deleteLater();
        sliders_.clear();
        gainLabels_.clear();
        frequencyButtons_.clear();
        levelBars_.clear();
        auto *container = new QWidget;
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
            slider->setAccessibleName(QString("Band %1 gain").arg(i + 1));
            sliders_.append(slider);
            auto *sliderRow = new QHBoxLayout;
            sliderRow->setSpacing(3);
            sliderRow->addWidget(slider, 1, Qt::AlignHCenter);
            auto *level = new BandLevelMeter;
            level->setPeakMarkersEnabled(peakMarkers_->isChecked());
            level->setAccessibleName(QString("Estimated output level near band %1").arg(i + 1));
            level->setToolTip("Estimated post-EQ level near this frequency");
            levelBars_.append(level);
            sliderRow->addWidget(level);
            column->addLayout(sliderRow, 1);
            auto *frequency = new QPushButton;
            frequency->setToolTip("Select this band to edit frequency, gain, and Q");
            frequency->setAccessibleName(QString("Select band %1").arg(i + 1));
            frequencyButtons_.append(frequency);
            column->addWidget(frequency);
            row->addLayout(column);
            connect(slider, &QSlider::valueChanged, this, [this, i](int value) {
                if (changing_) return;
                bands_[i].gain = value / 2.0;
                selectBand(int(i));
                markCustom();
                scheduleApply();
            });
            connect(frequency, &QPushButton::clicked, this, [this, i] { selectBand(int(i)); });
        }
        container->setFixedWidth(std::max(760, int(bands_.size()) * 63));
        container->setMinimumHeight(210);
        bandScroll_->setWidget(container);
    }

    void showLevels(const QVector<double> &levels, double peak) {
        const auto count = std::min(levels.size(), levelBars_.size());
        for (qsizetype i = 0; i < count; ++i) {
            const double db = 20.0 * std::log10(std::max(levels[i], 0.000001));
            if (power_->isChecked()) levelBars_[i]->setLevel(db);
            else levelBars_[i]->reset();
            levelBars_[i]->setToolTip(QString("Estimated output near %1: %2 dBFS")
                                     .arg(frequencyLabel(bands_[i].frequency)).arg(db, 0, 'f', 1));
        }
        if (!power_->isChecked()) {
            peakStatus_->setText("Estimated peak: EQ off");
            peakStatus_->setStyleSheet("color:#8fa2bb;");
        } else if (peak <= 0.000001) {
            peakStatus_->setText("Estimated peak: waiting for audio");
            peakStatus_->setStyleSheet("color:#8fa2bb;");
        } else {
            const double db = 20.0 * std::log10(peak);
            peakStatus_->setText(db >= -1.0
                                     ? QString("Clipping risk · estimated peak %1 dBFS").arg(db, 0, 'f', 1)
                                     : QString("Estimated peak %1 dBFS").arg(db, 0, 'f', 1));
            peakStatus_->setStyleSheet(db >= -1.0 ? "color:#f16b76;font-weight:700;"
                                                    : db >= -6.0 ? "color:#e6b450;" : "color:#50d1ba;");
        }
    }

    void syncBandControls() {
        changing_ = true;
        for (qsizetype i = 0; i < bands_.size(); ++i) {
            const auto &band = bands_[i];
            sliders_[i]->setValue(std::lround(band.gain * 2));
            gainLabels_[i]->setText((band.gain > 0 ? "+" : "") + QString::number(band.gain, 'g', 3));
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
        headroom_->setText("Auto headroom " + QString::number(headroom(bands_), 'f', 1) + " dB");
        curve_->setBands(bands_, selected_);
    }

    void selectBand(int index) {
        if (index < 0 || index >= bands_.size()) return;
        selected_ = index;
        syncBandControls();
    }

    void markCustom() {
        changing_ = true;
        presetCombo_->setCurrentText("Custom");
        changing_ = false;
    }

    void scheduleApply() { applyTimer_.start(); }

    void detailChanged() {
        if (changing_ || bands_.isEmpty()) return;
        auto &band = bands_[selected_];
        band.frequency = frequencyBox_->value();
        band.gain = gainBox_->value();
        band.q = qBox_->value();
        markCustom();
        syncBandControls();
        scheduleApply();
    }

    void changeBandCount(int count) {
        if (changing_ || count == bands_.size()) return;
        bands_ = remapBands(bands_, count);
        selected_ = std::min(selected_, count - 1);
        rebuildBandControls();
        markCustom();
        syncBandControls();
        scheduleApply();
    }

    void presetChanged() {
        if (changing_) return;
        const auto name = presetCombo_->currentText();
        if (builtinShapes().contains(name)) bands_ = builtinProfile(name, countBox_->value());
        else if (custom_.contains(name)) bands_ = custom_[name];
        else return;
        selected_ = std::min(selected_, int(bands_.size()) - 1);
        const QSignalBlocker blocker(countBox_);
        countBox_->setValue(int(bands_.size()));
        rebuildBandControls();
        syncBandControls();
        scheduleApply();
    }

    void loadCustomPresets() {
        QFile file(presetsPath());
        if (!file.open(QIODevice::ReadOnly)) return;
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key().trimmed().isEmpty() || it.key() == "Custom" || builtinShapes().contains(it.key())) continue;
            const auto parsed = parseBands(it.value());
            if (parsed) custom_.insert(it.key(), *parsed);
        }
    }

    void rebuildPresetList(const QString &selected) {
        const QSignalBlocker blocker(presetCombo_);
        presetCombo_->clear();
        auto addGroup = [this](const QStringList &names) {
            if (presetCombo_->count()) presetCombo_->insertSeparator(presetCombo_->count());
            for (const auto &name : names) presetCombo_->addItem(name);
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
            for (auto it = custom_.begin(); it != custom_.end(); ++it) presetCombo_->addItem(it.key());
        }
        presetCombo_->addItem("Custom");
        presetCombo_->setCurrentText(selected);
    }

    void savePreset() {
        bool ok = false;
        const auto name = QInputDialog::getText(this, "Save EQ preset", "Preset name:", QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok) return;
        if (name.isEmpty() || name == "Custom" || builtinShapes().contains(name)) {
            showError("Choose a name that is not a built-in preset.");
            return;
        }
        custom_[name] = bands_;
        QJsonObject object;
        for (auto it = custom_.begin(); it != custom_.end(); ++it)
            object.insert(it.key(), QJsonObject{{"bands", serializeBands(it.value())}});
        const auto path = presetsPath();
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) { showError("Could not create preset folder."); return; }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) { showError("Could not save preset."); return; }
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        file.write(QJsonDocument(object).toJson());
        if (!file.commit()) { showError("Could not finish saving preset."); return; }
        rebuildPresetList(name);
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
            if (!selectedName.isEmpty() && index >= 0) desired = findDevice(selectedName);
            else if (!selectedName.isEmpty() && index < 0) {
                desired = bestDevice(devices_);
                status_->setText("Selected output was unplugged. Switched to automatic output.");
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
                    status_->setText("No output device is connected.");
                } else {
                    meter_.stop();
                    audio_.start(desired, bands_, outputGain_->value());
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
            audio_.start(desired, bands_, outputGain_->value());
            if (isVisible()) meter_.start();
            showPlaybackStatus(desired);
        } catch (const std::exception &error) { showError(error.what()); }
    }

    void togglePower(bool on) {
        power_->setText(on ? "Equalizer on" : "Equalizer off");
        if (on) {
            const auto device = selectedDevice();
            if (device.name.isEmpty()) { power_->setChecked(false); showError("No output device is available."); return; }
            try {
                audio_.start(device, bands_, outputGain_->value());
                if (isVisible()) meter_.start();
                showPlaybackStatus(device);
            } catch (const std::exception &error) {
                power_->setChecked(false);
                showError(error.what());
            }
        } else {
            meter_.stop();
            audio_.stop();
            status_->setText("Equalizer is off. Your audio uses its normal output.");
        }
    }

    void showError(const QString &message) { status_->setText("Audio error: " + message); }

    AudioEngine audio_;
    SpectrumMonitor meter_;
    Bands bands_;
    QList<Device> devices_;
    QStringList knownNames_;
    QMap<QString, Bands> custom_;
    QComboBox *outputCombo_ = nullptr;
    QComboBox *presetCombo_ = nullptr;
    QCheckBox *power_ = nullptr;
    QDoubleSpinBox *outputGain_ = nullptr;
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
    QTimer applyTimer_;
    QTimer monitor_;
    QSystemTrayIcon *tray_ = nullptr;
    QAction *trayToggle_ = nullptr;
    int selected_ = 0;
    bool changing_ = false;
    bool backgroundNoticeShown_ = false;
};

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("SoundCurrent");
    QCoreApplication::setApplicationName("soundcurrent-eq");
    QGuiApplication::setDesktopFileName("io.github.rhamenator.SoundCurrentEQ");
    app.setWindowIcon(QIcon::fromTheme("io.github.rhamenator.SoundCurrentEQ"));
    if (app.arguments().size() == 3 && app.arguments()[1] == "--dump-filter-config") {
        QTextStream(stdout) << filterConfig(app.arguments()[2], defaultBands(kDefaultBands));
        return 0;
    }
    if ((app.arguments().size() == 3 || app.arguments().size() == 4) &&
        app.arguments()[1] == "--dump-preset-controls") {
        const auto name = app.arguments()[2];
        if (!builtinShapes().contains(name)) return 2;
        bool valid = true;
        const double gain = app.arguments().size() == 4 ? app.arguments()[3].toDouble(&valid) : 0.0;
        if (!valid || !std::isfinite(gain) || gain < -12.0 || gain > 12.0) return 2;
        QTextStream(stdout) << filterControls(builtinProfile(name, kDefaultBands), gain);
        return 0;
    }
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
            AudioEngine test;
            test.start(selected, builtinProfile("Bass Boost", kDefaultBands));
            auto adjusted = builtinProfile("Clear Voice", kMaxBands);
            adjusted[12].frequency = 320.0;
            adjusted[12].q = 2.0;
            test.update(adjusted);
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
    )");
    if (app.arguments().contains("--ui-self-test")) {
        MainWindow testWindow(false);
        SpectrumMonitor spectrumTest;
        if (spectrumTest.interval() != 50) qFatal("Default level interval is not 50 ms");
        spectrumTest.setInterval(25);
        if (spectrumTest.interval() != 25) qFatal("Level interval is not adjustable");
        spectrumTest.setProfile(defaultBands(kDefaultBands), 0.0);
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
        auto *outputGain = findDouble("Output gain after equalization");
        if (!outputGain || outputGain->minimum() != -12.0 || outputGain->maximum() != 12.0)
            qFatal("Output gain control is missing");
        auto *levelRefresh = findSpin("Level indicator refresh interval");
        if (!levelRefresh || levelRefresh->minimum() != 25 || levelRefresh->maximum() != 250 ||
            levelRefresh->singleStep() != 25)
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
        count->setValue(31);
        if (testWindow.findChildren<QSlider *>().size() != 31) qFatal("31-band layout failed");
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
        if (testWindow.findChildren<QSlider *>().size() != 15) qFatal("Band-count change failed");
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
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty() || !QFileInfo(runtime).isDir()) {
        qCritical("A private user runtime directory is required");
        return 1;
    }
    const auto socketPath = QDir(runtime).filePath("soundcurrent-eq.sock");
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
    MainWindow window;
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
