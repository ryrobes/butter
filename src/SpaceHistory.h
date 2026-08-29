#pragma once

#include <QDateTime>
#include <QString>
#include <QVariantList>

namespace SpaceHistory {

struct Capacity {
  bool valid = false;
  qulonglong totalBytes = 0;
  qulonglong freeBytes = 0;
};

QString historyPath();
QVariantList read(const QString &filePath = {});
bool record(qulonglong freeBytes, qulonglong totalBytes, const QString &source,
            const QString &filePath = {},
            const QDateTime &observedAt = QDateTime::currentDateTimeUtc());
Capacity measure(const QString &path);

} // namespace SpaceHistory
