// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QTextDocument>
namespace soundcurrent::ui {
// Help must never fetch images or files while converting existing rich labels.
class HelpTextDocument final : public QTextDocument {
protected:
    QVariant loadResource(int, const QUrl &) override { return {}; }
};
inline void describeButton(QAbstractButton *button) {
    if (!button) return;
    if (button->property("soundcurrentHelpReady").toBool() &&
        button->property("soundcurrentHelpCaption").toString() == button->text() &&
        button->property("soundcurrentHelpTip").toString() == button->toolTip() &&
        !button->accessibleDescription().isEmpty() && !button->whatsThis().isEmpty()) return;
    const bool generated = button->property("soundcurrentGeneratedHelp").toBool();
    const bool keepGenerated = generated && button->toolTip() == button->property("soundcurrentLastHelp").toString();
    QString description = keepGenerated ? QString() : button->toolTip();
    if (description.isEmpty() && !generated) description = button->accessibleDescription();
    if (description.isEmpty() && !generated) description = button->whatsThis();
    const bool contextual = description.isEmpty();
    if (contextual) {
        QString caption = button->accessibleName().isEmpty() ? button->text() : button->accessibleName();
        caption.remove('&');
        if (caption.isEmpty()) return; // Leave Qt's internal, unlabeled controls alone.
        QWidget *context = button->parentWidget();
        while (context && !qobject_cast<QGroupBox *>(context) && !qobject_cast<QDialog *>(context)) context = context->parentWidget();
        description = caption;
        if (auto *group = qobject_cast<QGroupBox *>(context)) description += "\n" + group->title();
        if (context) for (auto *label : context->findChildren<QLabel *>()) {
            const auto text = label->text();
            if (label->wordWrap() && text.size() >= 80 && text.size() <= 500) {
                QString contextText = text;
                if (label->textFormat() == Qt::RichText ||
                    (label->textFormat() == Qt::AutoText && Qt::mightBeRichText(text))) {
                    HelpTextDocument plain; plain.setHtml(text); contextText = plain.toPlainText();
                }
                description += "\n\n" + contextText; break;
            }
        }
    }
    QString accessible = description;
    if (!contextual && Qt::mightBeRichText(description)) {
        HelpTextDocument plain; plain.setHtml(description); accessible = plain.toPlainText();
    }
    if (contextual) {
        button->setProperty("soundcurrentGeneratedHelp", true);
        const auto tip = Qt::convertFromPlainText(description);
        button->setProperty("soundcurrentLastHelp", tip); button->setToolTip(tip);
    }
    if (generated || button->accessibleDescription().isEmpty()) button->setAccessibleDescription(accessible);
    if (generated || button->whatsThis().isEmpty()) button->setWhatsThis(button->toolTip());
    button->setProperty("soundcurrentHelpReady", true);
    button->setProperty("soundcurrentHelpCaption", button->text());
    button->setProperty("soundcurrentHelpTip", button->toolTip());
    if (!button->property("soundcurrentHelpConnected").toBool()) {
        button->setProperty("soundcurrentHelpConnected", true);
        QObject::connect(button, &QAbstractButton::clicked, button, [button] { describeButton(button); });
    }
}
class ButtonHelp final : public QObject {
public:
    explicit ButtonHelp(QObject *parent) : QObject(parent) {}
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::Show || event->type() == QEvent::ToolTip || event->type() == QEvent::FocusIn || event->type() == QEvent::Enter)
            describeButton(qobject_cast<QAbstractButton *>(object));
        return false;
    }
};
inline void installButtonHelp() {
    if (!qApp || qApp->property("soundcurrentButtonHelpInstalled").toBool()) return;
    qApp->setProperty("soundcurrentButtonHelpInstalled", true);
    qApp->installEventFilter(new ButtonHelp(qApp));
}
}
