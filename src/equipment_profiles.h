// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "dsp.h"
#include <QDialog>
#include <QJsonObject>
#include <QVector>
#include <functional>
#include <optional>
namespace soundcurrent::equipment {
struct Point {
    double frequency, db;
};
struct Profile {
    QString id, kind, brand, family, model, source, conditions, provenance;
    QString equipmentType = "Unclassified", powerType = "Unknown";
    QVector<EqBand> filters;
    QVector<Point> response;
    bool custom = false;
    bool estimated = false;
    QJsonObject estimation; // Immutable derivation provenance, distinct from editable correction filters.
};
QJsonObject serialize(const Profile &);
Profile parse(const QByteArray &); // throws with a user-readable reason; bounded JSON or response text
QString profileLabel(const Profile &);
Profile estimateProfile(const Profile &chain, const QVector<Profile> &known,
                        const QString &targetKind, const QString &conditions);
QVector<EqBand> fitResponse(const QVector<Point> &);
QString libraryPath();
QVector<Profile> loadLibrary();
void saveLibrary(const QVector<Profile> &);
QVector<Profile> bundledProfiles();
Profile importProfile(Profile); // Reuse matching content; never overwrite an existing identity.
using Removed = std::function<bool(const Profile &)>; // true when active correction was cleared.
// Temporary audition only: nullopt restores the baseline; false reports unavailable/failed preview.
// Callbacks must not persist selection and must handle restoration without throwing.
using Preview = std::function<bool(const std::optional<Profile> &)>;
void saveNewProfile(QWidget *, Profile, const Preview &preview = {});
// Selection callbacks apply immediately; an empty ID clears kind (empty kind clears all).
// Opening/filtering/importing/saving alone never invokes the selection callback.
void openLibrary(QWidget *, const std::function<void(const Profile &)> &apply,
                 const QString &initialKind = {}, const Preview &preview = {},
                 const std::optional<Profile> &initialProfile = {}, const Removed &removed = {});
} // namespace soundcurrent::equipment
