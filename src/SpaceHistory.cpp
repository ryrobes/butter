#include "SpaceHistory.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>

#include <algorithm>
#include <utility>

namespace {

struct StoredSample {
  QDateTime observedAt;
  qulonglong freeBytes = 0;
  qulonglong totalBytes = 0;
  QString source;
};

QString resolvedPath(const QString &filePath) {
  return filePath.isEmpty() ? SpaceHistory::historyPath() : filePath;
}

QList<StoredSample> loadSamples(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  if (!document.isObject() ||
      document.object().value(QStringLiteral("schema")).toInt() != 1)
    return {};

  QList<StoredSample> samples;
  for (const QJsonValue &value :
       document.object().value(QStringLiteral("samples")).toArray()) {
    const QJsonObject object = value.toObject();
    StoredSample sample;
    sample.observedAt = QDateTime::fromString(
        object.value(QStringLiteral("at")).toString(), Qt::ISODate);
    sample.freeBytes =
        object.value(QStringLiteral("freeBytes")).toVariant().toULongLong();
    sample.totalBytes =
        object.value(QStringLiteral("totalBytes")).toVariant().toULongLong();
    sample.source = object.value(QStringLiteral("source")).toString();
    if (sample.observedAt.isValid() && sample.totalBytes > 0 &&
        sample.freeBytes <= sample.totalBytes)
      samples.push_back(sample);
  }
  std::sort(samples.begin(), samples.end(),
            [](const StoredSample &a, const StoredSample &b) {
              return a.observedAt < b.observedAt;
            });
  return samples;
}

QJsonObject toJson(const StoredSample &sample) {
  return {
      {QStringLiteral("at"), sample.observedAt.toString(Qt::ISODate)},
      {QStringLiteral("freeBytes"), static_cast<qint64>(sample.freeBytes)},
      {QStringLiteral("totalBytes"), static_cast<qint64>(sample.totalBytes)},
      {QStringLiteral("source"), sample.source}};
}

qulonglong captureNumber(const QString &text, const QString &label) {
  const QRegularExpression expression(
      QStringLiteral("^\\s*%1:\\s*(\\d+)")
          .arg(QRegularExpression::escape(label)),
      QRegularExpression::MultilineOption);
  const QRegularExpressionMatch match = expression.match(text);
  return match.hasMatch() ? match.captured(1).toULongLong() : 0;
}

} // namespace

namespace SpaceHistory {

QString historyPath() {
  return QDir(QStandardPaths::writableLocation(
                  QStandardPaths::GenericStateLocation))
      .filePath(QStringLiteral("butter/space-history.json"));
}

QVariantList read(const QString &filePath) {
  QVariantList result;
  for (const StoredSample &sample : loadSamples(resolvedPath(filePath))) {
    QVariantMap item;
    item.insert(QStringLiteral("atMs"), sample.observedAt.toMSecsSinceEpoch());
    item.insert(QStringLiteral("freeBytes"), sample.freeBytes);
    item.insert(QStringLiteral("totalBytes"), sample.totalBytes);
    item.insert(QStringLiteral("freeMegabytes"),
                static_cast<qulonglong>(qRound64(
                    static_cast<double>(sample.freeBytes) / 1'000'000.0)));
    item.insert(QStringLiteral("freeRatio"),
                static_cast<double>(sample.freeBytes) /
                    static_cast<double>(sample.totalBytes));
    item.insert(QStringLiteral("source"), sample.source);
    result.push_back(item);
  }
  return result;
}

bool record(qulonglong freeBytes, qulonglong totalBytes, const QString &source,
            const QString &filePath, const QDateTime &observedAt) {
  if (!observedAt.isValid() || totalBytes == 0 || freeBytes > totalBytes)
    return false;
  const QString path = resolvedPath(filePath);
  if (!QDir().mkpath(QFileInfo(path).dir().absolutePath()))
    return false;

  QLockFile lock(path + QStringLiteral(".lock"));
  lock.setStaleLockTime(30000);
  if (!lock.tryLock(2000))
    return false;

  QList<StoredSample> samples = loadSamples(path);
  const QDateTime cutoff = observedAt.addDays(-370);
  samples.erase(std::remove_if(samples.begin(), samples.end(),
                               [&cutoff](const StoredSample &sample) {
                                 return sample.observedAt < cutoff;
                               }),
                samples.end());

  StoredSample sample{observedAt.toUTC(), freeBytes, totalBytes,
                      source.left(24)};
  constexpr qint64 coalesceWindowMs = 15 * 60 * 1000;
  if (!samples.isEmpty() && qAbs(samples.constLast().observedAt.msecsTo(
                                sample.observedAt)) <= coalesceWindowMs)
    samples.last() = sample;
  else
    samples.push_back(sample);

  std::sort(samples.begin(), samples.end(),
            [](const StoredSample &a, const StoredSample &b) {
              return a.observedAt < b.observedAt;
            });
  constexpr qsizetype maximumSamples = 800;
  if (samples.size() > maximumSamples)
    samples = samples.sliced(samples.size() - maximumSamples);

  QJsonArray jsonSamples;
  for (const StoredSample &stored : std::as_const(samples))
    jsonSamples.push_back(toJson(stored));
  const QJsonObject root = {{QStringLiteral("schema"), 1},
                            {QStringLiteral("samples"), jsonSamples}};
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 ||
      !file.commit())
    return false;
  QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  return true;
}

Capacity measure(const QString &path) {
  QStorageInfo storage(path);
  storage.refresh();
  Capacity result;
  if (!storage.isValid() || !storage.isReady() || storage.bytesTotal() <= 0 ||
      storage.bytesAvailable() < 0 ||
      storage.bytesAvailable() > storage.bytesTotal())
    return result;
  result.valid = true;
  result.totalBytes = static_cast<qulonglong>(storage.bytesTotal());
  result.freeBytes = static_cast<qulonglong>(storage.bytesAvailable());

  if (storage.fileSystemType() != QByteArrayLiteral("btrfs"))
    return result;
  const QString btrfs = QStandardPaths::findExecutable(QStringLiteral("btrfs"));
  if (btrfs.isEmpty())
    return result;
  QProcess process;
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  process.setProcessEnvironment(environment);
  process.start(btrfs, {QStringLiteral("filesystem"), QStringLiteral("usage"),
                        QStringLiteral("-b"), path});
  if (!process.waitForFinished(10000) || process.exitCode() != 0)
    return result;
  const QString output = QString::fromUtf8(process.readAllStandardOutput());
  const qulonglong total = captureNumber(output, QStringLiteral("Device size"));
  const qulonglong free =
      captureNumber(output, QStringLiteral("Free (estimated)"));
  if (total > 0 && free <= total) {
    result.totalBytes = total;
    result.freeBytes = free;
  }
  return result;
}

} // namespace SpaceHistory
