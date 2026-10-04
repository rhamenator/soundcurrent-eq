#include <QApplication>
#include <QCheckBox>
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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
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

QString filterConfig(const QString &target, const Bands &bands) {
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

    void start(const Device &device, const Bands &bands) {
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
        config.write(filterConfig(device.name, bands).toUtf8());
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

    void update(const Bands &bands) {
        if (!active()) return;
        const auto id = nodeId(kSink);
        if (id < 0) throw std::runtime_error("Equalizer sink disappeared");
        QStringList controls = {quote("preamp:Mult"), QString::number(std::pow(10.0, headroom(bands) / 20.0), 'f', 8)};
        for (int i = 0; i < kMaxBands; ++i) {
            const auto band = i < bands.size() ? bands[i] : Band{};
            controls << quote(QString("band_%1:Freq").arg(i + 1)) << QString::number(band.frequency, 'f', 1)
                     << quote(QString("band_%1:Q").arg(i + 1)) << QString::number(band.q, 'f', 2)
                     << quote(QString("band_%1:Gain").arg(i + 1)) << QString::number(band.gain, 'f', 2);
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

QMap<QString, std::array<double, 9>> builtinShapes() {
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

class MainWindow : public QMainWindow {
public:
    MainWindow() : bands_(builtinProfile("Balanced", kDefaultBands)) {
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

        auto *title = new QLabel("SoundCurrent EQ");
        title->setObjectName("title");
        root->addWidget(title);
        root->addWidget(new QLabel("Shape your sound with an adjustable parametric equalizer."));

        auto *outputBox = new QGroupBox("Playback");
        auto *outputLayout = new QVBoxLayout(outputBox);
        auto *outputRow = new QHBoxLayout;
        outputCombo_ = new QComboBox;
        outputCombo_->setAccessibleName("Output device");
        outputCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        outputRow->addWidget(outputCombo_, 1);
        auto *refresh = new QPushButton("Refresh devices");
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
        presetCombo_->setAccessibleName("Listening preset");
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
        eqLayout->addWidget(new QLabel("The graph shows the combined EQ response. Boosts lower the preamp to leave headroom."));
        root->addWidget(eqBox, 1);

        loadCustomPresets();
        rebuildPresetList("Balanced");
        rebuildBandControls();
        syncBandControls();
        refreshDevices();

        connect(refresh, &QPushButton::clicked, this, [this] { refreshDevices(); });
        connect(power_, &QCheckBox::toggled, this, [this](bool on) { togglePower(on); });
        connect(outputCombo_, &QComboBox::currentIndexChanged, this, [this] { outputChanged(); });
        connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this] { presetChanged(); });
        connect(save, &QPushButton::clicked, this, [this] { savePreset(); });
        connect(reset, &QPushButton::clicked, this, [this] { presetCombo_->setCurrentText("Flat"); });
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
            try { audio_.update(bands_); } catch (const std::exception &error) { showError(error.what()); }
        });
        monitor_.setInterval(1500);
        connect(&monitor_, &QTimer::timeout, this, [this] { refreshDevices(); });
        monitor_.start();
    }

    ~MainWindow() override { audio_.stop(); }

private:
    static QString frequencyLabel(double frequency) {
        return frequency >= 1000 ? QString::number(frequency / 1000.0, 'g', 3) + "k"
                                 : QString::number(frequency, 'g', 4);
    }

    void rebuildBandControls() {
        if (auto *old = bandScroll_->takeWidget()) old->deleteLater();
        sliders_.clear();
        gainLabels_.clear();
        frequencyButtons_.clear();
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
            column->addWidget(slider, 1, Qt::AlignHCenter);
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
        headroom_->setText("Preamp " + QString::number(headroom(bands_), 'f', 1) + " dB");
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
        for (const auto &name : {"Balanced", "Bass Boost", "Clear Voice", "Warm", "Bright", "Movies", "Flat"})
            presetCombo_->addItem(name);
        for (auto it = custom_.begin(); it != custom_.end(); ++it) presetCombo_->addItem(it.key());
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
                    audio_.stop();
                    power_->setChecked(false);
                    status_->setText("No output device is connected.");
                } else {
                    audio_.start(desired, bands_);
                    status_->setText("On · Playing through " + desired.description);
                }
            }
        } catch (const std::exception &error) { showError(error.what()); }
    }

    void outputChanged() {
        if (!power_->isChecked()) return;
        const auto desired = selectedDevice();
        if (desired.name.isEmpty() || desired.name == audio_.target()) return;
        try {
            audio_.start(desired, bands_);
            status_->setText("On · Playing through " + desired.description);
        } catch (const std::exception &error) { showError(error.what()); }
    }

    void togglePower(bool on) {
        if (on) {
            const auto device = selectedDevice();
            if (device.name.isEmpty()) { power_->setChecked(false); showError("No output device is available."); return; }
            try {
                audio_.start(device, bands_);
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
    Bands bands_;
    QList<Device> devices_;
    QStringList knownNames_;
    QMap<QString, Bands> custom_;
    QComboBox *outputCombo_ = nullptr;
    QComboBox *presetCombo_ = nullptr;
    QCheckBox *power_ = nullptr;
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
    QTimer applyTimer_;
    QTimer monitor_;
    int selected_ = 0;
    bool changing_ = false;
};

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("SoundCurrent");
    QCoreApplication::setApplicationName("soundcurrent-eq");
    QGuiApplication::setDesktopFileName("io.github.rhamenator.SoundCurrentEQ");
    app.setWindowIcon(QIcon::fromTheme("io.github.rhamenator.SoundCurrentEQ"));
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
    if (app.arguments().contains("--ui-self-test")) {
        MainWindow testWindow;
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
        count->setValue(31);
        if (testWindow.findChildren<QSlider *>().size() != 31) qFatal("31-band layout failed");
        frequency->setValue(22);
        gain->setValue(4);
        q->setValue(1.8);
        if (frequency->value() != 22 || gain->value() != 4 || q->value() != 1.8)
            qFatal("Selected-band editing failed");
        count->setValue(15);
        if (testWindow.findChildren<QSlider *>().size() != 15) qFatal("Band-count change failed");
        qInfo("UI self-test passed with editable 5–31 band layout");
        return 0;
    }
    MainWindow window;
    window.show();
    return app.exec();
}
