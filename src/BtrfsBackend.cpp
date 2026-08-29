#include "BtrfsBackend.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace {

qulonglong captureNumber(const QString &text, const QString &label) {
  const QRegularExpression re(QStringLiteral("^\\s*%1:\\s*(\\d+)")
                                  .arg(QRegularExpression::escape(label)),
                              QRegularExpression::MultilineOption);
  const auto match = re.match(text);
  return match.hasMatch() ? match.captured(1).toULongLong() : 0;
}

} // namespace

BtrfsBackend::BtrfsBackend(QObject *parent) : FilesystemBackend(parent) {
  composeAuditMeaning();
  refresh();
}

QString BtrfsBackend::run(const QString &program, const QStringList &arguments,
                          int timeoutMs) {
  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.setProgram(program);
  process.setArguments(arguments);
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  process.setProcessEnvironment(environment);
  process.start();
  if (!process.waitForFinished(timeoutMs)) {
    process.kill();
    process.waitForFinished();
  }
  return QString::fromUtf8(process.readAll()).trimmed();
}

BtrfsBackend::Usage BtrfsBackend::parseUsage(const QString &output) {
  Usage usage;
  usage.deviceSize = captureNumber(output, QStringLiteral("Device size"));
  usage.used = captureNumber(output, QStringLiteral("Used"));
  usage.estimatedFree =
      captureNumber(output, QStringLiteral("Free (estimated)"));

  const QRegularExpression dataRe(
      QStringLiteral("^Data,[^:]+:\\s*Size:\\s*(\\d+),\\s*Used:\\s*(\\d+)"),
      QRegularExpression::MultilineOption);
  const auto match = dataRe.match(output);
  if (match.hasMatch()) {
    usage.dataSize = match.captured(1).toULongLong();
    usage.dataUsed = match.captured(2).toULongLong();
  }
  return usage;
}

int BtrfsBackend::parseDeviceErrors(const QString &output) {
  int total = 0;
  const QRegularExpression re(QStringLiteral(
      "\\.(?:write_io|read_io|flush_io|corruption|generation)_errs\\s+(\\d+)"));
  auto it = re.globalMatch(output);
  while (it.hasNext())
    total += it.next().captured(1).toInt();
  return total;
}

QVariantList BtrfsBackend::parseRecoveryPoints(const QByteArray &json) {
  QVariantList points;
  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(json, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return points;

  const QJsonArray entries =
      document.object().value(QStringLiteral("snapshotEntries")).toArray();
  for (const QJsonValue &entryValue : entries) {
    const QJsonObject entry = entryValue.toObject();
    const QJsonObject snapshot =
        entry.value(QStringLiteral("snapperID")).toObject();
    const QJsonObject properties =
        snapshot.value(QStringLiteral("properties")).toObject();
    QVariantMap point;
    point.insert(QStringLiteral("id"),
                 snapshot.value(QStringLiteral("snapshotID")).toInt());
    point.insert(QStringLiteral("timestamp"),
                 snapshot.value(QStringLiteral("timestamp")).toString());
    point.insert(QStringLiteral("description"),
                 properties.value(QStringLiteral("description")).toString());
    point.insert(
        QStringLiteral("bootEntry"),
        !entry.value(QStringLiteral("kernelEntries")).toArray().isEmpty());
    points.push_back(point);
  }

  std::sort(points.begin(), points.end(),
            [](const QVariant &a, const QVariant &b) {
              return a.toMap().value(QStringLiteral("timestamp")).toString() >
                     b.toMap().value(QStringLiteral("timestamp")).toString();
            });
  return points;
}

QVariantList BtrfsBackend::parseAuditFindings(const QByteArray &json,
                                              QString *error,
                                              int *actualSnapshots,
                                              int *managedSnapshots,
                                              qulonglong *potentialBytes) {
  if (error)
    error->clear();
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    if (error)
      *error =
          QStringLiteral("The recovery probe returned an unreadable result.");
    return {};
  }

  const QJsonObject root = document.object();
  if (root.value(QStringLiteral("schema")).toInt() != 1) {
    if (error)
      *error =
          QStringLiteral("The recovery probe returned an unsupported result.");
    return {};
  }
  if (!root.value(QStringLiteral("error")).toString().isEmpty()) {
    if (error)
      *error = root.value(QStringLiteral("error")).toString();
    return {};
  }

  if (actualSnapshots)
    *actualSnapshots = root.value(QStringLiteral("actualSnapshots")).toInt();
  if (managedSnapshots)
    *managedSnapshots = root.value(QStringLiteral("managedSnapshots")).toInt();
  if (potentialBytes)
    *potentialBytes =
        root.value(QStringLiteral("potentialBytes")).toVariant().toULongLong();

  QVariantList findings;
  const QJsonArray entries = root.value(QStringLiteral("findings")).toArray();
  for (const QJsonValue &entry : entries)
    findings.push_back(entry.toObject().toVariantMap());
  return findings;
}

QString BtrfsBackend::valueFromConfig(const QString &path, const QString &key) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};
  const QString text = QString::fromUtf8(file.readAll());
  const QRegularExpression re(
      QStringLiteral("^\\s*%1\\s*=\\s*[\"']?([^\"'\\r\\n#]+)")
          .arg(QRegularExpression::escape(key)),
      QRegularExpression::MultilineOption);
  const auto match = re.match(text);
  return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

bool BtrfsBackend::unitIs(const QString &unit, const QString &property,
                          const QString &expected) {
  return run(QStringLiteral("systemctl"),
             {QStringLiteral("show"), unit,
              QStringLiteral("--property=%1").arg(property),
              QStringLiteral("--value")}) == expected;
}

QString BtrfsBackend::findRecoveryMetadata() {
  const QString direct = QStringLiteral("/boot/limine_history/snapshots.json");
  if (QFileInfo(direct).isReadable())
    return direct;

  const QDir boot(QStringLiteral("/boot"));
  const QFileInfoList dirs =
      boot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QFileInfo &dir : dirs) {
    const QString candidate =
        QDir(dir.absoluteFilePath())
            .filePath(QStringLiteral("limine_history/snapshots.json"));
    if (QFileInfo(candidate).isReadable())
      return candidate;
  }
  return {};
}

void BtrfsBackend::refresh() {
  setBusy(true);

  const QString mount =
      run(QStringLiteral("findmnt"),
          {QStringLiteral("--noheadings"), QStringLiteral("--output"),
           QStringLiteral("SOURCE,FSTYPE,OPTIONS"), QStringLiteral("/")});
  const QStringList mountParts = mount.split(
      QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
  m_supported =
      mountParts.size() >= 2 && mountParts.at(1) == QLatin1String("btrfs");
  m_source = mountParts.value(0);
  m_mountSummary = mountParts.mid(2).join(QLatin1Char(' '));

  QStorageInfo storage(QStringLiteral("/"));
  storage.refresh();
  m_totalBytes = storage.bytesTotal();
  m_freeBytes = storage.bytesAvailable();
  m_usedBytes = m_totalBytes > m_freeBytes ? m_totalBytes - m_freeBytes : 0;

  if (m_supported) {
    const Usage usage =
        parseUsage(run(QStringLiteral("btrfs"),
                       {QStringLiteral("filesystem"), QStringLiteral("usage"),
                        QStringLiteral("-b"), QStringLiteral("/")},
                       4000));
    if (usage.deviceSize > 0)
      m_totalBytes = usage.deviceSize;
    if (usage.estimatedFree > 0)
      m_freeBytes = usage.estimatedFree;
    m_dataChunkPercent = usage.dataSize > 0
                             ? (100.0 * static_cast<double>(usage.dataUsed) /
                                static_cast<double>(usage.dataSize))
                             : 0;
    m_deviceErrors =
        parseDeviceErrors(run(QStringLiteral("btrfs"),
                              {QStringLiteral("device"),
                               QStringLiteral("stats"), QStringLiteral("/")},
                              4000));
  }

  // "Used" in `btrfs filesystem usage` is an extent-accounting value and
  // does not add up with estimated free space once allocation and duplicated
  // metadata overhead are involved. The user-facing capacity answer must be
  // total minus what the filesystem says is actually available.
  m_usedBytes = m_totalBytes > m_freeBytes ? m_totalBytes - m_freeBytes : 0;

  m_usedPercent = m_totalBytes > 0 ? 100.0 * static_cast<double>(m_usedBytes) /
                                         static_cast<double>(m_totalBytes)
                                   : 0;
  m_freePercent = m_totalBytes > 0 ? 100.0 * static_cast<double>(m_freeBytes) /
                                         static_cast<double>(m_totalBytes)
                                   : 0;

  const QString snapperConfig = QStringLiteral("/etc/snapper/configs/root");
  m_snapshotLimit =
      valueFromConfig(snapperConfig, QStringLiteral("NUMBER_LIMIT")).toInt();
  m_timelineEnabled =
      valueFromConfig(snapperConfig, QStringLiteral("TIMELINE_CREATE")) ==
      QLatin1String("yes");
  m_cleanupActive =
      unitIs(QStringLiteral("snapper-cleanup.timer"),
             QStringLiteral("ActiveState"), QStringLiteral("active"));
  m_bootSyncActive =
      unitIs(QStringLiteral("limine-snapper-sync.service"),
             QStringLiteral("ActiveState"), QStringLiteral("active"));

  m_recoveryPoints.clear();
  const QString recoveryPath = findRecoveryMetadata();
  if (!recoveryPath.isEmpty()) {
    QFile recovery(recoveryPath);
    if (recovery.open(QIODevice::ReadOnly))
      m_recoveryPoints = parseRecoveryPoints(recovery.readAll());
  }

  composeMeaning();
  markUpdated();
  setBusy(false);
  emit stateChanged();
}

void BtrfsBackend::verifyRecovery() {
  if (!m_supported ||
      (m_auditProcess && m_auditProcess->state() != QProcess::NotRunning))
    return;

  const QString helper =
      QCoreApplication::applicationDirPath() + QStringLiteral("/butter-probe");
  if (!QFileInfo::exists(helper)) {
    m_auditState = QStringLiteral("error");
    m_auditTitle = QStringLiteral("Recovery check is unavailable");
    m_auditDetail =
        QStringLiteral("Butter could not find its read-only recovery helper.");
    emit stateChanged();
    return;
  }

  if (!m_auditProcess) {
    m_auditProcess = new QProcess(this);
    m_auditProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_auditProcess, &QProcess::readyReadStandardError, this, [this] {
      const QString progress =
          QString::fromUtf8(m_auditProcess->readAllStandardError()).trimmed();
      if (!progress.isEmpty()) {
        m_auditDetail = progress.section(QLatin1Char('\n'), -1);
        emit stateChanged();
      }
    });
    connect(m_auditProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            &BtrfsBackend::finishRecoveryAudit);
    connect(m_auditProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError processError) {
              if (processError != QProcess::FailedToStart)
                return;
              m_auditState = QStringLiteral("error");
              m_auditTitle = QStringLiteral("Recovery check is unavailable");
              m_auditDetail = QStringLiteral(
                  "Butter could not start the system authorization service.");
              emit stateChanged();
            });
  }

  m_auditFindings.clear();
  m_orphanCount = 0;
  m_potentialRecoveryBytes = 0;
  m_auditState = QStringLiteral("running");
  m_auditTitle = QStringLiteral("Checking every recovery copy");
  m_auditDetail = QStringLiteral("Waiting for permission…");
  emit stateChanged();

  m_auditProcess->setProgram(QStringLiteral("pkexec"));
  m_auditProcess->setArguments({helper});
  m_auditProcess->start();
}

void BtrfsBackend::finishRecoveryAudit(int exitCode,
                                       QProcess::ExitStatus exitStatus) {
  const QByteArray output = m_auditProcess->readAllStandardOutput();
  if (exitStatus != QProcess::NormalExit || exitCode != 0) {
    if (exitCode == 126 || exitCode == 127) {
      m_auditState = QStringLiteral("cancelled");
      m_auditTitle = QStringLiteral("Recovery check cancelled");
      m_auditDetail = QStringLiteral(
          "Nothing was read or changed. You can try again whenever you like.");
    } else {
      QString error;
      parseAuditFindings(output, &error);
      m_auditState = QStringLiteral("error");
      m_auditTitle = QStringLiteral("Butter could not finish the check");
      m_auditDetail =
          error.isEmpty()
              ? QStringLiteral(
                    "The privileged recovery probe stopped unexpectedly.")
              : error;
    }
    emit stateChanged();
    return;
  }

  QString error;
  int actualSnapshots = 0;
  int managedSnapshots = 0;
  m_auditFindings =
      parseAuditFindings(output, &error, &actualSnapshots, &managedSnapshots,
                         &m_potentialRecoveryBytes);
  if (!error.isEmpty()) {
    m_auditState = QStringLiteral("error");
    m_auditTitle = QStringLiteral("Butter could not finish the check");
    m_auditDetail = error;
    emit stateChanged();
    return;
  }

  for (QVariant &findingValue : m_auditFindings) {
    QVariantMap finding = findingValue.toMap();
    const int snapshotNumber =
        finding.value(QStringLiteral("snapshotNumber")).toInt();
    const bool bootVisible = std::any_of(
        m_recoveryPoints.cbegin(), m_recoveryPoints.cend(),
        [snapshotNumber](const QVariant &point) {
          return point.toMap().value(QStringLiteral("id")).toInt() ==
                 snapshotNumber;
        });
    finding.insert(QStringLiteral("bootVisible"), bootVisible);
    finding.insert(
        QStringLiteral("remedyEligible"),
        finding.value(QStringLiteral("kind")).toString() ==
                QLatin1String("unmanaged") &&
            finding.value(QStringLiteral("remedyCandidate")).toBool() &&
            !bootVisible);
    findingValue = finding;
  }
  m_orphanCount = std::count_if(
      m_auditFindings.cbegin(), m_auditFindings.cend(),
      [](const QVariant &finding) {
        return finding.toMap().value(QStringLiteral("kind")).toString() ==
               QLatin1String("unmanaged");
      });
  m_auditState = QStringLiteral("verified");
  composeAuditMeaning(actualSnapshots, managedSnapshots);
  composeMeaning();
  emit stateChanged();
}

void BtrfsBackend::removeOrphan(int snapshotNumber) {
  if (m_remedyProcess && m_remedyProcess->state() != QProcess::NotRunning)
    return;

  const auto findingIterator =
      std::find_if(m_auditFindings.cbegin(), m_auditFindings.cend(),
                   [snapshotNumber](const QVariant &finding) {
                     return finding.toMap()
                                .value(QStringLiteral("snapshotNumber"))
                                .toInt() == snapshotNumber;
                   });
  if (findingIterator == m_auditFindings.cend() ||
      !findingIterator->toMap()
           .value(QStringLiteral("remedyEligible"))
           .toBool()) {
    m_remedyState = QStringLiteral("error");
    m_remedyTitle = QStringLiteral("Expert review is needed");
    m_remedyDetail = QStringLiteral(
        "Butter does not have enough evidence to remove this copy safely.");
    emit stateChanged();
    return;
  }

  const QVariantMap finding = findingIterator->toMap();
  const int subvolumeId = finding.value(QStringLiteral("subvolumeId")).toInt();
  const QString helper =
      QCoreApplication::applicationDirPath() + QStringLiteral("/butter-remedy");
  if (!QFileInfo::exists(helper)) {
    m_remedyState = QStringLiteral("error");
    m_remedyTitle = QStringLiteral("Removal is unavailable");
    m_remedyDetail =
        QStringLiteral("Butter could not find its privileged helper.");
    emit stateChanged();
    return;
  }

  if (!m_remedyProcess) {
    m_remedyProcess = new QProcess(this);
    m_remedyProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_remedyProcess, &QProcess::readyReadStandardError, this, [this] {
      const QString progress =
          QString::fromUtf8(m_remedyProcess->readAllStandardError()).trimmed();
      if (!progress.isEmpty()) {
        m_remedyDetail = progress.section(QLatin1Char('\n'), -1);
        emit stateChanged();
      }
    });
    connect(m_remedyProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            &BtrfsBackend::finishRemedy);
    connect(m_remedyProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError processError) {
              if (processError != QProcess::FailedToStart)
                return;
              m_remedyState = QStringLiteral("error");
              m_remedyTitle = QStringLiteral("Removal is unavailable");
              m_remedyDetail = QStringLiteral(
                  "Butter could not start the system authorization service.");
              emit stateChanged();
            });
  }

  m_remedyState = QStringLiteral("running");
  m_remedyTitle = QStringLiteral("Re-checking before removal");
  m_remedyDetail = QStringLiteral(
      "Butter will stop if any evidence has changed since review.");
  emit stateChanged();

  m_remedyProcess->setProgram(QStringLiteral("pkexec"));
  m_remedyProcess->setArguments({helper, QStringLiteral("--remove-orphan"),
                                 QString::number(snapshotNumber),
                                 QString::number(subvolumeId)});
  m_remedyProcess->start();
}

void BtrfsBackend::finishRemedy(int exitCode, QProcess::ExitStatus exitStatus) {
  const QByteArray output = m_remedyProcess->readAllStandardOutput();
  if (exitStatus != QProcess::NormalExit || exitCode != 0) {
    if (exitCode == 126 || exitCode == 127) {
      m_remedyState = QStringLiteral("cancelled");
      m_remedyTitle = QStringLiteral("Removal cancelled");
      m_remedyDetail =
          QStringLiteral("The recovery copy is still there and unchanged.");
    } else {
      QString error;
      parseAuditFindings(output, &error);
      m_remedyState = QStringLiteral("error");
      m_remedyTitle = QStringLiteral("Butter stopped safely");
      m_remedyDetail =
          error.isEmpty() ? QStringLiteral("The recovery copy was not removed.")
                          : error;
    }
    emit stateChanged();
    return;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(output, &parseError);
  const QJsonObject result = document.object();
  if (parseError.error != QJsonParseError::NoError ||
      result.value(QStringLiteral("operation")).toString() !=
          QLatin1String("removed")) {
    m_remedyState = QStringLiteral("error");
    m_remedyTitle = QStringLiteral("Butter could not confirm removal");
    m_remedyDetail = QStringLiteral(
        "The result was unclear. Run the recovery check again before acting.");
    emit stateChanged();
    return;
  }

  const int snapshotNumber =
      result.value(QStringLiteral("snapshotNumber")).toInt();
  QVariantList remaining;
  qulonglong remainingBytes = 0;
  for (const QVariant &findingValue : std::as_const(m_auditFindings)) {
    const QVariantMap finding = findingValue.toMap();
    if (finding.value(QStringLiteral("snapshotNumber")).toInt() ==
        snapshotNumber)
      continue;
    remaining.push_back(findingValue);
    remainingBytes +=
        finding.value(QStringLiteral("exclusiveBytes")).toULongLong();
  }
  m_auditFindings = remaining;
  m_potentialRecoveryBytes = remainingBytes;
  m_orphanCount = std::count_if(
      remaining.cbegin(), remaining.cend(), [](const QVariant &finding) {
        return finding.toMap().value(QStringLiteral("kind")).toString() ==
               QLatin1String("unmanaged");
      });
  m_remedyState = QStringLiteral("removed");
  m_remedyTitle = QStringLiteral("Recovery copy removed safely");
  const double estimatedGb =
      result.value(QStringLiteral("estimatedBytes")).toVariant().toULongLong() /
      1e9;
  m_remedyDetail =
      estimatedGb > 0
          ? QStringLiteral("Btrfs accepted snapshot %1 for removal. About "
                           "%2 GB can now be reclaimed; space may return "
                           "gradually.")
                .arg(snapshotNumber)
                .arg(estimatedGb, 0, 'f', 1)
          : QStringLiteral("Btrfs accepted snapshot %1 for removal. Space "
                           "may return gradually.")
                .arg(snapshotNumber);
  if (!result.value(QStringLiteral("cleanupComplete")).toBool())
    m_remedyDetail += QStringLiteral(
        " The snapshot is gone, but its empty bookkeeping folder needs an "
        "Omarchy agent to tidy up.");
  m_auditTitle = QStringLiteral("Unmanaged copy removed");
  m_auditDetail = QStringLiteral(
      "Butter re-checked its identity and safety conditions immediately before "
      "removal.");
  composeMeaning();
  emit stateChanged();
  QTimer::singleShot(1000, this, &BtrfsBackend::refresh);
}

QString BtrfsBackend::auditReport() const {
  QStringList report;
  report << QStringLiteral("Butter recovery evidence")
         << QStringLiteral("Filesystem: %1").arg(m_source)
         << QStringLiteral("Generated: %1")
                .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
         << QString();
  for (const QVariant &findingValue : m_auditFindings) {
    const QVariantMap finding = findingValue.toMap();
    report
        << QStringLiteral("Snapshot: %1")
               .arg(finding.value(QStringLiteral("snapshotNumber")).toInt())
        << QStringLiteral("Btrfs subvolume ID: %1")
               .arg(finding.value(QStringLiteral("subvolumeId")).toInt())
        << QStringLiteral("Created: %1")
               .arg(finding.value(QStringLiteral("creationTime")).toString())
        << QStringLiteral("Read-only: %1")
               .arg(finding.value(QStringLiteral("readOnly")).toBool()
                        ? QStringLiteral("yes")
                        : QStringLiteral("no"))
        << QStringLiteral("Snapper metadata: %1")
               .arg(finding.value(QStringLiteral("metadata")).toString())
        << QStringLiteral("Visible in Limine: %1")
               .arg(finding.value(QStringLiteral("bootVisible")).toBool()
                        ? QStringLiteral("yes")
                        : QStringLiteral("no"))
        << QStringLiteral("Total referenced bytes: %1")
               .arg(finding.value(QStringLiteral("totalBytes")).toULongLong())
        << QStringLiteral("Estimated exclusive bytes: %1")
               .arg(finding.value(QStringLiteral("exclusiveBytes"))
                        .toULongLong())
        << QStringLiteral("Butter remedy: %1")
               .arg(finding.value(QStringLiteral("remedyEligible")).toBool()
                        ? QStringLiteral("eligible after live safety re-check")
                        : QStringLiteral("defer to an Omarchy agent"))
        << QString();
  }
  report << QStringLiteral("Names used in this report:")
         << QStringLiteral(
                "Btrfs: the filesystem storing files and recovery copies")
         << QStringLiteral("Snapper: Omarchy's recovery-copy tracker")
         << QStringLiteral(
                "Limine: the startup menu that can offer recovery copies")
         << QString()
         << QStringLiteral(
                "Butter made no changes during this evidence check.");
  return report.join(QLatin1Char('\n'));
}

void BtrfsBackend::copyAuditReport() {
  QGuiApplication::clipboard()->setText(auditReport());
  m_reportCopied = true;
  emit stateChanged();
  QTimer::singleShot(2000, this, [this] {
    m_reportCopied = false;
    emit stateChanged();
  });
}

void BtrfsBackend::composeAuditMeaning(int actualSnapshots,
                                       int managedSnapshots) {
  if (m_auditState == QLatin1String("idle")) {
    m_auditTitle = QStringLiteral("Check for hidden recovery copies");
    m_auditDetail = QStringLiteral(
        "A deeper read-only check compares Btrfs, where your files and copies "
        "live, with Snapper, Omarchy's recovery-copy tracker. It asks once for "
        "permission.");
    return;
  }
  if (m_auditState != QLatin1String("verified"))
    return;

  if (m_auditFindings.isEmpty()) {
    m_auditTitle = QStringLiteral("Everything lines up");
    m_auditDetail =
        QStringLiteral("All %1 recovery copies are accounted for by Snapper, "
                       "Omarchy's recovery-copy tracker. Butter changed "
                       "nothing.")
            .arg(actualSnapshots);
    return;
  }

  if (m_orphanCount == 1) {
    const QVariantMap orphan =
        std::find_if(
            m_auditFindings.cbegin(), m_auditFindings.cend(),
            [](const QVariant &finding) {
              return finding.toMap().value(QStringLiteral("kind")).toString() ==
                     QLatin1String("unmanaged");
            })
            ->toMap();
    m_auditTitle = QStringLiteral("1 unmanaged recovery copy");
    const QString size =
        m_potentialRecoveryBytes > 0
            ? QStringLiteral(" It may be keeping %1 GB.")
                  .arg(static_cast<double>(m_potentialRecoveryBytes) / 1e9, 0,
                       'f', 1)
            : QString();
    const QString visibility =
        orphan.value(QStringLiteral("bootVisible")).toBool()
            ? QStringLiteral(" It is still shown at startup.")
            : QStringLiteral(" It is not shown at startup.");
    const QString metadata =
        orphan.value(QStringLiteral("metadata")).toString() ==
                QLatin1String("empty")
            ? QStringLiteral(" Its management record is empty.")
            : QString();
    m_auditDetail =
        QStringLiteral(
            "Snapshot %1 exists in Btrfs but Snapper, Omarchy's recovery-copy "
            "tracker, does not manage it.%2%3%4 Butter changed nothing.")
            .arg(orphan.value(QStringLiteral("snapshotNumber")).toInt())
            .arg(metadata)
            .arg(visibility)
            .arg(size);
    return;
  }

  m_auditTitle =
      QStringLiteral("%1 recovery mismatches").arg(m_auditFindings.size());
  m_auditDetail =
      QStringLiteral("%1 Btrfs copies and %2 Snapper records were compared. "
                     "Review is recommended; nothing was changed.")
          .arg(actualSnapshots)
          .arg(managedSnapshots);
}

void BtrfsBackend::composeMeaning() {
  if (!m_supported) {
    m_severity = QStringLiteral("neutral");
    m_verdict = QStringLiteral("Butter is looking for your filesystem");
    m_verdictDetail = QStringLiteral(
        "This first slice understands Btrfs, the filesystem Omarchy uses to "
        "store your files and recovery copies.");
  } else if (m_deviceErrors > 0) {
    m_severity = QStringLiteral("danger");
    m_verdict = QStringLiteral("Your drive needs attention");
    m_verdictDetail = QStringLiteral("The filesystem has recorded %1 device "
                                     "errors. Butter has not changed anything.")
                          .arg(m_deviceErrors);
  } else if (m_orphanCount > 0) {
    m_severity = QStringLiteral("warning");
    m_verdict = m_orphanCount == 1
                    ? QStringLiteral("An old recovery copy is hiding")
                    : QStringLiteral("Old recovery copies are hiding");
    m_verdictDetail = QStringLiteral(
        "Butter found recovery data that Omarchy's recovery-copy tracker does "
        "not manage. Nothing has been changed.");
  } else if (m_freePercent < 5.0) {
    m_severity = QStringLiteral("warning");
    m_verdict = QStringLiteral("Space is tight");
    m_verdictDetail =
        QStringLiteral("Your filesystem looks calm, but it has very little "
                       "breathing room. Nothing has been changed.");
  } else if (m_recoveryPoints.isEmpty()) {
    m_severity = QStringLiteral("warning");
    m_verdict = QStringLiteral("Recovery is not visible yet");
    m_verdictDetail =
        QStringLiteral("The filesystem looks healthy, but Butter could not "
                       "find startup recovery entries.");
  } else {
    m_severity = QStringLiteral("good");
    m_verdict = QStringLiteral("You’re in good shape");
    m_verdictDetail =
        QStringLiteral("Your filesystem reports no device errors and Omarchy "
                       "recovery entries are present.");
  }

  const int bootable = std::count_if(
      m_recoveryPoints.cbegin(), m_recoveryPoints.cend(),
      [](const QVariant &value) {
        return value.toMap().value(QStringLiteral("bootEntry")).toBool();
      });
  if (!m_recoveryPoints.isEmpty()) {
    const QVariantMap newest = m_recoveryPoints.first().toMap();
    const QString rawTimestamp =
        newest.value(QStringLiteral("timestamp")).toString();
    const QDateTime created = QDateTime::fromString(
        rawTimestamp, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QString friendlyTimestamp =
        created.isValid()
            ? created.toString(QStringLiteral("MMM d 'at' h:mm AP"))
            : rawTimestamp;
    const QString visibleCount = bootable == m_recoveryPoints.size()
                                     ? QStringLiteral("All %1").arg(bootable)
                                     : QStringLiteral("%1 of %2")
                                           .arg(bootable)
                                           .arg(m_recoveryPoints.size());
    m_recoveryTitle =
        QStringLiteral("%1 ways back").arg(m_recoveryPoints.size());
    m_recoveryDetail =
        QStringLiteral("%1 appear in your startup menu. The newest was created "
                       "%2, before the update from Omarchy %3.")
            .arg(visibleCount)
            .arg(friendlyTimestamp)
            .arg(newest.value(QStringLiteral("description")).toString());
  } else {
    m_recoveryTitle = QStringLiteral("Recovery points not visible");
    m_recoveryDetail = QStringLiteral(
        "Butter could not read Limine, the startup recovery menu. It will not "
        "guess.");
  }
}
