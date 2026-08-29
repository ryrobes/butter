#include "HomeShadow.h"

#include "ShadowCommon.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <sys/stat.h>
#include <unistd.h>

namespace {

bool unsupportedFilesystem(const QString &filesystem) {
  const QString lower = filesystem.toLower();
  return lower.contains(QStringLiteral("fat")) ||
         lower.contains(QStringLiteral("ntfs")) ||
         lower.contains(QStringLiteral("fuseblk"));
}

bool differentDevice(const QString &source, const QString &destination) {
  struct stat sourceStat{};
  struct stat destinationStat{};
  return ::stat(QFile::encodeName(source).constData(), &sourceStat) == 0 &&
         ::stat(QFile::encodeName(destination).constData(), &destinationStat) ==
             0 &&
         sourceStat.st_dev != destinationStat.st_dev;
}

bool supportsHardLinks(const QString &directory) {
  QTemporaryFile original(
      QDir(directory).filePath(QStringLiteral(".butter-link-test-XXXXXX")));
  if (!original.open())
    return false;
  const QString linkPath = original.fileName() + QStringLiteral(".link");
  const bool linked = ::link(QFile::encodeName(original.fileName()).constData(),
                             QFile::encodeName(linkPath).constData()) == 0;
  if (linked)
    QFile::remove(linkPath);
  return linked;
}

bool timerEnabled() {
  QProcess process;
  process.start(QStringLiteral("systemctl"),
                {QStringLiteral("--user"), QStringLiteral("is-enabled"),
                 ShadowCommon::timerName()});
  return process.waitForFinished(5000) && process.exitCode() == 0 &&
         QString::fromUtf8(process.readAllStandardOutput()).trimmed() ==
             QLatin1String("enabled");
}

QString friendlyTime(const QString &iso) {
  const QDateTime time = QDateTime::fromString(iso, Qt::ISODate);
  return time.isValid()
             ? time.toLocalTime().toString(QStringLiteral("MMM d · h:mm AP"))
             : QString();
}

qulonglong regenerableBytes(const QVariantList &findings) {
  qulonglong bytes = 0;
  for (const QVariant &findingValue : findings) {
    const QVariantMap finding = findingValue.toMap();
    if (finding.value(QStringLiteral("safety")).toString() ==
        QLatin1String("regenerable"))
      bytes += finding.value(QStringLiteral("bytes")).toULongLong();
  }
  return bytes;
}

} // namespace

HomeShadow::HomeShadow(QObject *parent) : QObject(parent) {
  m_refreshTimer.setInterval(10000);
  connect(&m_refreshTimer, &QTimer::timeout, this, &HomeShadow::refresh);
  m_refreshTimer.start();
  refresh();
}

QVariantMap HomeShadow::inspectDestination(const QUrl &folder) const {
  QVariantMap result;
  const QString destination = ShadowCommon::normalized(folder.toLocalFile());
  const QString source = ShadowCommon::normalized(QDir::homePath());
  const QFileInfo info(destination);
  if (!folder.isLocalFile() || !info.exists() || !info.isDir() ||
      !info.isWritable()) {
    result.insert(QStringLiteral("valid"), false);
    result.insert(QStringLiteral("title"),
                  QStringLiteral("Choose a writable folder"));
    result.insert(QStringLiteral("detail"),
                  QStringLiteral("Butter needs an existing local folder on "
                                 "your second drive."));
    return result;
  }
  if (ShadowCommon::isInside(destination, source) ||
      !differentDevice(source, destination)) {
    result.insert(QStringLiteral("valid"), false);
    result.insert(QStringLiteral("title"),
                  QStringLiteral("That is still the Home drive"));
    result.insert(QStringLiteral("detail"),
                  QStringLiteral("A Home Shadow must live on another physical "
                                 "filesystem so it survives this drive."));
    return result;
  }

  const ShadowCommon::MountInfo mount = ShadowCommon::mountInfo(destination);
  const ShadowCommon::MountInfo sourceMount = ShadowCommon::mountInfo(source);
  if (!mount.valid || mount.uuid.isEmpty()) {
    result.insert(QStringLiteral("valid"), false);
    result.insert(QStringLiteral("title"),
                  QStringLiteral("Butter cannot identify this drive"));
    result.insert(QStringLiteral("detail"),
                  QStringLiteral("The destination needs a stable filesystem "
                                 "UUID before Butter can schedule safe runs."));
    return result;
  }
  if (sourceMount.valid && ShadowCommon::backingDevice(sourceMount.source) ==
                               ShadowCommon::backingDevice(mount.source)) {
    result.insert(QStringLiteral("valid"), false);
    result.insert(QStringLiteral("title"),
                  QStringLiteral("That is another partition on this drive"));
    result.insert(QStringLiteral("detail"),
                  QStringLiteral("Choose a filesystem backed by a different "
                                 "physical disk so one hardware failure cannot "
                                 "take both copies."));
    return result;
  }
  if (unsupportedFilesystem(mount.filesystem)) {
    result.insert(QStringLiteral("valid"), false);
    result.insert(QStringLiteral("title"),
                  QStringLiteral("This drive cannot preserve Linux files"));
    result.insert(QStringLiteral("detail"),
                  QStringLiteral("Use a Btrfs, ext4, or XFS destination. "
                                 "Windows-style filesystems lose permissions, "
                                 "links, and other Home metadata."));
    return result;
  }

  result.insert(QStringLiteral("valid"), true);
  result.insert(QStringLiteral("title"),
                QStringLiteral("Create a Home Shadow here?"));
  result.insert(QStringLiteral("detail"), destination);
  result.insert(QStringLiteral("path"), destination);
  result.insert(QStringLiteral("filesystem"), mount.filesystem);
  result.insert(QStringLiteral("drive"), mount.source);
  return result;
}

void HomeShadow::configure(const QUrl &folder, const QVariantList &findings) {
  const QVariantMap inspection = inspectDestination(folder);
  if (!inspection.value(QStringLiteral("valid")).toBool()) {
    setError(inspection.value(QStringLiteral("title")).toString(),
             inspection.value(QStringLiteral("detail")).toString());
    return;
  }

  const QString destination =
      inspection.value(QStringLiteral("path")).toString();
  const QString shadowRoot =
      QDir(destination).filePath(QStringLiteral(".butter-shadow"));
  if (!QDir().mkpath(
          QDir(shadowRoot).filePath(QStringLiteral("generations"))) ||
      !QDir().mkpath(QDir(shadowRoot).filePath(QStringLiteral("incomplete"))) ||
      !QDir().mkpath(QDir(shadowRoot).filePath(QStringLiteral("receipts"))) ||
      !supportsHardLinks(shadowRoot)) {
    setError(QStringLiteral("This folder cannot hold a Home Shadow"),
             QStringLiteral("Butter could not create and hard-link a test "
                            "file. Nothing was scheduled."));
    return;
  }
  QFile::setPermissions(shadowRoot, QFileDevice::ReadOwner |
                                        QFileDevice::WriteOwner |
                                        QFileDevice::ExeOwner);

  const ShadowCommon::MountInfo mount = ShadowCommon::mountInfo(destination);
  ShadowCommon::Config config;
  config.sourcePath = ShadowCommon::normalized(QDir::homePath());
  config.destinationPath = destination;
  config.mountUuid = mount.uuid;
  config.mountSource = mount.source;
  config.filesystem = mount.filesystem;
  config.machineId = ShadowCommon::machineId();
  config.createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  config.excludes =
      ShadowCommon::excludesFromFindings(findings, config.sourcePath);
  config.excludedBytes = regenerableBytes(findings);

  QString error;
  if (!ShadowCommon::saveJson(
          QDir(shadowRoot).filePath(QStringLiteral("identity.json")),
          ShadowCommon::toJson(config), &error) ||
      !ShadowCommon::saveJson(ShadowCommon::configPath(),
                              ShadowCommon::toJson(config), &error) ||
      !writeUnits(&error)) {
    setError(QStringLiteral("Home Shadow setup stopped"), error);
    return;
  }

  m_configured = true;
  m_state = QStringLiteral("ready");
  m_title = QStringLiteral("Ready for its first copy");
  m_detail = QStringLiteral(
      "Daily when connected · completed generations are never pruned.");
  emit changed();
  runNow();
}

bool HomeShadow::writeUnits(QString *error) {
  const QString helper = QDir(QCoreApplication::applicationDirPath())
                             .filePath(QStringLiteral("butter-shadow"));
  if (!QFileInfo::exists(helper)) {
    if (error)
      *error = QStringLiteral(
          "The Home Shadow helper is not installed beside Butter.");
    return false;
  }
  const QString unitDirectory =
      QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
          .filePath(QStringLiteral("systemd/user"));
  if (!QDir().mkpath(unitDirectory)) {
    if (error)
      *error =
          QStringLiteral("Butter could not create the user service folder.");
    return false;
  }

  const QString service =
      QStringLiteral("[Unit]\nDescription=Update Butter Home Shadow\n\n"
                     "[Service]\nType=oneshot\nExecStart=%1 --run\n"
                     "Nice=10\nIOSchedulingClass=idle\n")
          .arg(ShadowCommon::systemdQuote(helper));
  const QString timer = QStringLiteral(
      "[Unit]\nDescription=Update Butter Home Shadow daily\n\n"
      "[Timer]\nOnCalendar=daily\nPersistent=true\nRandomizedDelaySec=20m\n"
      "Unit=butter-home-shadow.service\n\n[Install]\nWantedBy=timers.target\n");

  const auto saveText = [&](const QString &path, const QString &text) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 ||
        !file.commit()) {
      if (error)
        *error = file.errorString();
      return false;
    }
    return true;
  };
  if (!saveText(QDir(unitDirectory).filePath(ShadowCommon::unitName()),
                service) ||
      !saveText(QDir(unitDirectory).filePath(ShadowCommon::timerName()), timer))
    return false;

  QProcess reload;
  reload.start(QStringLiteral("systemctl"),
               {QStringLiteral("--user"), QStringLiteral("daemon-reload")});
  if (!reload.waitForFinished(5000) || reload.exitCode() != 0) {
    if (error)
      *error = QString::fromUtf8(reload.readAllStandardError()).trimmed();
    return false;
  }
  QProcess enable;
  enable.start(QStringLiteral("systemctl"),
               {QStringLiteral("--user"), QStringLiteral("enable"),
                QStringLiteral("--now"), ShadowCommon::timerName()});
  if (!enable.waitForFinished(10000) || enable.exitCode() != 0) {
    if (error)
      *error = QString::fromUtf8(enable.readAllStandardError()).trimmed();
    return false;
  }
  return true;
}

void HomeShadow::runNow() {
  if (!m_configured || m_busy)
    return;
  if (!m_controlProcess) {
    m_controlProcess = new QProcess(this);
    connect(m_controlProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) {
              m_busy = false;
              refresh();
              refreshSoon();
            });
  }
  m_busy = true;
  m_state = QStringLiteral("running");
  m_title = QStringLiteral("Updating Home Shadow");
  m_detail = QStringLiteral(
      "Building a new generation without changing earlier copies.");
  emit changed();
  m_controlProcess->start(QStringLiteral("systemctl"),
                          {QStringLiteral("--user"), QStringLiteral("start"),
                           ShadowCommon::unitName()});
}

void HomeShadow::refresh() {
  ShadowCommon::Config config;
  QString error;
  if (!ShadowCommon::loadConfig(ShadowCommon::configPath(), &config, &error)) {
    m_configured = false;
    m_state = QStringLiteral("unconfigured");
    m_title = QStringLiteral("Your files still live on one drive");
    m_detail = QStringLiteral(
        "Create a quiet second copy of Home without rebuildable weight.");
    emit changed();
    return;
  }

  m_configured = true;
  m_destination = config.destinationPath;
  m_filesystem = config.filesystem;
  m_exclusionCount = config.excludes.size();
  m_excludedBytes = config.excludedBytes;
  const QString generationsPath =
      QDir(config.destinationPath)
          .filePath(QStringLiteral(".butter-shadow/generations"));
  m_generationCount =
      QDir(generationsPath)
          .entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)
          .size();

  const ShadowCommon::MountInfo currentMount =
      ShadowCommon::mountInfo(config.destinationPath);
  const bool connected =
      currentMount.valid && currentMount.uuid == config.mountUuid &&
      QFileInfo::exists(
          QDir(config.destinationPath)
              .filePath(QStringLiteral(".butter-shadow/identity.json")));
  if (!connected) {
    m_state = QStringLiteral("waiting");
    m_title = QStringLiteral("Waiting for the Shadow drive");
    m_detail = QStringLiteral("Connect the same drive; Butter will never write "
                              "into its empty mount path.");
    emit changed();
    return;
  }

  if (!timerEnabled()) {
    m_state = QStringLiteral("error");
    m_title = QStringLiteral("The daily schedule needs attention");
    m_detail = QStringLiteral("The Shadow is configured, but its user timer is "
                              "not enabled. Run now still works.");
    emit changed();
    return;
  }

  const QString currentStatusPath = ShadowCommon::statusPath();
  QFile statusFile(currentStatusPath);
  bool legacyStatus = false;
  if (!statusFile.open(QIODevice::ReadOnly)) {
    statusFile.setFileName(ShadowCommon::legacyStatusPath());
    legacyStatus = statusFile.open(QIODevice::ReadOnly);
  }
  if (statusFile.isOpen()) {
    const QJsonObject status =
        QJsonDocument::fromJson(statusFile.readAll()).object();
    if (legacyStatus)
      ShadowCommon::saveJson(currentStatusPath, status, nullptr);
    m_state = status.value(QStringLiteral("state")).toString();
    m_title = status.value(QStringLiteral("title")).toString();
    m_detail = status.value(QStringLiteral("detail")).toString();
    m_lastRun =
        friendlyTime(status.value(QStringLiteral("finishedAt")).toString());
    const bool controlRunning =
        m_controlProcess && m_controlProcess->state() != QProcess::NotRunning;
    m_busy = controlRunning || m_state == QLatin1String("running");
  } else {
    m_state = QStringLiteral("ready");
    m_title = QStringLiteral("Ready for its first copy");
    m_detail = QStringLiteral(
        "Daily when connected · completed generations are never pruned.");
    m_busy = false;
  }
  emit changed();
}

void HomeShadow::updateExclusions(const QVariantList &findings) {
  ShadowCommon::Config config;
  QString error;
  if (!ShadowCommon::loadConfig(ShadowCommon::configPath(), &config, &error))
    return;
  config.excludes =
      ShadowCommon::excludesFromFindings(findings, config.sourcePath);
  config.excludedBytes = regenerableBytes(findings);
  if (ShadowCommon::saveJson(ShadowCommon::configPath(),
                             ShadowCommon::toJson(config), &error))
    refresh();
}

void HomeShadow::setError(const QString &title, const QString &detail) {
  m_state = QStringLiteral("error");
  m_title = title;
  m_detail = detail;
  m_busy = false;
  emit changed();
}

void HomeShadow::refreshSoon() {
  QTimer::singleShot(1500, this, &HomeShadow::refresh);
}
