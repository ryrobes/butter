#include "BtrfsBackend.h"
#include "AgentNarrator.h"
#include "HomeScanner.h"
#include "HomeShadow.h"
#include "RecoveryAudit.h"
#include "ShadowCommon.h"
#include "SpaceHistory.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

class BtrfsBackendTest : public QObject {
  Q_OBJECT

private slots:
  void parsesUsage();
  void sumsDeviceErrors();
  void parsesRecoveryMetadata();
  void parsesSnapshotInventory();
  void parsesSnapperNumbers();
  void parsesExclusiveSpace();
  void parsesSubvolumeDetails();
  void parsesAuditResult();
  void parsesDockerSizes();
  void parsesDockerAccounting();
  void classifiesRemovableArtifacts();
  void rejectsUnreviewedArtifactRemoval();
  void buildsShadowExclusions();
  void usesProcessIndependentShadowStatusPath();
  void recordsBoundedSpaceHistory();
  void validatesAgentNarration();
  void preservesShadowGenerations();
  void validatesShadowDestinations();
};

void BtrfsBackendTest::parsesUsage() {
  const QString text = QStringLiteral(R"(
Overall:
    Device size: 998037782528
    Used: 907193647104
    Free (estimated): 33428930560 (min: 33428930560)
Data,single: Size:905678159872, Used:872249229312 (96.31%)
)");
  const auto usage = BtrfsBackend::parseUsage(text);
  QCOMPARE(usage.deviceSize, 998037782528ULL);
  QCOMPARE(usage.used, 907193647104ULL);
  QCOMPARE(usage.estimatedFree, 33428930560ULL);
  QCOMPARE(usage.dataSize, 905678159872ULL);
  QCOMPARE(usage.dataUsed, 872249229312ULL);
}

void BtrfsBackendTest::sumsDeviceErrors() {
  const QString text = QStringLiteral(R"(
[/dev/mapper/root].write_io_errs 0
[/dev/mapper/root].read_io_errs 2
[/dev/mapper/root].flush_io_errs 0
[/dev/mapper/root].corruption_errs 1
[/dev/mapper/root].generation_errs 0
)");
  QCOMPARE(BtrfsBackend::parseDeviceErrors(text), 3);
}

void BtrfsBackendTest::parsesRecoveryMetadata() {
  const QByteArray json = R"({
    "snapshotEntries": [
      {
        "snapperID": {
          "snapshotID": 44,
          "timestamp": "2026-08-26 23:32:14",
          "properties": { "description": "4.0.0-1" }
        },
        "kernelEntries": [{}]
      },
      {
        "snapperID": {
          "snapshotID": 43,
          "timestamp": "2026-08-25 06:13:26",
          "properties": { "description": "4.0.0-1" }
        },
        "kernelEntries": []
      }
    ]
  })";
  const QVariantList points = BtrfsBackend::parseRecoveryPoints(json);
  QCOMPARE(points.size(), 2);
  QCOMPARE(points.first().toMap().value(QStringLiteral("id")).toInt(), 44);
  QCOMPARE(
      points.first().toMap().value(QStringLiteral("description")).toString(),
      QStringLiteral("4.0.0-1"));
  QVERIFY(points.first().toMap().value(QStringLiteral("bootEntry")).toBool());
  QVERIFY(!points.last().toMap().value(QStringLiteral("bootEntry")).toBool());
}

void BtrfsBackendTest::parsesSnapshotInventory() {
  const QString text = QStringLiteral(R"(
ID      gen     top level       path
256     123     5               @
810     456     256             @/.snapshots/29/snapshot
940     789     256             @/.snapshots/44/snapshot
999     800     256             @/home
)");
  const auto snapshots = RecoveryAudit::parseSnapshotSubvolumes(text);
  const auto all = RecoveryAudit::parseSubvolumes(text);
  QCOMPARE(all.size(), 4);
  QCOMPARE(all.last().path, QStringLiteral("@/home"));
  QCOMPARE(snapshots.size(), 2);
  QCOMPARE(snapshots.first().snapshotNumber, 29);
  QCOMPARE(snapshots.first().subvolumeId, 810);
  QCOMPARE(snapshots.last().snapshotNumber, 44);
}

void BtrfsBackendTest::parsesSnapperNumbers() {
  const QString text = QStringLiteral("0\n\"40\"\n41\n44\n");
  const QSet<int> numbers = RecoveryAudit::parseSnapperNumbers(text);
  QCOMPARE(numbers, QSet<int>({40, 41, 44}));
}

void BtrfsBackendTest::parsesExclusiveSpace() {
  const QString text = QStringLiteral(R"(
     Total   Exclusive  Set shared  Filename
60473139200 27992449024 28872880128  /.snapshots/29/snapshot
)");
  const auto usage = RecoveryAudit::parseSpaceUsage(text);
  QVERIFY(usage.valid);
  QCOMPARE(usage.totalBytes, 60473139200ULL);
  QCOMPARE(usage.exclusiveBytes, 27992449024ULL);
  QCOMPARE(usage.sharedBytes, 28872880128ULL);
}

void BtrfsBackendTest::parsesSubvolumeDetails() {
  const QString text = QStringLiteral(R"(
/.snapshots/29/snapshot
        Name:                   snapshot
        UUID:                   3cfce0f4-93ad-b14c-aac1-acde48001122
        Parent UUID:            12345678-93ad-b14c-aac1-acde48001122
        Creation time:          2025-10-22 17:05:45 -0400
        Subvolume ID:           256
        Generation:             12345
        Flags:                  readonly
)");
  const auto details = RecoveryAudit::parseSubvolumeDetails(text);
  QVERIFY(details.valid);
  QVERIFY(details.readOnly);
  QCOMPARE(details.subvolumeId, 256);
  QCOMPARE(details.creationTime, QStringLiteral("2025-10-22 17:05:45 -0400"));
  QCOMPARE(details.uuid,
           QStringLiteral("3cfce0f4-93ad-b14c-aac1-acde48001122"));
}

void BtrfsBackendTest::parsesAuditResult() {
  const QByteArray json = R"({
    "schema": 1,
    "filesystem": "btrfs",
    "actualSnapshots": 6,
    "managedSnapshots": 5,
    "potentialBytes": 27992449024,
    "findings": [{
      "kind": "unmanaged",
      "snapshotNumber": 29,
      "subvolumeId": 810,
      "exclusiveBytes": 27992449024
    }]
  })";
  QString error;
  int actual = 0;
  int managed = 0;
  qulonglong potential = 0;
  const QVariantList findings = BtrfsBackend::parseAuditFindings(
      json, &error, &actual, &managed, &potential);
  QVERIFY(error.isEmpty());
  QCOMPARE(actual, 6);
  QCOMPARE(managed, 5);
  QCOMPARE(potential, 27992449024ULL);
  QCOMPARE(findings.size(), 1);
  QCOMPARE(
      findings.first().toMap().value(QStringLiteral("snapshotNumber")).toInt(),
      29);
}

void BtrfsBackendTest::parsesDockerSizes() {
  QCOMPARE(HomeScanner::parseHumanBytes(QStringLiteral("331.3MB (100%)")),
           331300000ULL);
  QCOMPARE(HomeScanner::parseHumanBytes(QStringLiteral("92.03GB")),
           92030000000ULL);
  QCOMPARE(HomeScanner::parseHumanBytes(QStringLiteral("1.5TB")),
           1500000000000ULL);
}

void BtrfsBackendTest::parsesDockerAccounting() {
  const QByteArray report = R"json(
{"Active":"6","Reclaimable":"24.52GB (47%)","Size":"51.52GB","TotalCount":"176","Type":"Images"}
{"Active":"7","Reclaimable":"1.753GB (46%)","Size":"3.804GB","TotalCount":"24","Type":"Local Volumes"}
{"Active":"9","Reclaimable":"92.03GB","Size":"122.9GB","TotalCount":"2419","Type":"Build Cache"}
)json";
  const QVariantList findings = HomeScanner::parseDockerReport(report);
  QCOMPARE(findings.size(), 3);
  QCOMPARE(findings.first().toMap().value(QStringLiteral("type")).toString(),
           QStringLiteral("Build Cache"));
  QCOMPARE(findings.first()
               .toMap()
               .value(QStringLiteral("reclaimableBytes"))
               .toULongLong(),
           92030000000ULL);
  QCOMPARE(findings.last().toMap().value(QStringLiteral("safety")).toString(),
           QStringLiteral("protected"));
}

void BtrfsBackendTest::classifiesRemovableArtifacts() {
  QVERIFY(HomeScanner::isRemovableArtifactType(QStringLiteral("rust")));
  QVERIFY(HomeScanner::isRemovableArtifactType(QStringLiteral("node")));
  QVERIFY(HomeScanner::isRemovableArtifactType(QStringLiteral("npm-cache")));
  QVERIFY(!HomeScanner::isRemovableArtifactType(QStringLiteral("python")));
  QVERIFY(!HomeScanner::isRemovableArtifactType(QStringLiteral("unknown")));
}

void BtrfsBackendTest::rejectsUnreviewedArtifactRemoval() {
  HomeScanner scanner;
  scanner.removeArtifact(QStringLiteral("/"), QStringLiteral("rust"));
  QCOMPARE(scanner.cleanupState(), QStringLiteral("error"));
  QCOMPARE(scanner.cleanupTitle(), QStringLiteral("Butter stopped safely"));
}

void BtrfsBackendTest::buildsShadowExclusions() {
  QTemporaryDir source;
  QVERIFY(source.isValid());
  const QString target =
      QDir(source.path()).filePath(QStringLiteral("app/target"));
  const QString environment =
      QDir(source.path()).filePath(QStringLiteral("app/.venv"));
  QVariantList findings = {
      QVariantMap{{QStringLiteral("path"), target},
                  {QStringLiteral("safety"), QStringLiteral("regenerable")}},
      QVariantMap{{QStringLiteral("path"), environment},
                  {QStringLiteral("safety"), QStringLiteral("review")}}};
  const QStringList excludes =
      ShadowCommon::excludesFromFindings(findings, source.path());
  QVERIFY(excludes.contains(QStringLiteral(".cache")));
  QVERIFY(excludes.contains(QStringLiteral("**/__pycache__")));
  QVERIFY(excludes.contains(QStringLiteral("**/.pytest_cache")));
  QVERIFY(excludes.contains(QStringLiteral(".local/share/pnpm/store")));
  QVERIFY(excludes.contains(QStringLiteral("go/pkg/mod/cache")));
  QVERIFY(excludes.contains(QStringLiteral("app/target")));
  QVERIFY(!excludes.contains(QStringLiteral("app/.venv")));
}

void BtrfsBackendTest::usesProcessIndependentShadowStatusPath() {
  const QString originalName = QCoreApplication::applicationName();
  QCoreApplication::setApplicationName(QStringLiteral("butter"));
  const QString appPath = ShadowCommon::statusPath();
  const QString appHistoryPath = SpaceHistory::historyPath();
  QCoreApplication::setApplicationName(QStringLiteral("butter-shadow"));
  const QString helperPath = ShadowCommon::statusPath();
  const QString helperHistoryPath = SpaceHistory::historyPath();
  QCoreApplication::setApplicationName(originalName);
  QCOMPARE(appPath, helperPath);
  QCOMPARE(appHistoryPath, helperHistoryPath);
  QVERIFY(appPath.endsWith(QStringLiteral("/butter/home-shadow-status.json")));
}

void BtrfsBackendTest::recordsBoundedSpaceHistory() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path =
      QDir(directory.path()).filePath(QStringLiteral("space-history.json"));
  const QDateTime now = QDateTime::fromString(
      QStringLiteral("2026-08-29T12:00:00Z"), Qt::ISODate);
  QVERIFY(SpaceHistory::record(500'000'000, 1'000'000'000,
                               QStringLiteral("old"), path, now.addDays(-371)));
  QVERIFY(SpaceHistory::record(400'000'000, 1'000'000'000,
                               QStringLiteral("app"), path, now));
  QVERIFY(SpaceHistory::record(390'000'000, 1'000'000'000,
                               QStringLiteral("shadow"), path,
                               now.addSecs(5 * 60)));
  QVariantList samples = SpaceHistory::read(path);
  QCOMPARE(samples.size(), 1);
  QCOMPARE(
      samples.first().toMap().value(QStringLiteral("freeBytes")).toULongLong(),
      390'000'000ULL);
  QCOMPARE(samples.first()
               .toMap()
               .value(QStringLiteral("freeMegabytes"))
               .toULongLong(),
           390ULL);
  QCOMPARE(samples.first().toMap().value(QStringLiteral("source")).toString(),
           QStringLiteral("shadow"));

  QVERIFY(SpaceHistory::record(300'000'000, 1'000'000'000,
                               QStringLiteral("app"), path, now.addDays(1)));
  samples = SpaceHistory::read(path);
  QCOMPARE(samples.size(), 2);
  QCOMPARE(samples.last().toMap().value(QStringLiteral("freeRatio")).toDouble(),
           0.3);
}

void BtrfsBackendTest::validatesAgentNarration() {
  const auto valid = AgentNarrator::parseResponse(QByteArrayLiteral(
      R"({"headline":"Your first Shadow is underway","summary":"The filesystem is healthy, and Butter is making its first protected Home copy now.","trendDirection":"down"})"));
  QVERIFY(valid.has_value());
  QCOMPARE(valid->headline, QStringLiteral("Your first Shadow is underway"));
  QCOMPARE(valid->summary,
           QStringLiteral("The filesystem is healthy, and Butter is making "
                          "its first protected Home copy now."));
  QCOMPARE(valid->trendDirection, QStringLiteral("down"));
  QVERIFY(!AgentNarrator::contradictsTrend(*valid, QStringLiteral("down")));

  const AgentNarrator::Response contradiction{
      QStringLiteral("Free space is trending upward"),
      QStringLiteral("The available space increased by 15 GB."),
      QStringLiteral("down")};
  QVERIFY(
      AgentNarrator::contradictsTrend(contradiction, QStringLiteral("down")));

  const auto simplified = AgentNarrator::parseResponse(QByteArrayLiteral(
      "model output: {\"headline\":\"Space is moving\",\"summary\":\"Free "
      "space changed across\\nrecent observations, but no device errors were "
      "reported.\",\"trendDirection\":\"up\"}"));
  QVERIFY(simplified.has_value());
  QVERIFY(!simplified->summary.contains(QLatin1Char('\n')));

  QVERIFY(
      !AgentNarrator::parseResponse(
           QByteArrayLiteral(
               R"({"headline":"Okay","summary":"short","trendDirection":"flat"})"))
           .has_value());
  QVERIFY(
      !AgentNarrator::parseResponse(
           QByteArrayLiteral(
               R"({"headline":"Space changed","summary":"Free space moved recently without device errors.","trendDirection":"sideways"})"))
           .has_value());
}

void BtrfsBackendTest::preservesShadowGenerations() {
  if (!QFileInfo::exists(QStringLiteral("/dev/shm")) ||
      QStandardPaths::findExecutable(QStringLiteral("rsync")).isEmpty())
    QSKIP("A distinct temporary filesystem and rsync are required.");

  QTemporaryDir source;
  QTemporaryDir destination(
      QStringLiteral("/dev/shm/butter-shadow-test-XXXXXX"));
  QVERIFY(source.isValid());
  QVERIFY(destination.isValid());
  const QString shadowRoot =
      QDir(destination.path()).filePath(QStringLiteral(".butter-shadow"));
  QVERIFY(
      QDir().mkpath(QDir(shadowRoot).filePath(QStringLiteral("generations"))));
  QVERIFY(
      QDir().mkpath(QDir(shadowRoot).filePath(QStringLiteral("incomplete"))));
  QVERIFY(QDir().mkpath(QDir(shadowRoot).filePath(QStringLiteral("receipts"))));
  QVERIFY(
      QDir().mkpath(QDir(source.path()).filePath(QStringLiteral(".cache"))));
  QVERIFY(QDir().mkpath(
      QDir(source.path()).filePath(QStringLiteral("project/__pycache__"))));

  const auto writeFile = [](const QString &path, const QByteArray &contents) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) &&
           file.write(contents) == contents.size();
  };
  QVERIFY(writeFile(QDir(source.path()).filePath(QStringLiteral("kept.txt")),
                    QByteArrayLiteral("first")));
  QVERIFY(writeFile(QDir(source.path()).filePath(QStringLiteral("changed.txt")),
                    QByteArrayLiteral("one")));
  QVERIFY(
      writeFile(QDir(source.path()).filePath(QStringLiteral("unchanged.txt")),
                QByteArrayLiteral("same")));
  QVERIFY(
      writeFile(QDir(source.path()).filePath(QStringLiteral(".cache/noise")),
                QByteArrayLiteral("skip")));
  QVERIFY(writeFile(
      QDir(source.path()).filePath(QStringLiteral("project/__pycache__/noise")),
      QByteArrayLiteral("skip recursively")));

  const ShadowCommon::MountInfo mount =
      ShadowCommon::mountInfo(destination.path());
  QVERIFY(mount.valid);
  ShadowCommon::Config config;
  config.sourcePath = ShadowCommon::normalized(source.path());
  config.destinationPath = ShadowCommon::normalized(destination.path());
  config.mountUuid = mount.uuid;
  config.mountSource = mount.source;
  config.filesystem = mount.filesystem;
  config.machineId = ShadowCommon::machineId();
  config.createdAt = QStringLiteral("2026-08-29T00:00:00Z");
  config.excludes = ShadowCommon::baselineExcludes();
  const QString configPath =
      QDir(source.path()).filePath(QStringLiteral("config.json"));
  const QString statusPath =
      QDir(source.path()).filePath(QStringLiteral("status.json"));
  const QString historyPath =
      QDir(source.path()).filePath(QStringLiteral("history.json"));
  QVERIFY(ShadowCommon::saveJson(configPath, ShadowCommon::toJson(config),
                                 nullptr));
  QVERIFY(ShadowCommon::saveJson(
      QDir(shadowRoot).filePath(QStringLiteral("identity.json")),
      ShadowCommon::toJson(config), nullptr));

  const QString helper = QDir(QCoreApplication::applicationDirPath())
                             .filePath(QStringLiteral("butter-shadow"));
  QTemporaryDir fakeTools;
  QVERIFY(fakeTools.isValid());
  const QString fakeRsync =
      QDir(fakeTools.path()).filePath(QStringLiteral("rsync"));
  QVERIFY(writeFile(
      fakeRsync, QByteArrayLiteral(
                     "#!/bin/sh\n"
                     "echo 'rsync: [receiver] mkstemp \"/backup/file\" failed: "
                     "Permission denied (13)' >&2\n"
                     "exit 23\n")));
  QVERIFY(QFile::setPermissions(fakeRsync, QFileDevice::ReadOwner |
                                               QFileDevice::WriteOwner |
                                               QFileDevice::ExeOwner));
  auto run = [&](int expectedExitCode = 0, bool forceFailure = false) {
    QProcess process;
    if (forceFailure) {
      QProcessEnvironment environment =
          QProcessEnvironment::systemEnvironment();
      environment.insert(QStringLiteral("PATH"),
                         fakeTools.path() + QDir::listSeparator() +
                             environment.value(QStringLiteral("PATH")));
      process.setProcessEnvironment(environment);
    }
    process.start(helper, {QStringLiteral("--run"), QStringLiteral("--config"),
                           configPath, QStringLiteral("--status"), statusPath,
                           QStringLiteral("--history"), historyPath});
    QVERIFY(process.waitForFinished(30000));
    QCOMPARE(process.exitCode(), expectedExitCode);
  };
  run();
  QStringList generations =
      QDir(QDir(shadowRoot).filePath(QStringLiteral("generations")))
          .entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  QCOMPARE(generations.size(), 1);
  const QString first =
      QDir(shadowRoot)
          .filePath(QStringLiteral("generations/") + generations.first());
  QVERIFY(QFileInfo::exists(QDir(first).filePath(QStringLiteral("kept.txt"))));
  QVERIFY(
      !QFileInfo::exists(QDir(first).filePath(QStringLiteral(".cache/noise"))));
  QVERIFY(!QFileInfo::exists(
      QDir(first).filePath(QStringLiteral("project/__pycache__/noise"))));
  QFile firstReceipt(QDir(shadowRoot)
                         .filePath(QStringLiteral("receipts/") +
                                   generations.first() +
                                   QStringLiteral(".json")));
  QVERIFY(firstReceipt.open(QIODevice::ReadOnly));
  const QJsonObject firstReceiptJson =
      QJsonDocument::fromJson(firstReceipt.readAll()).object();
  QVERIFY(firstReceiptJson.value(QStringLiteral("sourceBytes")).toInteger() >
          100);

  QVERIFY(
      QFile::remove(QDir(source.path()).filePath(QStringLiteral("kept.txt"))));
  QVERIFY(writeFile(QDir(source.path()).filePath(QStringLiteral("changed.txt")),
                    QByteArrayLiteral("two")));
  run();
  generations = QDir(QDir(shadowRoot).filePath(QStringLiteral("generations")))
                    .entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  QCOMPARE(generations.size(), 2);
  const QString second =
      QDir(shadowRoot)
          .filePath(QStringLiteral("generations/") + generations.last());
  QVERIFY(QFileInfo::exists(QDir(first).filePath(QStringLiteral("kept.txt"))));
  QVERIFY(
      !QFileInfo::exists(QDir(second).filePath(QStringLiteral("kept.txt"))));
  QFile changed(QDir(second).filePath(QStringLiteral("changed.txt")));
  QVERIFY(changed.open(QIODevice::ReadOnly));
  QCOMPARE(changed.readAll(), QByteArrayLiteral("two"));
  struct stat firstUnchanged{};
  struct stat secondUnchanged{};
  const QByteArray firstPath =
      QFile::encodeName(QDir(first).filePath(QStringLiteral("unchanged.txt")));
  const QByteArray secondPath =
      QFile::encodeName(QDir(second).filePath(QStringLiteral("unchanged.txt")));
  QCOMPARE(::stat(firstPath.constData(), &firstUnchanged), 0);
  QCOMPARE(::stat(secondPath.constData(), &secondUnchanged), 0);
  QCOMPARE(firstUnchanged.st_ino, secondUnchanged.st_ino);

  const QString blockedPath =
      QDir(source.path()).filePath(QStringLiteral("blocked.txt"));
  QVERIFY(writeFile(blockedPath, QByteArrayLiteral("finish me")));
  QVERIFY(QFile::setPermissions(blockedPath, {}));
  run();

  QFile skippedStatus(statusPath);
  QVERIFY(skippedStatus.open(QIODevice::ReadOnly));
  const QJsonObject skipped =
      QJsonDocument::fromJson(skippedStatus.readAll()).object();
  QCOMPARE(skipped.value(QStringLiteral("state")).toString(),
           QStringLiteral("current"));
  QCOMPARE(skipped.value(QStringLiteral("title")).toString(),
           QStringLiteral("Home Shadow is current"));
  QCOMPARE(skipped.value(QStringLiteral("skippedPathCount")).toInt(), 1);
  QCOMPARE(
      skipped.value(QStringLiteral("skippedAccessErrors")).toArray().size(), 1);

  QVERIFY(QFile::setPermissions(blockedPath, QFileDevice::ReadOwner |
                                                 QFileDevice::WriteOwner));

  const QString incompletePath =
      QDir(shadowRoot).filePath(QStringLiteral("incomplete/manual-resume"));
  QVERIFY(
      QDir().mkpath(QDir(incompletePath).filePath(QStringLiteral(".cache"))));
  QVERIFY(QDir().mkpath(
      QDir(incompletePath).filePath(QStringLiteral("work/.pytest_cache"))));
  QVERIFY(
      writeFile(QDir(incompletePath).filePath(QStringLiteral(".cache/stale")),
                QByteArrayLiteral("excluded")));
  QVERIFY(writeFile(
      QDir(incompletePath).filePath(QStringLiteral("ordinary-stale.txt")),
      QByteArrayLiteral("preserve")));
  QVERIFY(writeFile(
      QDir(incompletePath).filePath(QStringLiteral("work/.pytest_cache/stale")),
      QByteArrayLiteral("excluded recursively")));
  QVERIFY(ShadowCommon::saveStatus(
      statusPath, QStringLiteral("error"), QStringLiteral("test failure"),
      QStringLiteral("test failure"),
      {{QStringLiteral("incompletePath"), incompletePath}}));
  run(1, true);
  QFile failedStatus(statusPath);
  QVERIFY(failedStatus.open(QIODevice::ReadOnly));
  const QJsonObject failure =
      QJsonDocument::fromJson(failedStatus.readAll()).object();
  QCOMPARE(failure.value(QStringLiteral("state")).toString(),
           QStringLiteral("error"));
  QCOMPARE(failure.value(QStringLiteral("rsyncExitCode")).toInt(), 23);
  QVERIFY(!failure.value(QStringLiteral("errorLines")).toArray().isEmpty());
  QCOMPARE(failure.value(QStringLiteral("incompletePath")).toString(),
           incompletePath);

  run();
  QVERIFY(!QFileInfo::exists(incompletePath));
  const QString resumedGeneration =
      QDir(shadowRoot)
          .filePath(QStringLiteral("generations/") +
                    QFileInfo(incompletePath).fileName());
  QVERIFY(QFileInfo(resumedGeneration).isDir());
  QVERIFY(!QFileInfo::exists(
      QDir(resumedGeneration).filePath(QStringLiteral(".cache/stale"))));
  QVERIFY(!QFileInfo::exists(
      QDir(resumedGeneration)
          .filePath(QStringLiteral("work/.pytest_cache/stale"))));
  QVERIFY(QFileInfo::exists(
      QDir(resumedGeneration).filePath(QStringLiteral("ordinary-stale.txt"))));
  QFile completedStatus(statusPath);
  QVERIFY(completedStatus.open(QIODevice::ReadOnly));
  const QJsonObject completion =
      QJsonDocument::fromJson(completedStatus.readAll()).object();
  QCOMPARE(completion.value(QStringLiteral("state")).toString(),
           QStringLiteral("current"));
  QVERIFY(completion.value(QStringLiteral("resumed")).toBool());

  const QString mixedIncompletePath =
      QDir(shadowRoot).filePath(QStringLiteral("incomplete/manual-mixed"));
  QVERIFY(QDir().mkpath(mixedIncompletePath));
  QVERIFY(writeFile(QDir(mixedIncompletePath)
                        .filePath(QStringLiteral("copied-before-warning.txt")),
                    QByteArrayLiteral("keep the partial work")));
  QVERIFY(ShadowCommon::saveStatus(
      statusPath, QStringLiteral("error"), QStringLiteral("mixed source skip"),
      QStringLiteral("mixed source skip"),
      {{QStringLiteral("incompletePath"), mixedIncompletePath}}));
  QVERIFY(writeFile(
      fakeRsync,
      QByteArrayLiteral(
          "#!/bin/sh\n"
          "echo 'file has vanished: \"/home/test/catalog.sqlite-shm\"' >&2\n"
          "echo 'file has vanished: \"/home/test/catalog.sqlite-wal\"' >&2\n"
          "echo 'rsync: [sender] opendir \"/home/test/volumes/db\" failed: "
          "Permission denied (13)' >&2\n"
          "echo 'rsync: [sender] opendir \"/home/test/old/volumes/db\" failed: "
          "Permission denied (13)' >&2\n"
          "echo 'rsync error: some files/attrs were not transferred (see "
          "previous errors) (code 23) at main.c(1338) [sender=3.4.1]' >&2\n"
          "exit 23\n")));
  QVERIFY(QFile::setPermissions(fakeRsync, QFileDevice::ReadOwner |
                                               QFileDevice::WriteOwner |
                                               QFileDevice::ExeOwner));
  run(0, true);
  QVERIFY(!QFileInfo::exists(mixedIncompletePath));
  const QString mixedGeneration =
      QDir(shadowRoot)
          .filePath(QStringLiteral("generations/") +
                    QFileInfo(mixedIncompletePath).fileName());
  QVERIFY(QFileInfo(mixedGeneration).isDir());
  QVERIFY(QFileInfo::exists(
      QDir(mixedGeneration)
          .filePath(QStringLiteral("copied-before-warning.txt"))));
  QFile mixedStatus(statusPath);
  QVERIFY(mixedStatus.open(QIODevice::ReadOnly));
  const QJsonObject mixed =
      QJsonDocument::fromJson(mixedStatus.readAll()).object();
  QCOMPARE(mixed.value(QStringLiteral("state")).toString(),
           QStringLiteral("current"));
  QCOMPARE(mixed.value(QStringLiteral("skippedPathCount")).toInt(), 2);
  QCOMPARE(mixed.value(QStringLiteral("skippedAccessErrors")).toArray().size(),
           2);
  QCOMPARE(mixed.value(QStringLiteral("sourceChangeCount")).toInt(), 2);
  QCOMPARE(mixed.value(QStringLiteral("sourceChangeWarnings")).toArray().size(),
           2);
  QVERIFY(mixed.value(QStringLiteral("resumed")).toBool());
}

void BtrfsBackendTest::validatesShadowDestinations() {
  HomeShadow shadow;
  const QVariantMap sameDrive =
      shadow.inspectDestination(QUrl::fromLocalFile(QDir::homePath()));
  QVERIFY(!sameDrive.value(QStringLiteral("valid")).toBool());

  if (!QFileInfo(QStringLiteral("/backup")).isWritable())
    QSKIP("No writable second-drive fixture is mounted.");
  const QVariantMap secondDrive =
      shadow.inspectDestination(QUrl::fromLocalFile(QStringLiteral("/backup")));
  QVERIFY2(secondDrive.value(QStringLiteral("valid")).toBool(),
           qPrintable(secondDrive.value(QStringLiteral("detail")).toString()));
  QCOMPARE(secondDrive.value(QStringLiteral("filesystem")).toString(),
           QStringLiteral("ext4"));
}

QTEST_GUILESS_MAIN(BtrfsBackendTest)
#include "BtrfsBackendTest.moc"
