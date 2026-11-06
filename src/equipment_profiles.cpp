#include "button_help.h"
#include "localized_file_dialog.h"
#include "localization.h"
#include "equipment_display_text.h"
// SPDX-License-Identifier: GPL-3.0-only
#include "accelerating_spinbox.h"
#include "equipment_profiles.h"
#include <QApplication>
#include <QDateTime>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QGroupBox>
#include <QSlider>
#include <QKeyEvent>
#include <QShortcut>
#include <QKeySequence>
#include <QLineEdit>
#include <QCheckBox>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QScreen>
#include <QWheelEvent>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTableWidget>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace soundcurrent::equipment {
namespace {
void require(bool ok, const char *reason) {
    if (!ok)
        throw std::runtime_error(soundcurrent::i18n::text(reason).toStdString());
}
int ask(QWidget *parent, const QString &title, const QString &text, QMessageBox::StandardButtons buttons,
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton) {
    QMessageBox box(QMessageBox::Question, title, text, buttons, parent);
    box.setTextFormat(Qt::PlainText);
    box.setDefaultButton(defaultButton);
    return box.exec();
}
QString typeName(FilterType t) {
    return t == FilterType::LowShelf ? "LS" : t == FilterType::HighShelf ? "HS" : "PK";
}
double responseAt(const QVector<Point> &points, double f) {
    if (f <= points.front().frequency)
        return points.front().db;
    for (int i = 1; i < points.size(); ++i)
        if (f <= points[i].frequency) {
            const auto a = points[i - 1], b = points[i];
            return a.db + (b.db - a.db) * std::log(f / a.frequency) / std::log(b.frequency / a.frequency);
        }
    return points.back().db;
}
class ProfileCombo : public QComboBox {
  protected:
    // Scroll the dialog or open popup; do not audition equipment by an accidental wheel turn.
    void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};
class ProfileSlider : public QSlider {
  public:
    ProfileSlider() : QSlider(Qt::Horizontal) {
        setLayoutDirection(Qt::LeftToRight);
    }

  protected:
    void wheelEvent(QWheelEvent *event) override {
        event->ignore();
    }
};
class Plot : public QWidget {
  public:
    Profile profile;
    std::function<void(int, double, double)> onDrag;
    int dragging = -1, selected = 0;
    std::function<void(int)> onSelect;
    std::function<void(double, double)> onAdd;
    std::function<void()> onBegin, onEnd, onRemove;
    QRectF graphArea() const {
        return QRectF(45, 15, std::max(1, width() - 90), std::max(1, height() - 40));
    }
    int hit(const QPointF &position) const {
        const auto area = graphArea();
        int result = -1;
        double best = 20;
        for (int i = 0; i < profile.filters.size(); ++i) {
            const auto &b = profile.filters[i];
            const QPointF point(area.left() + std::log(b.frequency / 20.) / std::log(1000.) * area.width(),
                                area.center().y() - b.gainDb / (2 * rangeDb()) * area.height());
            const auto delta = point - position;
            const double d = std::hypot(delta.x(), delta.y());
            if (d < best || (i == selected && d <= best)) {
                best = d;
                result = i;
            }
        }
        return result;
    }
    std::pair<double, double> values(const QPointF &position) const {
        const auto area = graphArea();
        return {20. * std::pow(1000., std::clamp((position.x() - area.left()) / area.width(), 0., 1.)),
                std::clamp((area.center().y() - position.y()) / area.height() * (2 * rangeDb()), -6., 6.)};
    }
    double rangeDb() const {
        if (onDrag) { // Measured data is immutable; filter edits never move the scale under the pointer.
            double peak = 12.;
            for (const auto &point : profile.response)
                peak = std::max(peak, std::abs(point.db));
            return std::min(24., std::ceil(peak / 6.) * 6.);
        }
        double peak = 6;
        for (const auto &pt : profile.response)
            peak = std::max(peak, std::abs(pt.db));
        for (int i = 0; i < 40; ++i) {
            double db = 0;
            const double f = 20 * std::pow(1000., i / 39.);
            for (const auto &b : profile.filters)
                db += filterResponseDb(b, 48000, f);
            peak = std::max(peak, std::abs(db));
        }
        return std::min(24., std::ceil(peak / 6.) * 6.);
    }
    Plot() {
        setMinimumHeight(180);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(SC_TR("Published response and editable correction curves"));
    }
    void mousePressEvent(QMouseEvent *e) override {
        if (!onDrag || e->button() != Qt::LeftButton)
            return;
        setFocus();
        dragging = hit(e->position());
        if (dragging >= 0) {
            selected = dragging;
            if (onSelect)
                onSelect(selected);
            if (onBegin)
                onBegin();
            update();
        }
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (!onDrag || dragging < 0 || !(e->buttons() & Qt::LeftButton))
            return;
        const auto [f, g] = values(e->position());
        onDrag(dragging, f, g);
    }
    void mouseReleaseEvent(QMouseEvent *) override {
        if (dragging >= 0 && onEnd)
            onEnd();
        dragging = -1;
    }
    void mouseDoubleClickEvent(QMouseEvent *e) override {
        if (!onAdd || e->button() != Qt::LeftButton || hit(e->position()) >= 0 ||
            !graphArea().contains(e->position()))
            return;
        const auto [f, g] = values(e->position());
        onAdd(f, g);
    }
    void keyPressEvent(QKeyEvent *event) override {
        if (!onDrag || profile.filters.isEmpty()) {
            QWidget::keyPressEvent(event);
            return;
        }
        selected = std::clamp(selected, 0, int(profile.filters.size()) - 1);
        auto band = profile.filters[selected];
        bool edited = true;
        switch (event->key()) {
        case Qt::Key_Left:
            band.frequency = std::max(20., band.frequency / std::pow(2., 1. / 12));
            break;
        case Qt::Key_Right:
            band.frequency = std::min(20000., band.frequency * std::pow(2., 1. / 12));
            break;
        case Qt::Key_Up:
            band.gainDb = std::min(6., band.gainDb + .1);
            break;
        case Qt::Key_Down:
            band.gainDb = std::max(-6., band.gainDb - .1);
            break;
        case Qt::Key_Delete:
            if (onRemove)
                onRemove();
            return;
        case Qt::Key_Insert:
            if (onAdd)
                onAdd(1000, 0);
            return;
        default:
            edited = false;
            break;
        }
        if (edited) {
            if (onBegin)
                onBegin();
            onDrag(selected, band.frequency, band.gainDb);
            if (onEnd)
                onEnd();
            event->accept();
        } else
            QWidget::keyPressEvent(event);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setLayoutDirection(Qt::LeftToRight);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#172337"));
        const auto area = graphArea();
        auto x = [&](double f) { return area.left() + std::log(f / 20.) / std::log(1000.) * area.width(); };
        auto y = [&](double d) {
            return area.center().y() - std::clamp(d, -rangeDb(), rangeDb()) / (2 * rangeDb()) * area.height();
        };
        p.setPen(QColor("#718096"));
        for (double f : {20., 100., 1000., 10000., 20000.}) {
            p.drawLine(QPointF(x(f), area.top()), QPointF(x(f), area.bottom()));
            p.drawText(QPointF(x(f) - 10, area.bottom() + 17), QLocale().toString(f, 'g', 6));
        }
        for (double d : {-rangeDb(), -rangeDb() / 2, 0., rangeDb() / 2, rangeDb()}) {
            p.drawLine(QPointF(area.left(), y(d)), QPointF(area.right(), y(d)));
            p.drawText(QPointF(2, y(d) + 4), QLocale().toString(d, 'g', 6));
        }
        auto draw = [&](QColor color, auto db, double low = 20., double high = 20000.) {
            QPainterPath path;
            for (int i = 0; i <= 400; ++i) {
                const double f = low * std::pow(high / low, i / 400.);
                const QPointF v(x(f), y(db(f)));
                if (i)
                    path.lineTo(v);
                else
                    path.moveTo(v);
            }
            p.setPen(QPen(color, 2));
            p.drawPath(path);
        };
        if (!profile.response.isEmpty() && profile.response.back().frequency > 20. &&
            profile.response.front().frequency < 20000.)
            draw(
                QColor("#f6ad55"), [&](double f) { return responseAt(profile.response, f); },
                std::max(20., profile.response.front().frequency),
                std::min(20000., profile.response.back().frequency));
        draw(QColor("#4fd1c5"), [&](double f) {
            double d = 0;
            for (const auto &b : profile.filters)
                d += filterResponseDb(b, 48000, f);
            return d;
        });
        if (onDrag) {
            p.setPen(QColor("#4fd1c5"));
            p.setBrush(QColor("#4fd1c5"));
            for (int i = 0; i < profile.filters.size(); ++i) {
                const auto &b = profile.filters[i];
                const auto point = QPointF(x(b.frequency), y(b.gainDb));
                p.setBrush(i == selected ? QColor("#ffffff") : QColor("#4fd1c5"));
                p.drawEllipse(point, i == selected ? 6 : 4, i == selected ? 6 : 4);
                p.drawText(point + QPointF(7, -7), QLocale().toString(i + 1));
            }
        }
        if (onDrag && hasFocus()) {
            p.setPen(QPen(QColor("#ffffff"), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(rect().adjusted(2, 2, -3, -3));
        }
    }
};
QString curveLegend(const Profile &p) {
    if(p.estimated) return "<span style='color:#4fd1c5'>●</span> " + SC_TR("Correction filters:").toHtmlEscaped() +
        " &nbsp; <span style='color:#f6ad55'>●</span> " + SC_TR("Estimated profile").toHtmlEscaped() + " · dB";
    return SC_TR("Teal: correction EQ. Orange: measured response, when supplied. Vertical scale is relative dB.");
}
class Editor : public QDialog {
  public:
    Profile draft;
    bool dirty = false, restoring = false, gesture = false, gestureRecorded = false;
    QString originalId, originalProvenance;
    QTableWidget *table = nullptr;
    Plot *plot = nullptr;
    QLineEdit *brand, *family, *model, *source, *conditions, *equipmentType, *powerType;
    QComboBox *selection = nullptr;
    QSlider *frequency = nullptr, *gain = nullptr, *bandwidth = nullptr;
    QLabel *value = nullptr;
    QPushButton *undo = nullptr, *add = nullptr, *remove = nullptr, *previewButton = nullptr;
    QVector<QVector<EqBand>> history;
    std::function<bool(Profile)> save;
    Preview preview;
    Editor(Profile profile, QWidget *parent, std::function<bool(Profile)> writer,
           const Preview &audition = {})
        : QDialog(parent), draft(std::move(profile)), save(std::move(writer)), preview(audition) {
        originalId = draft.id;
        originalProvenance = draft.provenance;
        setWindowTitle(SC_TR("Equipment profile editor"));
        setObjectName("graphProfileEditor");
        soundcurrent::ui::installButtonHelp();
        const auto available = screen()->availableGeometry();
        resize(std::min(800, available.width() - 40), std::min(720, available.height() - 40));
        auto *outer = new QVBoxLayout(this);
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        auto *content = new QWidget;
        auto *layout = new QVBoxLayout(content);
        scroll->setWidget(content);
        outer->addWidget(scroll);
        if (draft.estimated) {
            auto *estimateNotice = new QLabel(SC_TR("Estimated profile") + "\n" + draft.estimation.value("limitations").toString());
            estimateNotice->setObjectName("estimatedProfileNotice");
            estimateNotice->setWordWrap(true);
            estimateNotice->setTextFormat(Qt::PlainText);
            layout->addWidget(estimateNotice);
        }
        auto *nameRow = new QFormLayout;
        model = new QLineEdit(draft.model);
        model->setObjectName("profileModel");
        model->setMaxLength(120);
        nameRow->addRow(SC_TR("Model"), model);
        layout->addLayout(nameRow);
        connect(model, &QLineEdit::textEdited, this, [this] { dirty = true; });
        auto *help = new QLabel(SC_TR("Drag points to shape the correction. Double-click to add a point."));
        help->setWordWrap(true);
        layout->addWidget(help);
        plot = new Plot;
        plot->setObjectName("profileCorrectionGraph");
        plot->profile = draft;
        if(draft.estimated) plot->setAccessibleName(SC_TR("Estimated profile"));
        plot->setToolTip(SC_TR("Drag points to shape the correction. Double-click to add a point.") + "\n" +
                         SC_TR("Arrow keys adjust the selected point; Delete removes it."));
        plot->setAccessibleDescription(plot->toolTip());
        layout->addWidget(plot);
        auto *legend = new QLabel(curveLegend(draft));
        legend->setWordWrap(true);
        layout->addWidget(legend);
        auto *notice =
            new QLabel(SC_TR("Drawn corrections are listening adjustments, not equipment measurements."));
        notice->setWordWrap(true);
        layout->addWidget(notice);
        auto *simple = new QFormLayout;
        selection = new QComboBox;
        selection->setObjectName("profileSelectedPoint");
        simple->addRow(SC_TR("Selected band"), selection);
        frequency = new ProfileSlider;
        frequency->setRange(0, 1000);
        frequency->setObjectName("profilePointFrequency");
        frequency->setAccessibleName(SC_TR("Frequency Hz"));
        simple->addRow(SC_TR("Frequency Hz"), frequency);
        gain = new ProfileSlider;
        gain->setRange(-60, 60);
        gain->setObjectName("profilePointGain");
        gain->setAccessibleName(SC_TR("Gain dB"));
        simple->addRow(SC_TR("Gain dB"), gain);
        bandwidth = new ProfileSlider;
        bandwidth->setRange(0, 100);
        bandwidth->setObjectName("profilePointWidth");
        bandwidth->setAccessibleName(SC_TR("Width"));
        bandwidth->setToolTip(SC_TR("Move right for a broader adjustment."));
        simple->addRow(SC_TR("Width"), bandwidth);
        value = new QLabel;
        value->setTextFormat(Qt::PlainText);
        value->setWordWrap(true);
        simple->addRow(value);
        layout->addLayout(simple);
        auto *tools = new QGridLayout;
        add = new QPushButton(SC_TR("Add point"));
        remove = new QPushButton(SC_TR("Remove point"));
        undo = new QPushButton(SC_TR("Undo"));
        auto *flat = new QPushButton(SC_TR("Reset to flat"));
        const QVector<QPushButton *> actions{add, remove, undo, flat};
        int actionWidth = 1;
        for (auto *action : actions)
            actionWidth = std::max(actionWidth, action->sizeHint().width());
        const int columns = std::clamp((this->width() - 64) / (actionWidth + 6), 1, 4);
        for (int i = 0; i < actions.size(); ++i)
            tools->addWidget(actions[i], i / columns, i % columns);
        layout->addLayout(tools);
        undo->setToolTip(SC_TR("Restore the previous EQ setting (Ctrl+Z)"));
        auto *advanced = new QGroupBox(SC_TR("Advanced"));
        advanced->setCheckable(true);
        advanced->setChecked(false);
        advanced->setObjectName("profileAdvanced");
        advanced->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        auto *advancedLayout = new QVBoxLayout(advanced);
        table = new QTableWidget(0, 4);
        table->setObjectName("profileFilterTable");
        table->setHorizontalHeaderLabels({SC_TR("Type"), SC_TR("Frequency Hz"), SC_TR("Gain dB"), "Q"});
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        advancedLayout->addWidget(table);
        table->hide();
        const int collapsedHeight = advanced->fontMetrics().height() + 20;
        advanced->setMaximumHeight(collapsedHeight);
        connect(advanced, &QGroupBox::toggled, this, [advanced, this, collapsedHeight](bool expanded) {
            table->setVisible(expanded);
            advanced->setMaximumHeight(expanded ? QWIDGETSIZE_MAX : collapsedHeight);
        });
        layout->addWidget(advanced);
        auto *details = new QGroupBox(SC_TR("Profile details"));
        details->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        details->setCheckable(true);
        details->setChecked(false);
        auto *detailsOuter = new QVBoxLayout(details);
        auto *fields = new QWidget;
        auto *form = new QFormLayout(fields);
        auto field = [&](const QString &label, const QString &text, int limit = 120) {
            auto *e = new QLineEdit(text);
            e->setMaxLength(limit);
            e->setCursorPosition(0);
            form->addRow(label, e);
            connect(e, &QLineEdit::textEdited, this, [this] { dirty = true; });
            return e;
        };
        brand = field(SC_TR("Brand"), draft.brand);
        family = field(SC_TR("Family"), draft.family);
        equipmentType = field(SC_TR("Equipment subtype"), draft.equipmentType);
        powerType = field(SC_TR("Active / passive / unknown"), draft.powerType);
        source = field(SC_TR("Source"), draft.source, 2048);
        source->setObjectName("profileSource");
        conditions = field(SC_TR("Conditions"), draft.conditions, 2000);
        conditions->setObjectName("profileConditions");
        detailsOuter->addWidget(fields);
        fields->hide();
        details->setMaximumHeight(collapsedHeight);
        connect(details, &QGroupBox::toggled, this, [details, fields, collapsedHeight](bool expanded) {
            fields->setVisible(expanded);
            details->setMaximumHeight(expanded ? QWIDGETSIZE_MAX : collapsedHeight);
        });
        layout->addWidget(details);
        layout->addStretch();
        auto *bar = new QHBoxLayout;
        previewButton = new QPushButton(SC_TR("Preview"));
        previewButton->setObjectName("previewProfile");
        previewButton->setCheckable(true);
        previewButton->setEnabled(bool(preview));
        previewButton->setToolTip(
            SC_TR("Preview is temporary. Closing the editor restores the previous correction."));
        bar->addWidget(previewButton);
        bar->addStretch();
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Save)->setText(SC_TR("Save copy"));
        bar->addWidget(buttons);
        outer->addLayout(bar);
        connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            if (persist())
                accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &Editor::reject);
        connect(previewButton, &QPushButton::toggled, this, [this](bool on) {
            if (!preview)
                return;
            if (!preview(on ? std::optional<Profile>(auditionProfile()) : std::nullopt)) {
                QSignalBlocker block(previewButton);
                previewButton->setChecked(false);
                if (on)
                    preview(std::nullopt);
            }
        });
        connect(selection, &QComboBox::currentIndexChanged, this, [this](int row) {
            if (restoring || row < 0)
                return;
            plot->selected = row;
            table->setCurrentCell(row, 0);
            syncSimple();
            plot->update();
        });
        auto sliderGesture = [this](QSlider *slider) {
            connect(slider, &QSlider::sliderPressed, this, [this] { beginGesture(); });
            connect(slider, &QSlider::sliderReleased, this, [this] { gesture = false; });
        };
        sliderGesture(frequency);
        sliderGesture(gain);
        sliderGesture(bandwidth);
        connect(frequency, &QSlider::valueChanged, this,
                [this](int v) { editSimple(1, 20 * std::pow(1000., v / 1000.)); });
        connect(gain, &QSlider::valueChanged, this, [this](int v) { editSimple(2, v / 10.); });
        connect(bandwidth, &QSlider::valueChanged, this,
                [this](int v) { editSimple(3, 6 * std::pow(.1 / 6., v / 100.)); });
        connect(table, &QTableWidget::currentCellChanged, this, [this](int row) {
            if (restoring || row < 0)
                return;
            selection->setCurrentIndex(row);
        });
        connect(add, &QPushButton::clicked, this, [this] { addPoint(freeFrequency(), 0); });
        connect(remove, &QPushButton::clicked, this, [this] { removePoint(); });
        connect(undo, &QPushButton::clicked, this, [this] {
            if (history.isEmpty())
                return;
            gesture = false;
            plot->dragging = -1;
            draft.filters = history.takeLast();
            dirty = true;
            rebuild();
            notifyPreview();
        });
        auto *undoKey = new QShortcut(QKeySequence::Undo, this);
        connect(undoKey, &QShortcut::activated, undo, &QPushButton::click);
        connect(flat, &QPushButton::clicked, this, [this] {
            remember();
            for (auto &b : draft.filters)
                b.gainDb = 0;
            dirty = true;
            rebuild();
            notifyPreview();
        });
        plot->onSelect = [this](int row) { selection->setCurrentIndex(row); };
        plot->onBegin = [this] { beginGesture(); };
        plot->onEnd = [this] { gesture = false; };
        plot->onAdd = [this](double f, double g) { addPoint(f, g); };
        plot->onRemove = [this] { removePoint(); };
        plot->onDrag = [this](int row, double f, double g) {
            if (row < 0 || row >= table->rowCount())
                return;
            restoring = true;
            qobject_cast<QDoubleSpinBox *>(table->cellWidget(row, 1))->setValue(f);
            qobject_cast<QDoubleSpinBox *>(table->cellWidget(row, 2))->setValue(g);
            restoring = false;
            changed(row, -1);
        };
        rebuild();
    }
    ~Editor() override {
        if (preview && previewButton->isChecked()) {
            try {
                preview(std::nullopt);
            } catch (...) {
            }
        }
    }
    Profile auditionProfile() const {
        auto p = draft;
        p.custom = true;
        p.model = model->text().trimmed();
        return p;
    }
    void notifyPreview() {
        if (preview && previewButton->isChecked() && !preview(auditionProfile())) {
            QSignalBlocker block(previewButton);
            previewButton->setChecked(false);
            preview(std::nullopt);
        }
    }
    void remember() {
        history.append(draft.filters);
        if (history.size() > 50)
            history.removeFirst();
        undo->setEnabled(true);
    }
    void beginGesture() {
        gesture = true;
        gestureRecorded = false;
    }
    double freeFrequency() const {
        QVector<double> frequencies{20, 20000};
        for (const auto &b : draft.filters)
            frequencies.append(b.frequency);
        std::sort(frequencies.begin(), frequencies.end());
        double best = 0, result = 1000;
        for (int i = 1; i < frequencies.size(); ++i) {
            const auto gap = std::log(frequencies[i] / frequencies[i - 1]);
            if (gap > best) {
                best = gap;
                result = std::sqrt(frequencies[i] * frequencies[i - 1]);
            }
        }
        return result;
    }
    void addPoint(double f, double g) {
        if (draft.filters.size() >= 32)
            return;
        remember();
        draft.filters.append({f, g, 1});
        plot->selected = draft.filters.size() - 1;
        dirty = true;
        rebuild();
        notifyPreview();
    }
    void removePoint() {
        if (draft.filters.size() <= 1)
            return;
        remember();
        draft.filters.removeAt(std::clamp(plot->selected, 0, int(draft.filters.size()) - 1));
        dirty = true;
        rebuild();
        notifyPreview();
    }
    void editSimple(int column, double v) {
        if (restoring)
            return;
        const int row = selection->currentIndex();
        if (row < 0)
            return;
        qobject_cast<QDoubleSpinBox *>(table->cellWidget(row, column))->setValue(v);
    }
    void syncSimple() {
        if (draft.filters.isEmpty())
            return;
        const QSignalBlocker a(frequency), b(gain), c(bandwidth);
        const auto &p = draft.filters[std::clamp(plot->selected, 0, int(draft.filters.size()) - 1)];
        frequency->setValue(int(std::lround(std::log(p.frequency / 20.) / std::log(1000.) * 1000)));
        gain->setValue(int(std::lround(p.gainDb * 10)));
        bandwidth->setValue(int(std::lround(std::log(p.q / 6.) / std::log(.1 / 6.) * 100)));
        value->setText(QLocale().toString(p.frequency, 'f', 1) + " Hz · " +
                       QLocale().toString(p.gainDb, 'f', 1) + " dB · " + SC_TR("Width") + " " +
                       QLocale().toString(bandwidth->value()) + QLocale().percent());
        frequency->setAccessibleDescription(QLocale().toString(p.frequency, 'f', 1) + " Hz");
        gain->setAccessibleDescription(QLocale().toString(p.gainDb, 'f', 1) + " dB");
        bandwidth->setAccessibleDescription(QLocale().toString(bandwidth->value()) + QLocale().percent());
        plot->setAccessibleDescription(plot->toolTip() + "\n" + value->text());
    }
    void rebuild() {
        restoring = true;
        table->setRowCount(0);
        selection->clear();
        for (int row = 0; row < draft.filters.size(); ++row) {
            table->insertRow(row);
            addRow(row, draft.filters[row]);
            selection->addItem(QLocale().toString(row + 1));
        }
        plot->selected = std::clamp(plot->selected, 0, int(draft.filters.size()) - 1);
        selection->setCurrentIndex(plot->selected);
        table->setCurrentCell(plot->selected, 0);
        restoring = false;
        plot->profile = draft;
        plot->update();
        syncSimple();
        add->setEnabled(draft.filters.size() < 32);
        remove->setEnabled(draft.filters.size() > 1);
        undo->setEnabled(!history.isEmpty());
    }
    void addRow(int row, const EqBand &band) {
        auto *type = new QComboBox;
        type->addItems({"PK", "LS", "HS"});
        type->setCurrentText(typeName(band.type));
        table->setCellWidget(row, 0, type);
        connect(type, &QComboBox::currentIndexChanged, this, [this, row] { changed(row, 0); });
        for (int column = 1; column < 4; ++column) {
            auto *spin = new soundcurrent::AcceleratingDoubleSpinBox;
            spin->setDecimals(8);
            spin->setRange(column == 1 ? 20 : column == 2 ? -6 : .1, column == 1 ? 20000 : 6);
            spin->setValue(column == 1 ? band.frequency : column == 2 ? band.gainDb : band.q);
            spin->setAccessibleName(table->horizontalHeaderItem(column)->text());
            table->setCellWidget(row, column, spin);
            connect(spin, &QDoubleSpinBox::valueChanged, this, [this, row, column] { changed(row, column); });
        }
    }
    void changed(int row, int column) {
        if (restoring || row < 0 || row >= draft.filters.size())
            return;
        auto next = draft.filters[row];
        if (column == 0) {
            const auto type = qobject_cast<QComboBox *>(table->cellWidget(row, 0))->currentText();
            next.type = type == "LS"   ? FilterType::LowShelf
                        : type == "HS" ? FilterType::HighShelf
                                       : FilterType::Peaking;
        }
        if (column == 1 || column == -1)
            next.frequency = qobject_cast<QDoubleSpinBox *>(table->cellWidget(row, 1))->value();
        if (column == 2 || column == -1)
            next.gainDb = qobject_cast<QDoubleSpinBox *>(table->cellWidget(row, 2))->value();
        if (column == 3)
            next.q = qobject_cast<QDoubleSpinBox *>(table->cellWidget(row, 3))->value();
        const auto &old = draft.filters[row];
        if (next.frequency == old.frequency && next.gainDb == old.gainDb && next.q == old.q &&
            next.type == old.type)
            return;
        if (!gesture || !gestureRecorded) {
            remember();
            if (gesture)
                gestureRecorded = true;
        }
        draft.filters[row] = next;
        dirty = true;
        plot->profile = draft;
        plot->update();
        syncSimple();
        notifyPreview();
    }
    bool persist() {
        draft.brand = brand->text().trimmed();
        draft.family = family->text().trimmed();
        draft.equipmentType = equipmentType->text().trimmed();
        draft.powerType = powerType->text().trimmed();
        draft.model = model->text().trimmed();
        draft.source = source->text();
        draft.conditions = conditions->text();
        draft.custom = true;
        draft.provenance = originalProvenance.left(1800) + "\n" +
                           (originalId.isEmpty() ? SC_TR("User-created profile")
                                                 : SC_TR("Custom copy of %1").arg(originalId));
        draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (save(draft)) {
            dirty = false;
            return true;
        }
        return false;
    }
    void reject() override {
        if (dirty) {
            const auto answer =
                ask(this, SC_TR("Save modified profile?"),
                    SC_TR("This profile has changed. Save a custom copy before leaving?"),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
            if (answer == QMessageBox::Cancel)
                return;
            if (answer == QMessageBox::Save && !persist())
                return;
        }
        QDialog::reject();
    }
};

} // namespace
QString profileLabel(const Profile &p) {
    return p.model + (p.estimated ? " [" + SC_TR("Estimated profile") + "]" : QString());
}
Profile estimateProfile(const Profile &chain, const QVector<Profile> &known,
                        const QString &targetKind, const QString &conditions) {
    require(QStringList{"speaker", "microphone", "amplifier"}.contains(targetKind), "Invalid estimated equipment kind.");
    require(!conditions.trimmed().isEmpty() && conditions.size() <= 2000, "Measurement conditions are required.");
    require(!known.isEmpty() && known.size() <= 8, "Select between one and eight known component responses.");
    // Validate public API inputs as strictly as imported files before any interpolation.
    const auto measured = parse(QJsonDocument(serialize(chain)).toJson());
    require(measured.response.size() >= 6, "A measured chain response is required; correction filters cannot be subtracted.");
    double low = std::max(20., measured.response.front().frequency);
    double high = std::min(20000., measured.response.back().frequency);
    QVector<Profile> inputs;
    QJsonArray sources;
    auto provenance = [](const Profile &p) {
        return QJsonObject{{"id",p.id},{"label",p.brand + " / " + p.model},
            {"source",p.source},{"conditions",p.conditions},{"estimated",p.estimated},
            {"sha256",QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(serialize(p)).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex())}};
    };
    QStringList identities;
    for (const auto &input : known) {
        auto p = parse(QJsonDocument(serialize(input)).toJson());
        require(p.response.size() >= 6, "A measured component response is required; correction filters cannot be subtracted.");
        const auto identity = provenance(p).value("sha256").toString();
        require(identity != provenance(measured).value("sha256").toString() && !identities.contains(identity), "A component cannot be subtracted twice or from itself.");
        identities.append(identity);
        low = std::max(low, p.response.front().frequency);
        high = std::min(high, p.response.back().frequency);
        sources.append(provenance(p)); inputs.append(p);
    }
    require(high >= low * 4., "Measured responses must overlap by at least two octaves.");
    Profile result;
    result.kind=targetKind; result.custom=true; result.estimated=true;
    result.brand=SC_TR("Custom"); result.family=SC_TR("Estimated profile"); result.model=SC_TR("Estimated profile");
    result.source=measured.source; result.conditions=conditions.trimmed();
    const QString limitations=SC_TR("This is an estimated residual, not a certified equipment measurement. Any unknown room, microphone, clothing, placement or interface response remains included. Published curves may not match your setup. Do not correct deep nulls or unmeasured frequencies.");
    result.estimation={{"method","chain-minus-known-responses-db-v1"},{"createdUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"measurement",provenance(measured)},{"knownComponents",sources},{"targetKind",targetKind},
        {"interpretation","combined-residual"},{"limitations",limitations},{"lowHz",low},{"highHz",high}};
    QStringList componentNames;
    for(const auto &p:inputs) componentNames.append((p.brand+" / "+profileLabel(p)).left(100));
    result.provenance=SC_TR("Estimated profile") + "\n" + limitations + "\n" +
        SC_TR("Measured chain") + ": " + (measured.brand+" / "+profileLabel(measured)).left(100) + "\n" +
        SC_TR("Known component responses") + ": " + componentNames.join("; ");
    for(int i=0;i<96;++i) {
        const double hz=low*std::pow(high/low,i/95.);
        double db=responseAt(measured.response,hz);
        for(const auto &p:inputs) db-=responseAt(p.response,hz);
        require(std::isfinite(db) && std::abs(db)<=120., "Estimated response exceeds the supported range.");
        result.response.append({hz,db});
    }
    result.filters=fitResponse(result.response);
    parse(QJsonDocument(serialize(result)).toJson()); // Reject oversize provenance before opening the editor.
    return result;
}
QJsonObject serialize(const Profile &p) {
    QJsonArray filters, response;
    for (const auto &b : p.filters)
        filters.append(QJsonObject{
            {"type", typeName(b.type)}, {"frequency", b.frequency}, {"gain", b.gainDb}, {"q", b.q}});
    for (const auto &v : p.response)
        response.append(QJsonArray{v.frequency, v.db});
    return {{"schema", p.estimated ? 4 : p.filters.size() > 16 ? 3 : 2},
            {"estimated", p.estimated}, {"estimation", p.estimation},
            {"id", p.id},
            {"kind", p.kind},
            {"brand", p.brand},
            {"family", p.family},
            {"equipmentType", p.equipmentType}, {"powerType", p.powerType},
            {"model", p.model},
            {"measurementSource", p.source},
            {"conditions", p.conditions},
            {"provenance", p.provenance},
            {"custom", p.custom},
            {"filters", filters},
            {"response", response}};
}
Profile parse(const QByteArray &bytes) {
    require(bytes.size() <= 1024 * 1024, "Profile exceeds the 1 MiB limit.");
    const auto document = QJsonDocument::fromJson(bytes);
    require(document.isObject(),
            "Expected a JSON equipment profile. Import response text using the response import button.");
    const auto o = document.object();
    const int schema = o.value("schema").toInt();
    require(schema == 2 || schema == 3 || schema == 4, "Unsupported equipment profile schema (expected 2/3/4).");
    Profile p;
    p.id = o.value("id").toString();
    p.kind = o.value("kind").toString();
    p.brand = o.value("brand").toString().trimmed();
    p.family = o.value("family").toString().trimmed();
    p.equipmentType = o.value("equipmentType").toString("Unclassified").trimmed();
    p.powerType = o.value("powerType").toString("Unknown").trimmed();
    require(!p.equipmentType.isEmpty() && p.equipmentType.size()<=120 && !p.powerType.isEmpty() && p.powerType.size()<=120,"Invalid equipment subtype or power type");
    p.model = o.value("model").toString().trimmed();
    p.source = o.value("measurementSource").toString();
    p.conditions = o.value("conditions").toString();
    p.provenance = o.value("provenance").toString();
    p.custom = o.value("custom").toBool();
    require(!o.contains("estimated") || o.value("estimated").isBool(), "Invalid estimated profile status.");
    p.estimated = o.value("estimated").toBool();
    require(schema != 4 || p.estimated, "Estimated profile status is required for schema 4.");
    require(!p.estimated || (schema == 4 && o.value("estimation").isObject()), "Estimated profiles require derivation metadata.");
    require(!o.contains("estimation") || o.value("estimation").isObject(), "Estimated profiles require derivation metadata.");
    p.estimation = o.value("estimation").toObject();
    require(p.estimation.isEmpty() || p.estimated, "Derivation metadata requires an estimated profile.");
    require(QJsonDocument(p.estimation).toJson(QJsonDocument::Compact).size() <= 32768, "Derivation metadata exceeds 32 KiB.");
    require(!p.estimated || (p.estimation.value("method").isString() && !p.estimation.value("method").toString().isEmpty() &&
                p.estimation.value("limitations").isString() && !p.estimation.value("limitations").toString().isEmpty()), "Estimated profiles require method and limitations.");
    require(QStringList{"speaker", "microphone", "amplifier"}.contains(p.kind),
            "Equipment kind must be speaker, microphone or amplifier.");
    require(!p.brand.isEmpty() && !p.family.isEmpty() && !p.model.isEmpty() && p.brand.size() <= 120 &&
                p.family.size() <= 120 && p.model.size() <= 120,
            "Brand, family and model are required (maximum 120 characters each).");
    require(p.id.size() <= 120 && p.conditions.size() <= 2000 && p.provenance.size() <= 2000 &&
                p.source.size() <= 2048,
            "Profile metadata is too long.");
    require(!p.conditions.isEmpty(), "Measurement conditions are required.");
    const QUrl url(p.source);
    require(p.custom || (url.isValid() && url.scheme() == "https" && !url.host().isEmpty()),
            "Published profiles need an HTTPS measurement source.");
    const auto filters = o.value("filters").toArray();
    require(filters.size() >= 1 && filters.size() <= (schema >= 3 ? 32 : 16), "Profiles need 1–32 correction filters.");
    for (const auto &value : filters) {
        require(value.isObject(), "Invalid filter.");
        const auto f = value.toObject();
        const auto type = f.value("type").toString();
        require(QStringList{"PK", "LS", "HS"}.contains(type), "Unsupported filter type.");
        require(f.value("frequency").isDouble() && f.value("gain").isDouble() && f.value("q").isDouble(),
                "Filter values must be numbers.");
        EqBand b{f.value("frequency").toDouble(), f.value("gain").toDouble(), f.value("q").toDouble(),
                 type == "LS"   ? FilterType::LowShelf
                 : type == "HS" ? FilterType::HighShelf
                                : FilterType::Peaking};
        require(std::isfinite(b.frequency) && std::isfinite(b.gainDb) && std::isfinite(b.q) &&
                    b.frequency >= 20 && b.frequency <= 20000 && std::abs(b.gainDb) <= 6 && b.q >= .1 &&
                    b.q <= 6,
                "Filters exceed frequency, gain or Q limits.");
        p.filters.append(b);
    }
    const auto response = o.value("response").toArray();
    require(response.size() <= 4096, "Response exceeds 4096 points.");
    for (const auto &v : response) {
        const auto a = v.toArray();
        require(a.size() == 2 && a[0].isDouble() && a[1].isDouble(), "Invalid response point.");
        Point pt{a[0].toDouble(), a[1].toDouble()};
        require(std::isfinite(pt.frequency) && std::isfinite(pt.db) && pt.frequency >= 10 &&
                    pt.frequency <= 40000 && std::abs(pt.db) <= 200 &&
                    (p.response.isEmpty() || pt.frequency > p.response.back().frequency),
                "Response frequencies must increase, with finite bounded values.");
        p.response.append(pt);
    }
    if (p.id.isEmpty())
        p.id = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    return p;
}
QVector<EqBand> fitResponse(const QVector<Point> &points) {
    require(points.size() >= 2 && points.size() <= 4096, "Response needs 2–4096 measured points.");
    double previous = 0;
    for (const auto &pt : points) {
        require(std::isfinite(pt.frequency) && std::isfinite(pt.db) && pt.frequency >= 10 &&
                    pt.frequency <= 40000 && std::abs(pt.db) <= 200 && pt.frequency > previous,
                "Invalid or unordered measured response.");
        previous = pt.frequency;
    }
    QVector<EqBand> bands;
    const double low = std::max(20., points.front().frequency),
                 high = std::min(20000., points.back().frequency);
    require(high > low, "Response has no usable audio range.");
    for (int i = 0; i < 16; ++i)
        bands.append({low * std::pow(high / low, (i + .5) / 16.), 0, 1.4});
    // Bounded coordinate descent against measured relative response; no extrapolation or phase
    // reconstruction.
    for (int iteration = 0; iteration < 8; ++iteration)
        for (auto &b : bands) {
            double numerator = 0, denominator = 0;
            const double old = b.gainDb;
            b.gainDb = 1;
            for (int i = 0; i < 128; ++i) {
                const double f = low * std::pow(high / low, i / 127.);
                const double basis = filterResponseDb(b, 48000, f);
                double current = 0;
                for (const auto &other : bands)
                    if (&other != &b)
                        current += filterResponseDb(other, 48000, f);
                numerator += basis * (-responseAt(points, f) - current);
                denominator += basis * basis;
            }
            b.gainDb = denominator > 0 ? std::clamp(numerator / denominator, -6., 6.) : old;
            if (b.frequency < 80 && b.gainDb > 0)
                b.gainDb = 0;
        }
    return bands;
}
QString libraryPath() {
    return qApp->arguments().contains("--ui-self-test")
               ? QFileInfo(QSettings().fileName()).absolutePath() + "/equipment.json"
               : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/equipment.json";
}
QVector<Profile> loadLibrary() {
    QFile f(libraryPath());
    if (!f.exists())
        return {};
    require(f.open(QIODevice::ReadOnly), "Cannot read profile library.");
    require(f.size() <= 16 * 1024 * 1024, "Profile library exceeds 16 MiB.");
    const auto doc = QJsonDocument::fromJson(f.readAll());
    require(doc.isArray() && doc.array().size() <= 256, "Invalid profile library.");
    QVector<Profile> out;
    for (const auto &v : doc.array())
        out.append(parse(QJsonDocument(v.toObject()).toJson()));
    return out;
}
void saveLibrary(const QVector<Profile> &profiles) {
    require(profiles.size() <= 256, "The custom library holds up to 256 profiles.");
    QJsonArray a;
    for (const auto &p : profiles) {
        const auto o = serialize(p);
        parse(QJsonDocument(o).toJson());
        a.append(o);
    }
    const auto bytes = QJsonDocument(a).toJson();
    require(bytes.size() <= 16 * 1024 * 1024, "Library exceeds 16 MiB.");
    require(QDir().mkpath(QFileInfo(libraryPath()).absolutePath()), "Cannot create profile folder.");
    QSaveFile f(libraryPath());
    require(f.open(QIODevice::WriteOnly), "Cannot save profile library.");
    require(f.write(bytes) == bytes.size() && f.commit(), "Cannot finish saving profile library.");
}
QVector<Profile> bundledProfiles() {
    QVector<Profile> out;
    for (const auto &path : {":/equipment/profiles.json", ":/equipment/spinorama.json", ":/equipment/published.json"}) {
        QFile f(path);
        require(f.open(QIODevice::ReadOnly), "Equipment resource missing.");
        for (const auto &v : QJsonDocument::fromJson(f.readAll()).array())
            out.append(parse(QJsonDocument(v.toObject()).toJson()));
    }
    return out;
}
Profile importProfile(Profile p) {
    p = parse(QJsonDocument(serialize(p)).toJson());
    auto saved = loadLibrary();
    auto known = saved; known += bundledProfiles();
    auto content = [](const Profile &profile) {
        auto json = serialize(profile);
        for (const auto *key : {"id", "custom", "provenance"}) json.remove(key);
        return json;
    };
    const auto importedContent = content(p);
    for (const auto &existing : known)
        if (existing.id == p.id && content(existing) == importedContent) return existing;
    for (const auto &existing : known)
        if (existing.kind == p.kind && existing.brand == p.brand && existing.model == p.model && content(existing) == importedContent) return existing;
    if (p.id.isEmpty() || std::any_of(known.begin(), known.end(), [&](const Profile &existing) { return existing.id == p.id; }))
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    saved.append(p); saveLibrary(saved);
    return p;
}
void saveNewProfile(QWidget *parent, Profile p, const Preview &preview) {
    Editor editor(p, parent, [parent](Profile draft) {
        try {
            auto profiles = loadLibrary();
            profiles.append(draft);
            saveLibrary(profiles);
            return true;
        } catch (const std::exception &e) {
            QMessageBox::warning(parent, SC_TR("Save profile"), e.what());
            return false;
        }
    }, preview);
    editor.dirty = true;
    editor.exec();
}
void openLibrary(QWidget *parent, const std::function<void(const Profile &)> &apply, const QString &initialKind, const Preview &preview, const std::optional<Profile> &initialProfile, const Removed &removed) {
    QDialog dialog(parent);
    soundcurrent::ui::installButtonHelp();
    dialog.setWindowTitle(SC_TR("Equipment profiles — brand / family / model"));
    const auto available = dialog.screen()->availableGeometry();
    dialog.resize(std::max(1,std::min(800,available.width()-40)),
                  std::max(1,std::min(650,available.height()-40)));
    dialog.setObjectName("equipmentProfileManager");
    auto *outer = new QVBoxLayout(&dialog);
    auto *appliedProfile = new QLabel(&dialog);
    auto activeSelection = initialProfile;
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    scroll->setWidget(content);
    outer->addWidget(scroll);
    auto *search = new QLineEdit;
    search->setPlaceholderText(SC_TR("Search brand, family, model or measurement conditions"));
    layout->addWidget(search);
    auto *list = new ProfileCombo;
    list->setObjectName("equipmentProfileList");
    list->setMaxVisibleItems(15);
    list->setAccessibleName(SC_TR("Equipment profiles by brand family and model"));
    layout->addWidget(list);
    auto *details = new QLabel;
    details->setWordWrap(true);
    details->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    details->setTextFormat(Qt::PlainText);
    details->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    layout->addWidget(details);
        layout->addStretch();
    auto *legend = new QLabel(curveLegend({}));
    legend->setWordWrap(true);
    layout->addWidget(legend);
    auto *plot = new Plot;
    layout->addWidget(plot);
    auto *customOnly = new QCheckBox(SC_TR("Custom"));
    customOnly->setObjectName("customProfilesOnly");
    customOnly->setToolTip(SC_TR("Show saved custom profiles only."));
    layout->insertWidget(0, customOnly);
    auto *customList = new QListWidget;
    customList->setObjectName("savedCustomProfiles");
    customList->setAccessibleName(SC_TR("Custom"));
    customList->setMaximumHeight(125);
    customList->hide();
    layout->insertWidget(layout->indexOf(details), customList);
    QVector<Profile> custom = loadLibrary(), profiles = bundledProfiles();
    const int bundledCount = profiles.size();
    profiles += custom;
    // An active imported correction may predate the saved library. Offer it
    // for editing without silently persisting it or changing the audio route.
    if (initialProfile && !initialProfile->id.isEmpty() &&
        std::none_of(profiles.begin(), profiles.end(), [&](const Profile &p) {
            return p.id == initialProfile->id && serialize(p) == serialize(*initialProfile);
        })) profiles.append(*initialProfile);

    auto *taxonomy = new QGridLayout;
    auto *kindFilter = new QComboBox;
    kindFilter->setObjectName("equipmentKindFilter");
    kindFilter->addItem(SC_TR("All equipment"), QString());
    kindFilter->addItem(SC_TR("Speaker"), "speaker");
    kindFilter->addItem(SC_TR("Microphone"), "microphone");
    kindFilter->addItem(SC_TR("Amplifier"), "amplifier");
    kindFilter->setCurrentIndex(std::max(0, kindFilter->findData(initialKind)));
    kindFilter->setAccessibleName(SC_TR("Equipment type"));
    auto *brandFilter = new QComboBox;
    brandFilter->setAccessibleName(SC_TR("Equipment brand"));
    auto *familyFilter = new QComboBox;
    familyFilter->setAccessibleName(SC_TR("Equipment family"));
    auto *subtypeFilter=new QComboBox;subtypeFilter->setObjectName("equipmentSubtypeFilter");subtypeFilter->setAccessibleName(SC_TR("Equipment subtype"));
    for (auto *combo : {static_cast<QComboBox *>(list), kindFilter, brandFilter, familyFilter, subtypeFilter}) {
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(8);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    int taxonomyWidth = 1;
    const QVector<QComboBox *> taxonomyChoices{kindFilter, brandFilter, familyFilter, subtypeFilter};
    for (auto *combo : taxonomyChoices) taxonomyWidth = std::max(taxonomyWidth, combo->minimumSizeHint().width());
    const int taxonomyColumns = std::clamp((dialog.width() - 64) / (taxonomyWidth + std::max(6, taxonomy->spacing())), 1, 4);
    for (int i = 0; i < taxonomyChoices.size(); ++i) taxonomy->addWidget(taxonomyChoices[i], i / taxonomyColumns, i % taxonomyColumns);
    layout->insertLayout(0, taxonomy);
    auto taxonomyRefresh = [&] {
        const QSignalBlocker b(brandFilter), f(familyFilter), st(subtypeFilter);
        const auto brand = brandFilter->currentText(), family = familyFilter->currentText();
        QStringList brands, families, subtypes;
        for (const auto &p : profiles)
            if (kindFilter->currentIndex() == 0 || p.kind == kindFilter->currentData().toString()) {
                if (!subtypes.contains(p.equipmentType))subtypes.append(p.equipmentType);
                if (!brands.contains(p.brand))
                    brands.append(p.brand);
                if ((brand == p.brand || brandFilter->currentIndex() == 0 || brand.isEmpty()) &&
                    !families.contains(p.family))
                    families.append(p.family);
            }
        const auto subtype=subtypeFilter->currentData().toString();subtypes.sort(Qt::CaseInsensitive);subtypeFilter->clear();subtypeFilter->addItem(SC_TR("All subtypes"));for(const auto &type:subtypes)subtypeFilter->addItem(soundcurrent::i18n::equipmentTypeText(type),type);subtypeFilter->setCurrentIndex(std::max(0,subtypeFilter->findData(subtype)));
        brands.sort(Qt::CaseInsensitive);
        families.sort(Qt::CaseInsensitive);
        brandFilter->clear();
        brandFilter->addItem(SC_TR("All brands"));
        brandFilter->addItems(brands);
        brandFilter->setCurrentIndex(std::max(0, brandFilter->findText(brand)));
        familyFilter->clear();
        familyFilter->addItem(SC_TR("All families"));
        familyFilter->addItems(families);
        familyFilter->setCurrentIndex(std::max(0, familyFilter->findText(family)));
    };
    taxonomyRefresh();
    QPushButton *remove = nullptr;
    bool refreshing = false;
    bool noneWasApplied = false;
    QString lastAppliedId;
    auto refresh = [&] {
        refreshing = true;
        list->clear();
        customList->clear();
        list->addItem(SC_TR("None — use my own EQ"), -1);
        for (int i = 0; i < profiles.size(); ++i) {
            const auto &p = profiles[i];
            const QString name =
                (p.kind == "speaker" ? SC_TR("Speaker") : p.kind == "microphone" ? SC_TR("Microphone") : SC_TR("Amplifier")) + " / " + p.brand + " / " + p.family + " / " + profileLabel(p) + (p.custom ? SC_TR(" [custom]") : QString());
            if ((!customOnly->isChecked() || i >= bundledCount) && (kindFilter->currentIndex() == 0 || p.kind == kindFilter->currentData().toString()) &&
                (brandFilter->currentIndex() == 0 || p.brand == brandFilter->currentText()) &&
                (familyFilter->currentIndex() == 0 || p.family == familyFilter->currentText()) &&
                (subtypeFilter->currentIndex()==0 || p.equipmentType==subtypeFilter->currentData().toString()) &&
                (name + " " + p.conditions).contains(search->text(), Qt::CaseInsensitive))
            {
                list->addItem(name, i);
                if (i >= bundledCount) {
                    auto *item = new QListWidgetItem(name, customList);
                    item->setData(Qt::UserRole, list->count() - 1);
                    item->setToolTip(name);
                }
            }
        }
        list->setCurrentIndex(0);
        noneWasApplied = false;
        refreshing = false;
    };
    QObject::connect(list, &QComboBox::currentIndexChanged, &dialog, [&] {
        const QSignalBlocker block(customList);
        customList->setCurrentRow(-1); customList->clearSelection();
        for (int row = 0; row < customList->count(); ++row)
            if (customList->item(row)->data(Qt::UserRole).toInt() == list->currentIndex()) {
                customList->setCurrentRow(row); break;
            }
        if (list->currentIndex() < 0 || list->currentData().toInt() < 0) {
            details->clear();
            plot->profile = {};
            plot->hide();
            plot->update();
            return;
        }
        const auto &p = profiles[list->currentData().toInt()];
        details->setText((p.estimated ? SC_TR("Estimated profile") + "\n" : QString()) + p.source + "\n" + p.conditions + "\n" + p.provenance + (p.estimated ? "\n" + p.estimation.value("limitations").toString() : QString()));
        plot->profile = p;
        legend->setText(curveLegend(p));
        plot->setAccessibleName(p.estimated ? SC_TR("Estimated profile") : SC_TR("Published response and editable correction curves"));
        plot->show();
        plot->update();
    });
    auto updateRemove = [&] {
        if (!remove) return;
        const int i = list->currentData().toInt();
        const bool selected = list->currentIndex() >= 0 && i >= 0 && i < profiles.size();
        remove->setEnabled(selected && i >= bundledCount);
    };
    QObject::connect(list, &QComboBox::currentIndexChanged, &dialog, updateRemove);
    QObject::connect(customOnly, &QCheckBox::toggled, &dialog, [&](bool on) {
        if (on) { const QSignalBlocker a(brandFilter), b(familyFilter), c(subtypeFilter);
            brandFilter->setCurrentIndex(0); familyFilter->setCurrentIndex(0); subtypeFilter->setCurrentIndex(0); }
        customList->setVisible(on); taxonomyRefresh(); refresh();
    });
    QObject::connect(customList, &QListWidget::currentItemChanged, &dialog, [&](QListWidgetItem *item) {
        if (!refreshing && item) list->setCurrentIndex(item->data(Qt::UserRole).toInt());
    });
    auto activateCustom = [&](QListWidgetItem *item) {
        if (refreshing || !item) return;
        const int row = item->data(Qt::UserRole).toInt();
        list->setCurrentIndex(row);
        // Import selects a displayed row without applying it. An explicit click/Enter still applies.
        QMetaObject::invokeMethod(list, "activated", Qt::DirectConnection, Q_ARG(int, row));
    };
    QObject::connect(customList, &QListWidget::itemClicked, &dialog, activateCustom);
    QObject::connect(customList, &QListWidget::itemActivated, &dialog, activateCustom);
    QObject::connect(search, &QLineEdit::textChanged, &dialog, [&] { refresh(); });
    QObject::connect(kindFilter, &QComboBox::currentIndexChanged, &dialog, [&] {
        taxonomyRefresh();
        refresh();
    });
    QObject::connect(brandFilter, &QComboBox::currentIndexChanged, &dialog, [&] {
        taxonomyRefresh();
        refresh();
    });
    QObject::connect(familyFilter, &QComboBox::currentIndexChanged, &dialog, [&] { refresh(); });
    QObject::connect(subtypeFilter,&QComboBox::currentIndexChanged,&dialog,[&]{refresh();});
    auto selectStored = [&](const Profile &p) {
        custom = loadLibrary(); profiles = bundledProfiles(); profiles += custom;
        for (int index = 0; index < bundledCount; ++index)
            if (profiles[index].id == p.id && serialize(profiles[index]) == serialize(p)) {
                const QSignalBlocker block(customOnly); customOnly->setChecked(false); customList->hide(); break;
            }
        lastAppliedId.clear();
        {
            const QSignalBlocker a(kindFilter), b(brandFilter), c(familyFilter), d(search), st(subtypeFilter);
            kindFilter->setCurrentIndex(std::max(0, kindFilter->findData(p.kind)));
            brandFilter->setCurrentIndex(0); familyFilter->setCurrentIndex(0); subtypeFilter->setCurrentIndex(0); search->clear();
        }
        taxonomyRefresh(); refresh();
        refreshing = true;
        for (int i = 1; i < list->count(); ++i)
            if (profiles[list->itemData(i).toInt()].id == p.id && serialize(profiles[list->itemData(i).toInt()]) == serialize(p)) { list->setCurrentIndex(i); break; }
        refreshing = false;
    };
    auto save = [&](Profile p) {
        try {
            p = parse(QJsonDocument(serialize(p)).toJson());
            auto proposed = custom; proposed.append(p); saveLibrary(proposed);
            selectStored(p); return true;
        } catch (const std::exception &e) {
            QMessageBox::warning(&dialog, SC_TR("Profile"), QString::fromUtf8(e.what())); return false;
        }
    };
    auto *row = new QGridLayout;
    layout->insertLayout(layout->indexOf(details), row);
    QVector<QPushButton *> actions;
    auto button = [&](const QString &label) {
        auto *b = new QPushButton(label);
        actions.append(b);
        return b;
    };
    auto *import = button(SC_TR("Import JSON"));
    auto *text = button(SC_TR("Import response text"));
    auto *create = button(SC_TR("Create profile"));
    auto *edit = button(SC_TR("Edit / save copy"));
    auto *estimate = button(SC_TR("Estimate profile"));
    estimate->setObjectName("estimateEquipmentProfile");
    estimate->setToolTip(SC_TR("Subtract known measured responses from a measured chain response, then review the estimated correction."));
    auto *exportButton = button(SC_TR("Export JSON"));
    auto *use = new QPushButton(SC_TR("Apply profile"));
    auto updateSelectionActions = [&] {
        const bool selected = list->currentIndex() >= 0 && list->currentData().toInt() >= 0;
        edit->setEnabled(selected); exportButton->setEnabled(selected);
        estimate->setEnabled(selected && profiles[list->currentData().toInt()].response.size() >= 6);
    };
    QObject::connect(list, &QComboBox::currentIndexChanged, &dialog, updateSelectionActions);
    remove = button(SC_TR("Delete"));
    remove->setObjectName("deleteEquipmentProfile");
    QObject::connect(remove, &QPushButton::clicked, &dialog, [&] {
        if (list->currentIndex() < 0 || list->currentData().toInt() < 0) return;
        const int userIndex = list->currentData().toInt() - bundledCount;
        if (userIndex < 0 || list->currentData().toInt() >= profiles.size()) return;
        const auto selected = profiles[list->currentData().toInt()];
        auto proposed = custom;
        if (ask(&dialog, SC_TR("Delete"), selected.brand + " / " + selected.family + " / " + selected.model,
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        if (userIndex < proposed.size()) proposed.removeAt(userIndex);
        try {
            saveLibrary(proposed);
            custom = proposed; profiles = bundledProfiles(); profiles += custom;
            taxonomyRefresh(); refresh(); updateRemove();
            bool cleared = false;
            if (removed) cleared = removed(selected);
            else if (activeSelection && serialize(*activeSelection) == serialize(selected)) {
                Profile none; none.kind = selected.kind; apply(none); cleared = true;
            }
            if (cleared) {
                activeSelection.reset(); lastAppliedId.clear();
                appliedProfile->setText(QString::fromUtf8("✓ ") + SC_TR("None — use my own EQ"));
            }
        } catch (const std::exception &e) {
            QMessageBox::warning(&dialog, SC_TR("Profile"), QString::fromUtf8(e.what()));
        }
    });
    QObject::connect(import, &QPushButton::clicked, &dialog, [&] {
        const auto path = soundcurrent::i18n::FileDialogs::getOpenFileName(&dialog, SC_TR("Import equipment profile"), {},
                                                       SC_TR("Equipment profiles (*.json)"));
        if (path.isEmpty())
            return;
        try {
            QFile f(path);
            require(f.open(QIODevice::ReadOnly) && f.size() <= 1024 * 1024,
                    "Cannot read profile or file exceeds 1 MiB.");
            auto p = parse(f.readAll());
            if (ask(&dialog, SC_TR("Import profile?"),
                    SC_TR("%1 / %2\n%3\nImport into your library?").arg(p.brand, p.model, p.conditions),
                    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
                selectStored(importProfile(p));
        } catch (const std::exception &e) {
            QMessageBox::warning(&dialog, SC_TR("Import"), e.what());
        }
    });
    QObject::connect(text, &QPushButton::clicked, &dialog, [&] {
        const auto path = soundcurrent::i18n::FileDialogs::getOpenFileName(&dialog, SC_TR("Import relative measured response"), {},
                                                       SC_TR("Response data (*.txt *.csv *.frd *.cal)"));
        if (path.isEmpty())
            return;
        try {
            QFile f(path);
            require(f.open(QIODevice::ReadOnly) && f.size() <= 1024 * 1024,
                    "Cannot read response or file exceeds 1 MiB.");
            Profile p;
            p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            p.kind = "microphone";
            p.brand = SC_TR("Custom");
            p.family = SC_TR("Measured response");
            p.model = QFileInfo(path).completeBaseName().left(120);
            p.custom = true;
            p.conditions = SC_TR("User imported relative frequency response; specify microphone orientation / "
                           "serial, or speaker measurement conditions before use.");
            p.provenance =
                SC_TR("Imported %1; SHA256 %2").arg(QFileInfo(path).fileName(),
                    QString::fromLatin1(QCryptographicHash::hash(f.peek(f.size()), QCryptographicHash::Sha256).toHex()));
            for (const auto &line : QString::fromUtf8(f.readAll()).split('\n')) {
                const auto s = line.trimmed();
                if (s.isEmpty() || s.startsWith('*') || s.startsWith('#') || s.startsWith(';') ||
                    s.startsWith("frequency", Qt::CaseInsensitive))
                    continue;
                const auto tokens = s.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts);
                bool a = false, b = false;
                const double hz = tokens.value(0).toDouble(&a), db = tokens.value(1).toDouble(&b);
                require(a && b && tokens.size() >= 2,
                        "Expected frequency Hz and relative measured response dB on every data line.");
                require(p.response.size() < 4096 && std::isfinite(hz) && std::isfinite(db) && hz >= 10 &&
                            hz <= 40000 && std::abs(db) <= 200 &&
                            (p.response.isEmpty() || hz > p.response.back().frequency),
                        "Invalid or unordered response data.");
                p.response.append({hz, db});
            }
            p.filters = fitResponse(p.response);
            QDialog kindDialog(&dialog);
            kindDialog.setWindowTitle(SC_TR("Import response text"));
            auto *kl = new QVBoxLayout(&kindDialog);
            kl->addWidget(new QLabel(SC_TR("This imports measured RESPONSE, not already-inverted EQ gains. Confirm "
                                     "equipment type. Absolute SPL needs normalization before import.")));
            auto *k = new QComboBox;
            k->setObjectName("equipmentCreationType");
            k->addItem(SC_TR("Microphone"), "microphone");
            k->addItem(SC_TR("Speaker"), "speaker");
            k->addItem(SC_TR("Amplifier"), "amplifier");
            k->setCurrentIndex(std::max(0, k->findData(kindFilter->currentData())));
            kl->addWidget(k);
            auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            kl->addWidget(bb);
            QObject::connect(bb, &QDialogButtonBox::accepted, &kindDialog, &QDialog::accept);
            QObject::connect(bb, &QDialogButtonBox::rejected, &kindDialog, &QDialog::reject);
            if (kindDialog.exec() != QDialog::Accepted)
                return;
            p.kind = k->currentData().toString();
            Editor editor(p, &dialog, save, preview);
            editor.dirty = true;
            editor.exec();
        } catch (const std::exception &e) {
            QMessageBox::warning(&dialog, SC_TR("Response import"), e.what());
        }
    });
    QObject::connect(create, &QPushButton::clicked, &dialog, [&] {
        Profile p;
        p.kind = "speaker";
        p.brand = SC_TR("Custom");
        p.family = SC_TR("My equipment");
        p.model = SC_TR("New profile");
        p.custom = true;
        p.conditions = SC_TR("User-created profile");
        p.filters.append({1000, 0, 1});
        QDialog select(&dialog);
        select.setWindowTitle(SC_TR("Create profile"));
        auto *l = new QVBoxLayout(&select);
        auto *k = new QComboBox;
        k->setObjectName("equipmentCreationType");
        k->addItem(SC_TR("Speaker"), "speaker");
        k->addItem(SC_TR("Microphone"), "microphone");
        k->addItem(SC_TR("Amplifier"), "amplifier");
        k->setCurrentIndex(std::max(0, k->findData(kindFilter->currentData())));
        l->addWidget(k);
        auto *b = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        l->addWidget(b);
        QObject::connect(b, &QDialogButtonBox::accepted, &select, &QDialog::accept);
        QObject::connect(b, &QDialogButtonBox::rejected, &select, &QDialog::reject);
        if (select.exec() != QDialog::Accepted)
            return;
        p.kind = k->currentData().toString();
        Editor editor(p, &dialog, save, preview);
        editor.dirty = true;
        editor.exec();
    });
    QObject::connect(estimate, &QPushButton::clicked, &dialog, [&] {
        if (list->currentIndex()<0 || list->currentData().toInt()<0) return;
        const Profile chain=profiles[list->currentData().toInt()];
        QDialog setup(&dialog);
        setup.setObjectName("estimateProfileDialog");
        setup.setWindowTitle(SC_TR("Estimate profile")); setup.setWindowModality(Qt::ApplicationModal);
        const auto screenSize=setup.screen()->availableGeometry();
        setup.resize(std::min(680,screenSize.width()-40),std::min(640,screenSize.height()-40));
        auto *outer=new QVBoxLayout(&setup);
        auto *scroll=new QScrollArea; scroll->setWidgetResizable(true);
        auto *content=new QWidget; auto *body=new QVBoxLayout(content);scroll->setWidget(content);outer->addWidget(scroll);
        auto *intro=new QLabel(SC_TR("Use an imported measurement of the complete chain. Select only measured component responses that were part of that chain. Correction EQ filters are never subtracted."));
        intro->setWordWrap(true); body->addWidget(intro);
        auto *measurement=new QLabel(SC_TR("Measured chain") + ": " + chain.brand + " / " + profileLabel(chain));
        measurement->setTextFormat(Qt::PlainText);measurement->setWordWrap(true);body->addWidget(measurement);
        auto *form=new QFormLayout;auto *kind=new ProfileCombo;
        kind->setObjectName("estimatedEquipmentKind");
        kind->addItem(SC_TR("Microphone"),"microphone");kind->addItem(SC_TR("Speaker"),"speaker");kind->addItem(SC_TR("Amplifier"),"amplifier");
        form->addRow(SC_TR("Type"),kind);
        auto *conditions=new QLineEdit;conditions->setMaxLength(1200);conditions->setObjectName("estimationConditions");
        form->addRow(SC_TR("Conditions"),conditions);body->addLayout(form);
        body->addWidget(new QLabel(SC_TR("Known component responses")));
        auto *filters=new QHBoxLayout;auto *componentKind=new ProfileCombo;auto *componentBrand=new ProfileCombo;
        componentKind->setObjectName("estimationKindFilter");componentBrand->setObjectName("estimationBrandFilter");
        componentKind->addItem(SC_TR("All equipment"),"");componentKind->addItem(SC_TR("Microphone"),"microphone");
        componentKind->addItem(SC_TR("Speaker"),"speaker");componentKind->addItem(SC_TR("Amplifier"),"amplifier");
        componentBrand->addItem(SC_TR("All manufacturers"),"");QStringList brands;
        for(const auto &p:profiles)if(p.response.size()>=6 && !brands.contains(p.brand))brands.append(p.brand);
        brands.sort(Qt::CaseInsensitive);for(const auto &brand:brands)componentBrand->addItem(brand,brand);
        filters->addWidget(componentKind);filters->addWidget(componentBrand,1);body->addLayout(filters);
        auto *search=new QLineEdit;search->setObjectName("estimationSearch");search->setPlaceholderText(SC_TR("Search brand, family, model or measurement conditions"));body->addWidget(search);
        auto *components=new QListWidget;components->setObjectName("estimationComponents");components->setMinimumHeight(180);
        for(int i=0;i<profiles.size();++i) {
            if(i==list->currentData().toInt() || profiles[i].response.size()<6)continue;
            const auto &p=profiles[i];
            auto *item=new QListWidgetItem(p.brand + " / " + profileLabel(p),components);
            item->setData(Qt::UserRole,i);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Unchecked);
            item->setToolTip(p.kind + "\n" + p.source + "\n" + p.conditions);
        }
        body->addWidget(components);
        auto filterComponents=[&]{for(int i=0;i<components->count();++i){auto *item=components->item(i);const auto &p=profiles[item->data(Qt::UserRole).toInt()];
            item->setHidden((!componentKind->currentData().toString().isEmpty() && p.kind!=componentKind->currentData().toString()) ||
                (!componentBrand->currentData().toString().isEmpty() && p.brand!=componentBrand->currentData().toString()) ||
                !(p.brand+" "+p.family+" "+p.model+" "+p.conditions).contains(search->text(),Qt::CaseInsensitive));}};
        QObject::connect(componentKind,&QComboBox::currentIndexChanged,&setup,filterComponents);
        QObject::connect(componentBrand,&QComboBox::currentIndexChanged,&setup,filterComponents);
        QObject::connect(search,&QLineEdit::textChanged,&setup,filterComponents);
        auto *checkedSummary=new QLabel;checkedSummary->setObjectName("estimationSelectedComponents");checkedSummary->setWordWrap(true);checkedSummary->setTextFormat(Qt::PlainText);body->addWidget(checkedSummary);
        auto *ack=new QCheckBox;
        const QString acknowledgment=SC_TR("I understand that unknown room, microphone, clothing and placement effects remain in this estimate.");
        ack->setAccessibleName(acknowledgment);ack->setObjectName("estimationAcknowledged");
        auto *ackText=new QLabel(acknowledgment);ackText->setWordWrap(true);ackText->setBuddy(ack);
        auto *ackRow=new QHBoxLayout;ackRow->addWidget(ack);ackRow->addWidget(ackText,1);body->addLayout(ackRow);
        auto *feedback=new QLabel;feedback->setObjectName("estimationFeedback");feedback->setWordWrap(true);feedback->setTextFormat(Qt::PlainText);body->addWidget(feedback);
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Cancel);auto *review=buttons->addButton(SC_TR("Review estimated profile"),QDialogButtonBox::AcceptRole);
        review->setObjectName("reviewEstimatedProfile");review->setEnabled(false);outer->addWidget(buttons);
        review->setToolTip(SC_TR("Open the estimated response and correction in the editor. Nothing is saved or applied yet."));
        auto enable=[&]{QStringList names;for(int i=0;i<components->count();++i)if(components->item(i)->checkState()==Qt::Checked)names.append(components->item(i)->text());
            checkedSummary->setText(SC_TR("Known component responses") + ": " + names.join("; "));
            review->setEnabled(ack->isChecked() && !conditions->text().trimmed().isEmpty() && !names.isEmpty() && names.size()<=8);};
        QObject::connect(ack,&QCheckBox::toggled,&setup,enable);QObject::connect(conditions,&QLineEdit::textChanged,&setup,enable);
        QObject::connect(components,&QListWidget::itemChanged,&setup,enable);
        std::optional<Profile> proposal;
        QObject::connect(buttons,&QDialogButtonBox::rejected,&setup,&QDialog::reject);
        QObject::connect(review,&QPushButton::clicked,&setup,[&]{
            try {QVector<Profile> known;for(int i=0;i<components->count();++i)if(components->item(i)->checkState()==Qt::Checked)known.append(profiles[components->item(i)->data(Qt::UserRole).toInt()]);
                proposal=estimateProfile(chain,known,kind->currentData().toString(),conditions->text());setup.accept();
            }catch(const std::exception &e){feedback->setText(QString::fromUtf8(e.what()));}
        });
        if(setup.exec()!=QDialog::Accepted || !proposal)return;
        Editor editor(*proposal,&dialog,save,preview);editor.dirty=true;editor.exec();
    });
    QObject::connect(edit, &QPushButton::clicked, &dialog, [&] {
        if (list->currentIndex() < 0 || list->currentData().toInt() < 0)
            return;
        Editor editor(profiles[list->currentData().toInt()], &dialog, save, preview);
        editor.exec();
    });
    QObject::connect(exportButton, &QPushButton::clicked, &dialog, [&] {
        if (list->currentIndex() < 0 || list->currentData().toInt() < 0)
            return;
        const auto path =
            soundcurrent::i18n::FileDialogs::getSaveFileName(&dialog, SC_TR("Export profile"), {}, SC_TR("Equipment profile (*.json)"));
        if (path.isEmpty())
            return;
        QSaveFile f(path);
        const auto bytes = QJsonDocument(serialize(profiles[list->currentData().toInt()])).toJson();
        if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
            QMessageBox::warning(&dialog, SC_TR("Export"), SC_TR("Cannot save profile."));
    });
    appliedProfile->setObjectName("appliedEquipmentProfile");
    appliedProfile->setWordWrap(true);
    appliedProfile->setTextFormat(Qt::PlainText);
    outer->addWidget(appliedProfile); // Keep application feedback visible while the body scrolls.
    auto selectProfile = [&] {
        if (refreshing || list->currentIndex() < 0) return;
        const int index = list->currentData().toInt();
        if (index < 0) {
            Profile cleared; cleared.kind = kindFilter->currentData().toString();
            apply(cleared); activeSelection.reset(); noneWasApplied = true;
            appliedProfile->setText(QString::fromUtf8("✓ ") + SC_TR("None — use my own EQ"));
        } else {
            apply(profiles[index]); activeSelection = profiles[index]; noneWasApplied = false; lastAppliedId = profiles[index].id;
            appliedProfile->setText(QString::fromUtf8("✓ ") + profiles[index].brand + " / " + profileLabel(profiles[index]));
        }
    };
    QObject::connect(list, &QComboBox::currentIndexChanged, &dialog, selectProfile);
    // Choosing the initial None row is meaningful even when its index did not change.
    QObject::connect(list, &QComboBox::activated, &dialog, [&](int index) {
        if ((index == 0 && !noneWasApplied) ||
            (index > 0 && profiles[list->itemData(index).toInt()].id != lastAppliedId)) selectProfile();
    });
    QObject::connect(use, &QPushButton::clicked, &dialog, selectProfile);
    auto *selectionHint = new QLabel(SC_TR("Profiles apply immediately when selected."));
    selectionHint->setWordWrap(true);
    layout->insertWidget(1, selectionHint);
    auto *sources = new QLabel(
        SC_TR("Published measurement sources: <a href=\"https://www.spinorama.org/\">Speaker measurements / EQ</a> "
        "· <a href=\"https://support.daytonaudio.com/microphonecalibrationtool\">Dayton serial "
        "calibration</a> · <a href=\"https://www.minidsp.com/products/acoustic-measurement/umik-1\">miniDSP "
        "serial calibration</a> · <a href=\"https://www.neumann.com/de-de/downloads/\">Neumann microphone "
        "graphs</a> · <a href=\"https://docs.audio-technica.com/us/at2020_english.pdf\">AT2020 response "
        "graph</a> · <a "
        "href=\"https://www.soundstagenetwork.com/"
        "index.php?Itemid=154&amp;id=97&amp;option=com_content&amp;view=category\">Amplifier "
        "measurements</a>"));
    sources->setWordWrap(true);
    sources->setOpenExternalLinks(true);
    layout->addWidget(sources);
    // Fit translated button labels using this platform's actual font metrics.
    int actionWidth = 1;
    for (auto *action : actions) actionWidth = std::max(actionWidth, action->sizeHint().width());
    const int columns = std::clamp((dialog.width() - 64) / (actionWidth + std::max(6, row->spacing())), 1, 3);
    for (int i = 0; i < actions.size(); ++i) row->addWidget(actions[i], i / columns, i % columns);
    import->setToolTip(SC_TR("Add a JSON profile to your saved library."));
    text->setToolTip(SC_TR("Turn measured response data into a correction curve."));
    create->setToolTip(SC_TR("Create a new correction profile."));
    edit->setToolTip(SC_TR("Edit a copy; keep the original unchanged."));
    exportButton->setToolTip(SC_TR("Export the selected profile to a JSON file."));
    use->setToolTip(SC_TR("Use the selected profile for this equipment."));
    remove->setToolTip(SC_TR("Delete this saved custom profile."));
    auto *primary = new QHBoxLayout;
    primary->addWidget(use); primary->addStretch();
    outer->addLayout(primary);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close);
    primary->addWidget(close);
    close->button(QDialogButtonBox::Close)->setToolTip(SC_TR("Close the dialog; keep the applied correction."));
    QObject::connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    refresh();
    if (initialProfile && !initialProfile->id.isEmpty()) {
        refreshing = true;
        for (int row = 1; row < list->count(); ++row)
            if (profiles[list->itemData(row).toInt()].id == initialProfile->id && serialize(profiles[list->itemData(row).toInt()]) == serialize(*initialProfile)) {
                list->setCurrentIndex(row);
                lastAppliedId = initialProfile->id;
                break;
            }
        refreshing = false;
    }
    updateRemove();
    updateSelectionActions();
    dialog.exec();
}
} // namespace soundcurrent::equipment
