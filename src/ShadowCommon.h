#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace ShadowCommon {

struct MountInfo {
  bool valid = false;
  QString uuid;
  QString filesystem;
  QString source;
  QString target;
};

struct Config {
  int schema = 1;
  QString sourcePath;
  QString destinationPath;
  QString mountUuid;
  QString mountSource;
  QString filesystem;
  QString machineId;
  QString createdAt;
  QStringList excludes;
  qulonglong excludedBytes = 0;
};

QString configPath();
QString statusPath();
QString legacyStatusPath();
QString unitName();
QString timerName();
QString machineId();
QString normalized(const QString &path);
bool isInside(const QString &path, const QString &parent);
MountInfo mountInfo(const QString &path);
QString backingDevice(const QString &mountSource);
QStringList baselineExcludes();
QStringList excludesFromFindings(const QVariantList &findings,
                                 const QString &sourcePath);
QJsonObject toJson(const Config &config);
bool fromJson(const QJsonObject &object, Config *config, QString *error);
bool loadConfig(const QString &path, Config *config, QString *error);
bool saveJson(const QString &path, const QJsonObject &object, QString *error);
bool saveStatus(const QString &path, const QString &state, const QString &title,
                const QString &detail, const QJsonObject &extra = {});
QString systemdQuote(const QString &value);

} // namespace ShadowCommon
