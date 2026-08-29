#include "RecoveryAudit.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <algorithm>
#include <cstdio>
#include <unistd.h>

namespace {

struct CommandResult {
  int exitCode = -1;
  QString output;
  QString error;
};

CommandResult run(const QString &program, const QStringList &arguments,
                  int timeoutMs = 15000) {
  QProcess process;
  process.setProgram(program);
  process.setArguments(arguments);
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  process.setProcessEnvironment(environment);
  process.start();
  if (!process.waitForStarted(3000))
    return {-1, {}, process.errorString()};
  if (!process.waitForFinished(timeoutMs)) {
    process.kill();
    process.waitForFinished();
    return {-1, QString::fromUtf8(process.readAllStandardOutput()),
            QStringLiteral("The read-only command timed out.")};
  }
  return {process.exitCode(),
          QString::fromUtf8(process.readAllStandardOutput()).trimmed(),
          QString::fromUtf8(process.readAllStandardError()).trimmed()};
}

void progress(const char *message) {
  std::fprintf(stderr, "%s\n", message);
  std::fflush(stderr);
}

int fail(const QString &message) {
  QJsonObject result;
  result.insert(QStringLiteral("schema"), 1);
  result.insert(QStringLiteral("error"), message);
  std::fputs(QJsonDocument(result).toJson(QJsonDocument::Compact).constData(),
             stdout);
  std::fputc('\n', stdout);
  return 1;
}

CommandResult readSubvolumes() {
  return run(QStringLiteral("btrfs"),
             {QStringLiteral("subvolume"), QStringLiteral("list"),
              QStringLiteral("-t"), QStringLiteral("/")});
}

CommandResult readSnapperNumbers() {
  return run(QStringLiteral("snapper"),
             {QStringLiteral("--no-dbus"), QStringLiteral("--root"),
              QStringLiteral("/"), QStringLiteral("--config"),
              QStringLiteral("root"), QStringLiteral("--csvout"),
              QStringLiteral("--no-headers"), QStringLiteral("list"),
              QStringLiteral("--disable-used-space"),
              QStringLiteral("--columns"), QStringLiteral("number")});
}

#ifdef BUTTER_REMEDY_HELPER
QString findRecoveryMetadata() {
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

bool recoveryMetadataContains(int snapshotNumber, QString *error) {
  const QString path = findRecoveryMetadata();
  QFile file(path);
  if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
    *error = QStringLiteral(
        "Butter could not verify Limine, the startup recovery menu.");
    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    *error = QStringLiteral(
        "Limine's startup recovery records could not be verified.");
    return false;
  }

  const QJsonArray entries =
      document.object().value(QStringLiteral("snapshotEntries")).toArray();
  for (const QJsonValue &entryValue : entries) {
    const QJsonObject snapshot =
        entryValue.toObject().value(QStringLiteral("snapperID")).toObject();
    if (snapshot.value(QStringLiteral("snapshotID")).toInt() == snapshotNumber)
      return true;
  }
  return false;
}

int removeOrphan(int snapshotNumber, int expectedSubvolumeId) {
  if (snapshotNumber <= 0 || expectedSubvolumeId <= 0)
    return fail(QStringLiteral("Butter refused an invalid recovery identity."));

  progress("Re-checking the recovery copy…");
  const CommandResult subvolumes = readSubvolumes();
  if (subvolumes.exitCode != 0)
    return fail(
        QStringLiteral("The filesystem would not re-check the recovery copy."));
  const auto actual = RecoveryAudit::parseSnapshotSubvolumes(subvolumes.output);
  const auto allSubvolumes = RecoveryAudit::parseSubvolumes(subvolumes.output);
  const auto candidate = std::find_if(
      actual.cbegin(), actual.cend(), [snapshotNumber](const auto &snapshot) {
        return snapshot.snapshotNumber == snapshotNumber;
      });
  if (candidate == actual.cend() ||
      candidate->subvolumeId != expectedSubvolumeId)
    return fail(QStringLiteral(
        "The recovery copy changed after review, so Butter stopped."));
  const bool hasNestedSubvolume = std::any_of(
      allSubvolumes.cbegin(), allSubvolumes.cend(),
      [&candidate](const auto &item) {
        return item.path.startsWith(candidate->path + QLatin1Char('/'));
      });
  if (hasNestedSubvolume)
    return fail(QStringLiteral("The recovery copy contains another subvolume "
                               "and needs expert review."));

  const CommandResult snapper = readSnapperNumbers();
  if (snapper.exitCode != 0 ||
      RecoveryAudit::parseSnapperNumbers(snapper.output)
          .contains(snapshotNumber))
    return fail(QStringLiteral(
        "Snapper, Omarchy's recovery-copy tracker, now manages this copy, so "
        "Butter stopped."));

  const QString path =
      QStringLiteral("/.snapshots/%1/snapshot").arg(snapshotNumber);
  const QString metadataPath =
      QStringLiteral("/.snapshots/%1/info.xml").arg(snapshotNumber);
  const QFileInfo pathInfo(path);
  const QFileInfo metadata(metadataPath);
  if (!pathInfo.isDir() || pathInfo.isSymbolicLink() ||
      (!pathInfo.canonicalFilePath().isEmpty() &&
       pathInfo.canonicalFilePath() != path))
    return fail(QStringLiteral("The recovery path is not safe to remove."));
  if (metadata.exists() &&
      (metadata.isSymbolicLink() || !metadata.isFile() || metadata.size() != 0))
    return fail(QStringLiteral(
        "The recovery copy now has management metadata, so Butter stopped."));

  const CommandResult shown =
      run(QStringLiteral("btrfs"),
          {QStringLiteral("subvolume"), QStringLiteral("show"), path});
  const RecoveryAudit::SubvolumeDetails details =
      shown.exitCode == 0 ? RecoveryAudit::parseSubvolumeDetails(shown.output)
                          : RecoveryAudit::SubvolumeDetails{};
  if (!details.valid || details.subvolumeId != expectedSubvolumeId ||
      !details.readOnly)
    return fail(QStringLiteral("The recovery copy is no longer the read-only "
                               "snapshot Butter reviewed."));

  const CommandResult currentRoot = run(
      QStringLiteral("btrfs"), {QStringLiteral("inspect-internal"),
                                QStringLiteral("rootid"), QStringLiteral("/")});
  bool rootIdValid = false;
  const int rootId = currentRoot.output.trimmed().toInt(&rootIdValid);
  if (currentRoot.exitCode != 0 || !rootIdValid ||
      rootId == expectedSubvolumeId)
    return fail(QStringLiteral(
        "Butter could not prove this is separate from the running system."));

  const CommandResult mounted =
      run(QStringLiteral("findmnt"),
          {QStringLiteral("--noheadings"), QStringLiteral("--output"),
           QStringLiteral("TARGET"), QStringLiteral("--target"), path});
  if (mounted.output.trimmed() == path)
    return fail(QStringLiteral(
        "The recovery copy is mounted and cannot be removed safely."));

  QString recoveryError;
  const bool bootVisible =
      recoveryMetadataContains(snapshotNumber, &recoveryError);
  if (!recoveryError.isEmpty())
    return fail(recoveryError);
  if (bootVisible)
    return fail(QStringLiteral(
        "The recovery copy is now shown at startup, so Butter stopped."));

  const CommandResult measured =
      run(QStringLiteral("btrfs"),
          {QStringLiteral("filesystem"), QStringLiteral("du"),
           QStringLiteral("-s"), QStringLiteral("--raw"), path},
          120000);
  const RecoveryAudit::SpaceUsage usage =
      measured.exitCode == 0 ? RecoveryAudit::parseSpaceUsage(measured.output)
                             : RecoveryAudit::SpaceUsage{};

  progress("Removing the verified unmanaged copy…");
  const CommandResult removed = run(
      QStringLiteral("btrfs"),
      {QStringLiteral("subvolume"), QStringLiteral("delete"), path}, 120000);
  if (removed.exitCode != 0)
    return fail(
        QStringLiteral("The filesystem refused to remove the recovery copy: %1")
            .arg(removed.error));

  bool metadataClean = true;
  if (metadata.exists())
    metadataClean = QFile::remove(metadataPath);
  const QString snapshotDirectory =
      QStringLiteral("/.snapshots/%1").arg(snapshotNumber);
  bool directoryClean = !QFileInfo::exists(snapshotDirectory);
  if (metadataClean && !directoryClean)
    directoryClean = QDir(QStringLiteral("/.snapshots"))
                         .rmdir(QString::number(snapshotNumber));

  QJsonObject result;
  result.insert(QStringLiteral("schema"), 1);
  result.insert(QStringLiteral("operation"), QStringLiteral("removed"));
  result.insert(QStringLiteral("snapshotNumber"), snapshotNumber);
  result.insert(QStringLiteral("subvolumeId"), expectedSubvolumeId);
  result.insert(QStringLiteral("cleanupComplete"),
                metadataClean && directoryClean);
  if (usage.valid)
    result.insert(QStringLiteral("estimatedBytes"),
                  static_cast<qint64>(usage.exclusiveBytes));
  std::fputs(QJsonDocument(result).toJson(QJsonDocument::Compact).constData(),
             stdout);
  std::fputc('\n', stdout);
  return 0;
}
#endif

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  if (geteuid() != 0)
    return fail(QStringLiteral("The recovery probe was not authorized."));

  const QStringList arguments = QCoreApplication::arguments();
#ifdef BUTTER_REMEDY_HELPER
  if (arguments.size() == 4 &&
      arguments.at(1) == QLatin1String("--remove-orphan")) {
    bool numberValid = false;
    bool idValid = false;
    const int snapshotNumber = arguments.at(2).toInt(&numberValid);
    const int subvolumeId = arguments.at(3).toInt(&idValid);
    if (!numberValid || !idValid)
      return fail(
          QStringLiteral("Butter refused an invalid recovery identity."));
    return removeOrphan(snapshotNumber, subvolumeId);
  }
  return fail(QStringLiteral("Butter refused an unsupported remedy."));
#else
  if (arguments.size() != 1)
    return fail(QStringLiteral(
        "The read-only Butter probe does not accept privileged actions."));

  progress("Reading Btrfs recovery inventory…");
  const CommandResult subvolumes = readSubvolumes();
  if (subvolumes.exitCode != 0)
    return fail(
        QStringLiteral("Btrfs would not provide its recovery inventory: %1")
            .arg(subvolumes.error));

  const QList<RecoveryAudit::SnapshotSubvolume> actual =
      RecoveryAudit::parseSnapshotSubvolumes(subvolumes.output);
  const QList<RecoveryAudit::SubvolumeEntry> allSubvolumes =
      RecoveryAudit::parseSubvolumes(subvolumes.output);
  const CommandResult currentRoot = run(
      QStringLiteral("btrfs"), {QStringLiteral("inspect-internal"),
                                QStringLiteral("rootid"), QStringLiteral("/")});
  bool currentRootValid = false;
  const int currentRootId =
      currentRoot.output.trimmed().toInt(&currentRootValid);

  progress("Comparing with Snapper’s records…");
  const CommandResult snapper = readSnapperNumbers();
  if (snapper.exitCode != 0)
    return fail(
        QStringLiteral("Snapper would not provide its recovery records: %1")
            .arg(snapper.error));

  const QSet<int> managed = RecoveryAudit::parseSnapperNumbers(snapper.output);
  QSet<int> actualNumbers;
  for (const auto &snapshot : actual)
    actualNumbers.insert(snapshot.snapshotNumber);

  QJsonArray findings;
  qulonglong potentialBytes = 0;
  for (const auto &snapshot : actual) {
    if (managed.contains(snapshot.snapshotNumber))
      continue;

    progress("Measuring an unmanaged recovery copy…");
    const QString mountedPath =
        QStringLiteral("/.snapshots/%1/snapshot").arg(snapshot.snapshotNumber);
    const CommandResult measured =
        run(QStringLiteral("btrfs"),
            {QStringLiteral("filesystem"), QStringLiteral("du"),
             QStringLiteral("-s"), QStringLiteral("--raw"), mountedPath},
            120000);
    const RecoveryAudit::SpaceUsage usage =
        measured.exitCode == 0 ? RecoveryAudit::parseSpaceUsage(measured.output)
                               : RecoveryAudit::SpaceUsage{};
    const CommandResult shown =
        run(QStringLiteral("btrfs"),
            {QStringLiteral("subvolume"), QStringLiteral("show"), mountedPath});
    const RecoveryAudit::SubvolumeDetails details =
        shown.exitCode == 0 ? RecoveryAudit::parseSubvolumeDetails(shown.output)
                            : RecoveryAudit::SubvolumeDetails{};
    const bool nestedSubvolume = std::any_of(
        allSubvolumes.cbegin(), allSubvolumes.cend(),
        [&snapshot](const auto &item) {
          return item.path.startsWith(snapshot.path + QLatin1Char('/'));
        });
    const CommandResult mounted = run(
        QStringLiteral("findmnt"),
        {QStringLiteral("--noheadings"), QStringLiteral("--output"),
         QStringLiteral("TARGET"), QStringLiteral("--target"), mountedPath});
    const bool exactlyMounted = mounted.output.trimmed() == mountedPath;
    const QFileInfo pathInfo(mountedPath);
    const bool pathSafe = pathInfo.isDir() && !pathInfo.isSymbolicLink() &&
                          (pathInfo.canonicalFilePath().isEmpty() ||
                           pathInfo.canonicalFilePath() == mountedPath);

    QJsonObject finding;
    finding.insert(QStringLiteral("kind"), QStringLiteral("unmanaged"));
    finding.insert(QStringLiteral("snapshotNumber"), snapshot.snapshotNumber);
    finding.insert(QStringLiteral("subvolumeId"), snapshot.subvolumeId);
    finding.insert(QStringLiteral("path"), mountedPath);
    const QFileInfo metadata(
        QStringLiteral("/.snapshots/%1/info.xml").arg(snapshot.snapshotNumber));
    finding.insert(QStringLiteral("metadata"),
                   !metadata.exists()     ? QStringLiteral("missing")
                   : metadata.size() == 0 ? QStringLiteral("empty")
                                          : QStringLiteral("present"));
    finding.insert(QStringLiteral("readOnly"), details.readOnly);
    finding.insert(QStringLiteral("creationTime"), details.creationTime);
    finding.insert(QStringLiteral("uuid"), details.uuid);
    finding.insert(QStringLiteral("nestedSubvolume"), nestedSubvolume);
    finding.insert(QStringLiteral("mounted"), exactlyMounted);
    finding.insert(QStringLiteral("currentRoot"),
                   currentRootValid && currentRootId == snapshot.subvolumeId);
    finding.insert(QStringLiteral("pathSafe"), pathSafe);
    finding.insert(QStringLiteral("remedyCandidate"),
                   details.valid && details.readOnly &&
                       details.subvolumeId == snapshot.subvolumeId &&
                       (!metadata.exists() || metadata.size() == 0) &&
                       currentRoot.exitCode == 0 && currentRootValid &&
                       currentRootId != snapshot.subvolumeId &&
                       !nestedSubvolume && !exactlyMounted && pathSafe);
    if (usage.valid) {
      finding.insert(QStringLiteral("totalBytes"),
                     static_cast<qint64>(usage.totalBytes));
      finding.insert(QStringLiteral("exclusiveBytes"),
                     static_cast<qint64>(usage.exclusiveBytes));
      finding.insert(QStringLiteral("sharedBytes"),
                     static_cast<qint64>(usage.sharedBytes));
      potentialBytes += usage.exclusiveBytes;
    }
    findings.push_back(finding);
  }

  for (const int snapshotNumber : managed) {
    if (actualNumbers.contains(snapshotNumber))
      continue;
    QJsonObject finding;
    finding.insert(QStringLiteral("kind"), QStringLiteral("missing-subvolume"));
    finding.insert(QStringLiteral("snapshotNumber"), snapshotNumber);
    findings.push_back(finding);
  }

  progress("Finishing the comparison…");
  QJsonObject result;
  result.insert(QStringLiteral("schema"), 1);
  result.insert(QStringLiteral("filesystem"), QStringLiteral("btrfs"));
  result.insert(QStringLiteral("actualSnapshots"), actual.size());
  result.insert(QStringLiteral("managedSnapshots"), managed.size());
  result.insert(QStringLiteral("potentialBytes"),
                static_cast<qint64>(potentialBytes));
  result.insert(QStringLiteral("findings"), findings);
  std::fputs(QJsonDocument(result).toJson(QJsonDocument::Compact).constData(),
             stdout);
  std::fputc('\n', stdout);
  return 0;
#endif
}
