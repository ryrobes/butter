#include "ShadowCommon.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace ShadowCommon {

QString configPath() {
  return QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
      .filePath(QStringLiteral("butter/home-shadow.json"));
}

QString statusPath() {
  return QDir(QStandardPaths::writableLocation(
                  QStandardPaths::GenericStateLocation))
      .filePath(QStringLiteral("butter/home-shadow-status.json"));
}

QString legacyStatusPath() {
  return QDir(QStandardPaths::writableLocation(
                  QStandardPaths::GenericStateLocation))
      .filePath(QStringLiteral("butter-shadow/butter/home-shadow-status.json"));
}

QString unitName() { return QStringLiteral("butter-home-shadow.service"); }

QString timerName() { return QStringLiteral("butter-home-shadow.timer"); }

QString machineId() {
  QFile file(QStringLiteral("/etc/machine-id"));
  return file.open(QIODevice::ReadOnly)
             ? QString::fromUtf8(file.readAll()).trimmed()
             : QString();
}

QString normalized(const QString &path) {
  const QString canonical = QFileInfo(path).canonicalFilePath();
  return canonical.isEmpty() ? QDir::cleanPath(path) : canonical;
}

bool isInside(const QString &path, const QString &parent) {
  return path == parent || path.startsWith(parent + QLatin1Char('/'));
}

MountInfo mountInfo(const QString &path) {
  QProcess process;
  process.setProgram(QStringLiteral("findmnt"));
  process.setArguments({QStringLiteral("--json"), QStringLiteral("--output"),
                        QStringLiteral("UUID,FSTYPE,SOURCE,TARGET"),
                        QStringLiteral("--target"), path});
  process.start();
  if (!process.waitForFinished(5000) || process.exitCode() != 0)
    return {};

  const QJsonDocument document =
      QJsonDocument::fromJson(process.readAllStandardOutput());
  const QJsonArray filesystems =
      document.object().value(QStringLiteral("filesystems")).toArray();
  if (filesystems.isEmpty())
    return {};
  const QJsonObject item = filesystems.first().toObject();
  MountInfo info;
  info.uuid = item.value(QStringLiteral("uuid")).toString();
  info.filesystem = item.value(QStringLiteral("fstype")).toString();
  info.source = item.value(QStringLiteral("source")).toString();
  info.target = item.value(QStringLiteral("target")).toString();
  info.valid = !info.source.isEmpty() && !info.target.isEmpty();
  return info;
}

QString backingDevice(const QString &mountSource) {
  QString device = mountSource.section(QLatin1Char('['), 0, 0);
  if (!device.startsWith(QLatin1String("/dev/")))
    return device;
  QProcess process;
  process.start(QStringLiteral("lsblk"),
                {QStringLiteral("--inverse"), QStringLiteral("--paths"),
                 QStringLiteral("--noheadings"), QStringLiteral("--output"),
                 QStringLiteral("NAME,TYPE"), device});
  if (!process.waitForFinished(5000) || process.exitCode() != 0)
    return device;
  const QStringList lines = QString::fromUtf8(process.readAllStandardOutput())
                                .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    if (!line.trimmed().endsWith(QLatin1String(" disk")))
      continue;
    const int start = line.indexOf(QLatin1String("/dev/"));
    if (start < 0)
      continue;
    return line.mid(start).section(QLatin1Char(' '), 0, 0).trimmed();
  }
  return device;
}

QStringList baselineExcludes() {
  return {QStringLiteral(".cache"),
          QStringLiteral(".local/share/Trash/files"),
          QStringLiteral(".npm/_cacache"),
          QStringLiteral(".npm/_npx"),
          QStringLiteral(".cargo/registry"),
          QStringLiteral(".cargo/git"),
          QStringLiteral(".local/share/pnpm/store"),
          QStringLiteral("go/pkg/mod/cache"),
          QStringLiteral("**/__pycache__"),
          QStringLiteral("**/.pytest_cache"),
          QStringLiteral("**/.mypy_cache"),
          QStringLiteral("**/.ruff_cache"),
          QStringLiteral("**/.parcel-cache"),
          QStringLiteral("**/.vite")};
}

QStringList excludesFromFindings(const QVariantList &findings,
                                 const QString &sourcePath) {
  QStringList excludes = baselineExcludes();
  const QDir source(sourcePath);
  for (const QVariant &findingValue : findings) {
    const QVariantMap finding = findingValue.toMap();
    if (finding.value(QStringLiteral("safety")).toString() !=
        QLatin1String("regenerable"))
      continue;
    const QString path =
        normalized(finding.value(QStringLiteral("path")).toString());
    if (!isInside(path, sourcePath) || path == sourcePath)
      continue;
    const QString relative = QDir::cleanPath(source.relativeFilePath(path));
    if (!relative.isEmpty() && relative != QLatin1String(".") &&
        !relative.startsWith(QLatin1String("../")))
      excludes.push_back(relative);
  }
  excludes.removeDuplicates();
  std::sort(excludes.begin(), excludes.end());
  return excludes;
}

QJsonObject toJson(const Config &config) {
  QJsonArray excludes;
  for (const QString &exclude : config.excludes)
    excludes.push_back(exclude);
  return {{QStringLiteral("schema"), config.schema},
          {QStringLiteral("sourcePath"), config.sourcePath},
          {QStringLiteral("destinationPath"), config.destinationPath},
          {QStringLiteral("mountUuid"), config.mountUuid},
          {QStringLiteral("mountSource"), config.mountSource},
          {QStringLiteral("filesystem"), config.filesystem},
          {QStringLiteral("machineId"), config.machineId},
          {QStringLiteral("createdAt"), config.createdAt},
          {QStringLiteral("excludedBytes"),
           static_cast<qint64>(config.excludedBytes)},
          {QStringLiteral("excludes"), excludes}};
}

bool fromJson(const QJsonObject &object, Config *config, QString *error) {
  if (!config) {
    if (error)
      *error = QStringLiteral("No configuration target was supplied.");
    return false;
  }
  config->schema = object.value(QStringLiteral("schema")).toInt();
  config->sourcePath =
      normalized(object.value(QStringLiteral("sourcePath")).toString());
  config->destinationPath =
      normalized(object.value(QStringLiteral("destinationPath")).toString());
  config->mountUuid = object.value(QStringLiteral("mountUuid")).toString();
  config->mountSource = object.value(QStringLiteral("mountSource")).toString();
  config->filesystem = object.value(QStringLiteral("filesystem")).toString();
  config->machineId = object.value(QStringLiteral("machineId")).toString();
  config->createdAt = object.value(QStringLiteral("createdAt")).toString();
  config->excludedBytes = static_cast<qulonglong>(
      object.value(QStringLiteral("excludedBytes")).toInteger());
  config->excludes.clear();
  for (const QJsonValue &value :
       object.value(QStringLiteral("excludes")).toArray())
    config->excludes.push_back(value.toString());

  if (config->schema != 1 || config->sourcePath.isEmpty() ||
      config->destinationPath.isEmpty() || config->machineId.isEmpty()) {
    if (error)
      *error = QStringLiteral("The Home Shadow configuration is incomplete.");
    return false;
  }
  return true;
}

bool loadConfig(const QString &path, Config *config, QString *error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    if (error)
      *error = file.errorString();
    return false;
  }
  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    if (error)
      *error =
          QStringLiteral("The Home Shadow configuration is not valid JSON.");
    return false;
  }
  return fromJson(document.object(), config, error);
}

bool saveJson(const QString &path, const QJsonObject &object, QString *error) {
  if (!QDir().mkpath(QFileInfo(path).dir().absolutePath())) {
    if (error)
      *error =
          QStringLiteral("Butter could not create the configuration folder.");
    return false;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0 ||
      !file.commit()) {
    if (error)
      *error = file.errorString();
    return false;
  }
  QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
  return true;
}

bool saveStatus(const QString &path, const QString &state, const QString &title,
                const QString &detail, const QJsonObject &extra) {
  QJsonObject object = extra;
  object.insert(QStringLiteral("schema"), 1);
  object.insert(QStringLiteral("state"), state);
  object.insert(QStringLiteral("title"), title);
  object.insert(QStringLiteral("detail"), detail);
  object.insert(QStringLiteral("updatedAt"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
  return saveJson(path, object, nullptr);
}

QString systemdQuote(const QString &value) {
  QString escaped = value;
  escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
  escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
  return QLatin1Char('"') + escaped + QLatin1Char('"');
}

} // namespace ShadowCommon
