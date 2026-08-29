#include "ShadowCommon.h"
#include "SpaceHistory.h"

#include <algorithm>
#include <utility>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

#include <sys/stat.h>
#include <unistd.h>

namespace {

QString statusPath;

int finish(const QString &state, const QString &title, const QString &detail,
           const QJsonObject &extra = {}, int exitCode = 1) {
  ShadowCommon::saveStatus(statusPath, state, title, detail, extra);
  return exitCode;
}

bool differentDevice(const QString &source, const QString &destination) {
  struct stat sourceStat{};
  struct stat destinationStat{};
  return ::stat(QFile::encodeName(source).constData(), &sourceStat) == 0 &&
         ::stat(QFile::encodeName(destination).constData(), &destinationStat) ==
             0 &&
         sourceStat.st_dev != destinationStat.st_dev;
}

qulonglong statValue(const QByteArray &output, const QString &label) {
  const QRegularExpression expression(
      QStringLiteral("^%1:\\s*([0-9,]+)")
          .arg(QRegularExpression::escape(label)),
      QRegularExpression::MultilineOption);
  const QRegularExpressionMatch match =
      expression.match(QString::fromUtf8(output));
  QString number = match.captured(1);
  number.remove(QLatin1Char(','));
  return number.toULongLong();
}

QJsonObject readStatus(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return {};
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
  return document.isObject() ? document.object() : QJsonObject{};
}

QString resumablePath(const QString &statusFile,
                      const QString &incompleteRoot) {
  QJsonObject status = readStatus(statusFile);
  if (status.isEmpty() && statusFile == ShadowCommon::statusPath())
    status = readStatus(ShadowCommon::legacyStatusPath());
  if (status.value(QStringLiteral("state")).toString() !=
      QLatin1String("error"))
    return {};

  const QString candidate = ShadowCommon::normalized(
      status.value(QStringLiteral("incompletePath")).toString());
  const QString root = ShadowCommon::normalized(incompleteRoot);
  const QFileInfo info(candidate);
  if (candidate.isEmpty() || candidate == root ||
      !ShadowCommon::isInside(candidate, root) || !info.isDir() ||
      info.isSymLink())
    return {};
  return candidate;
}

QStringList substantiveErrors(const QString &standardError) {
  QStringList useful;
  for (QString line :
       standardError.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
    line = line.trimmed();
    if (line.isEmpty() || line.startsWith(QLatin1String("rsync error:")))
      continue;
    if (!useful.contains(line))
      useful.push_back(line);
  }
  return useful;
}

QStringList boundedErrors(const QStringList &errors) {
  QStringList useful = errors;
  if (useful.size() > 6)
    useful = useful.sliced(useful.size() - 6);
  for (QString &line : useful) {
    if (line.size() > 320)
      line = line.left(317) + QStringLiteral("...");
  }
  return useful;
}

bool isUnreadableSourcePath(const QString &error) {
  return error.startsWith(QLatin1String("rsync: [sender] ")) &&
         error.endsWith(QLatin1String("Permission denied (13)")) &&
         (error.contains(QLatin1String(" opendir ")) ||
          error.contains(QLatin1String(" send_files failed to open ")));
}

bool isVanishedSourcePath(const QString &error) {
  if (error.startsWith(QLatin1String("file has vanished: ")) ||
      error.startsWith(QLatin1String(
          "rsync warning: some files vanished before they could be ")))
    return true;
  return error.startsWith(QLatin1String("rsync: [sender] ")) &&
         error.endsWith(QLatin1String("No such file or directory (2)")) &&
         (error.contains(QLatin1String(" link_stat ")) ||
          error.contains(QLatin1String(" readlink_stat ")) ||
          error.contains(QLatin1String(" send_files failed to open ")));
}

bool removeIncompleteEntry(const QString &path) {
  const QFileInfo info(path);
  if (!info.exists() && !info.isSymLink())
    return true;
  if (info.isSymLink() || info.isFile())
    return QFile::remove(path);
  return info.isDir() && QDir(path).removeRecursively();
}

QStringList purgeExcludedFromIncomplete(const QString &stagingPath,
                                        const QStringList &excludes) {
  QStringList recursiveNames;
  QStringList exactPaths;
  for (const QString &exclude : excludes) {
    const QString clean = QDir::cleanPath(exclude);
    if (clean.isEmpty() || clean == QLatin1String(".") ||
        clean.startsWith(QLatin1String("../")) ||
        clean.startsWith(QLatin1Char('/')))
      continue;
    if (clean.startsWith(QLatin1String("**/"))) {
      const QString name = clean.mid(3);
      if (!name.isEmpty() && !name.contains(QLatin1Char('/')) &&
          !name.contains(QLatin1Char('*')) && !name.contains(QLatin1Char('?')))
        recursiveNames.push_back(name);
      continue;
    }
    if (!clean.contains(QLatin1Char('*')) && !clean.contains(QLatin1Char('?')))
      exactPaths.push_back(QDir(stagingPath).filePath(clean));
  }

  QDirIterator iterator(stagingPath,
                        QDir::AllEntries | QDir::Hidden | QDir::System |
                            QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
  while (iterator.hasNext()) {
    iterator.next();
    if (recursiveNames.contains(iterator.fileInfo().fileName()))
      exactPaths.push_back(iterator.filePath());
  }
  std::sort(
      exactPaths.begin(), exactPaths.end(),
      [](const QString &a, const QString &b) { return a.size() > b.size(); });
  exactPaths.removeDuplicates();

  QStringList failures;
  for (const QString &path : std::as_const(exactPaths)) {
    if (!removeIncompleteEntry(path))
      failures.push_back(path);
  }
  return failures;
}

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("butter-shadow"));
  const QStringList commandLine = app.arguments();
  if (!commandLine.contains(QStringLiteral("--run")))
    return 2;
  QString configPath = ShadowCommon::configPath();
  statusPath = ShadowCommon::statusPath();
  QString historyPath = SpaceHistory::historyPath();
  const int configOption = commandLine.indexOf(QStringLiteral("--config"));
  const int statusOption = commandLine.indexOf(QStringLiteral("--status"));
  const int historyOption = commandLine.indexOf(QStringLiteral("--history"));
  if (configOption >= 0 && configOption + 1 < commandLine.size())
    configPath = commandLine.at(configOption + 1);
  if (statusOption >= 0 && statusOption + 1 < commandLine.size())
    statusPath = commandLine.at(statusOption + 1);
  if (historyOption >= 0 && historyOption + 1 < commandLine.size())
    historyPath = commandLine.at(historyOption + 1);
  QLockFile lock(statusPath + QStringLiteral(".lock"));
  lock.setStaleLockTime(6 * 60 * 60 * 1000);
  if (!lock.tryLock(0))
    return finish(
        QStringLiteral("running"),
        QStringLiteral("Home Shadow is already updating"),
        QStringLiteral(
            "Butter kept the existing run and did not start another."),
        {}, 0);

  ShadowCommon::Config config;
  QString error;
  if (!ShadowCommon::loadConfig(configPath, &config, &error))
    return finish(QStringLiteral("error"),
                  QStringLiteral("Home Shadow is not configured"), error);
  if (config.machineId != ShadowCommon::machineId())
    return finish(
        QStringLiteral("error"),
        QStringLiteral("This configuration belongs to another computer"),
        QStringLiteral("Butter refused to reuse its drive identity."));

  const SpaceHistory::Capacity sourceCapacity =
      SpaceHistory::measure(config.sourcePath);
  if (sourceCapacity.valid)
    SpaceHistory::record(sourceCapacity.freeBytes, sourceCapacity.totalBytes,
                         QStringLiteral("shadow"), historyPath);

  const ShadowCommon::MountInfo mount =
      ShadowCommon::mountInfo(config.destinationPath);
  const QString shadowRoot =
      QDir(config.destinationPath).filePath(QStringLiteral(".butter-shadow"));
  const QString identityPath =
      QDir(shadowRoot).filePath(QStringLiteral("identity.json"));
  ShadowCommon::Config identity;
  if (!mount.valid || mount.uuid != config.mountUuid ||
      !ShadowCommon::loadConfig(identityPath, &identity, &error) ||
      identity.machineId != config.machineId ||
      identity.destinationPath != config.destinationPath ||
      !differentDevice(config.sourcePath, config.destinationPath)) {
    return finish(QStringLiteral("waiting"),
                  QStringLiteral("Waiting for the Shadow drive"),
                  QStringLiteral(
                      "The recorded drive is not mounted at the reviewed path. "
                      "Nothing was written."),
                  {}, 0);
  }

  const QString rsync = QStandardPaths::findExecutable(QStringLiteral("rsync"));
  if (rsync.isEmpty())
    return finish(QStringLiteral("error"),
                  QStringLiteral("rsync is unavailable"),
                  QStringLiteral("Install rsync before running Home Shadow."));

  const QString generations =
      QDir(shadowRoot).filePath(QStringLiteral("generations"));
  const QString incomplete =
      QDir(shadowRoot).filePath(QStringLiteral("incomplete"));
  const QString receipts =
      QDir(shadowRoot).filePath(QStringLiteral("receipts"));
  if (!QDir().mkpath(generations) || !QDir().mkpath(incomplete) ||
      !QDir().mkpath(receipts))
    return finish(
        QStringLiteral("error"),
        QStringLiteral("The Shadow drive is not writable"),
        QStringLiteral("Butter could not create its generation folders."));

  QString stagingPath = resumablePath(statusPath, incomplete);
  const bool resumed = !stagingPath.isEmpty();
  if (!resumed) {
    const QString stamp = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyy-MM-ddTHHmmssZ"));
    stagingPath =
        QDir(incomplete)
            .filePath(stamp + QLatin1Char('-') +
                      QString::number(QCoreApplication::applicationPid()));
  }
  const QString generationName = QFileInfo(stagingPath).fileName();
  const QString generationPath = QDir(generations).filePath(generationName);
  if (!QDir().mkpath(stagingPath))
    return finish(
        QStringLiteral("error"),
        QStringLiteral("A new generation could not start"),
        QStringLiteral("Earlier completed generations remain untouched."));

  const QString startedAt =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  ShadowCommon::saveStatus(
      statusPath, QStringLiteral("running"),
      resumed ? QStringLiteral("Resuming Home Shadow")
              : QStringLiteral("Updating Home Shadow"),
      resumed ? QStringLiteral("Completing the preserved incomplete copy.")
              : QStringLiteral("Building a new immutable generation."),
      {{QStringLiteral("startedAt"), startedAt},
       {QStringLiteral("incompletePath"), stagingPath},
       {QStringLiteral("resumed"), resumed}});

  if (resumed) {
    const QStringList purgeFailures =
        purgeExcludedFromIncomplete(stagingPath, config.excludes);
    if (!purgeFailures.isEmpty())
      return finish(
          QStringLiteral("error"),
          QStringLiteral("Home Shadow could not clean its unfinished copy"),
          QStringLiteral("A newly excluded cache path could not be removed; "
                         "the incomplete generation was kept."),
          {{QStringLiteral("startedAt"), startedAt},
           {QStringLiteral("incompletePath"), stagingPath},
           {QStringLiteral("resumed"), true}});
  }

  QStringList arguments = {QStringLiteral("--archive"),
                           QStringLiteral("--hard-links"),
                           QStringLiteral("--acls"),
                           QStringLiteral("--xattrs"),
                           QStringLiteral("--one-file-system"),
                           QStringLiteral("--modify-window=-1"),
                           QStringLiteral("--partial"),
                           QStringLiteral("--stats")};
  for (const QString &exclude : config.excludes) {
    QString clean = QDir::cleanPath(exclude);
    if (clean.isEmpty() || clean == QLatin1String(".") ||
        clean.startsWith(QLatin1String("../")) ||
        clean.startsWith(QLatin1Char('/')))
      continue;
    arguments.push_back(QStringLiteral("--exclude=/%1/***").arg(clean));
  }

  const QString currentPath =
      QDir(shadowRoot).filePath(QStringLiteral("current"));
  const QString previous = QFileInfo(currentPath).canonicalFilePath();
  if (!previous.isEmpty() && ShadowCommon::isInside(previous, generations))
    arguments.push_back(QStringLiteral("--link-dest=%1").arg(previous));
  arguments.push_back(config.sourcePath + QLatin1Char('/'));
  arguments.push_back(stagingPath + QLatin1Char('/'));

  QProcess process;
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  process.setProcessEnvironment(environment);
  process.setProgram(rsync);
  process.setArguments(arguments);
  process.start();
  if (!process.waitForStarted(10000))
    return finish(
        QStringLiteral("error"), QStringLiteral("Home Shadow could not start"),
        QStringLiteral(
            "The incomplete generation was preserved for inspection."));
  process.waitForFinished(-1);
  const QByteArray output = process.readAllStandardOutput();
  const QString standardError =
      QString::fromUtf8(process.readAllStandardError()).trimmed();
  const QStringList allErrors = substantiveErrors(standardError);
  const bool exitedForChangingSource =
      process.exitStatus() == QProcess::NormalExit && process.exitCode() == 24;
  const bool onlySkippableSourceErrors =
      process.exitStatus() == QProcess::NormalExit &&
      process.exitCode() == 23 && !allErrors.isEmpty() &&
      std::all_of(
          allErrors.cbegin(), allErrors.cend(), [](const QString &line) {
            return isUnreadableSourcePath(line) || isVanishedSourcePath(line);
          });
  if (process.exitStatus() != QProcess::NormalExit ||
      (process.exitCode() != 0 && !exitedForChangingSource &&
       !onlySkippableSourceErrors)) {
    const QStringList errorLines = boundedErrors(allErrors);
    QJsonArray errors;
    for (const QString &line : errorLines)
      errors.push_back(line);
    return finish(
        QStringLiteral("error"),
        QStringLiteral("Home Shadow stopped before completion"),
        errorLines.isEmpty()
            ? QStringLiteral("The incomplete generation was kept; the last "
                             "successful generation is still current.")
            : errorLines.constLast(),
        {{QStringLiteral("startedAt"), startedAt},
         {QStringLiteral("incompletePath"), stagingPath},
         {QStringLiteral("resumed"), resumed},
         {QStringLiteral("rsyncExitCode"), process.exitCode()},
         {QStringLiteral("errorLines"), errors}});
  }

  if (!QDir().rename(stagingPath, generationPath))
    return finish(QStringLiteral("error"),
                  QStringLiteral("The completed copy could not be promoted"),
                  QStringLiteral("It remains in the incomplete area and no "
                                 "earlier generation was changed."));

  const QString temporaryLink =
      QDir(shadowRoot)
          .filePath(QStringLiteral(".current-%1")
                        .arg(QCoreApplication::applicationPid()));
  const QByteArray linkTarget =
      QFile::encodeName(QStringLiteral("generations/") + generationName);
  if (::symlink(linkTarget.constData(),
                QFile::encodeName(temporaryLink).constData()) != 0 ||
      ::rename(QFile::encodeName(temporaryLink).constData(),
               QFile::encodeName(currentPath).constData()) != 0)
    return finish(QStringLiteral("error"),
                  QStringLiteral("The generation is safe but not current"),
                  QStringLiteral("Butter preserved it and left the previous "
                                 "current pointer unchanged."));

  const QString finishedAt =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  const qulonglong sourceBytes =
      statValue(output, QStringLiteral("Total file size"));
  const qulonglong transferredBytes =
      statValue(output, QStringLiteral("Total transferred file size"));
  const int generationCount =
      QDir(generations)
          .entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)
          .size();
  QJsonArray skippedPaths;
  QJsonArray sourceChanges;
  for (const QString &line : allErrors) {
    if (isUnreadableSourcePath(line))
      skippedPaths.push_back(line);
    else if (isVanishedSourcePath(line))
      sourceChanges.push_back(line);
  }
  const QJsonObject receipt = {
      {QStringLiteral("schema"), 1},
      {QStringLiteral("generation"), generationName},
      {QStringLiteral("startedAt"), startedAt},
      {QStringLiteral("finishedAt"), finishedAt},
      {QStringLiteral("sourceBytes"), static_cast<qint64>(sourceBytes)},
      {QStringLiteral("transferredBytes"),
       static_cast<qint64>(transferredBytes)},
      {QStringLiteral("generationCount"), generationCount},
      {QStringLiteral("excludedPaths"), config.excludes.size()},
      {QStringLiteral("skippedPathCount"), skippedPaths.size()},
      {QStringLiteral("skippedAccessErrors"), skippedPaths},
      {QStringLiteral("sourceChangeCount"), sourceChanges.size()},
      {QStringLiteral("sourceChangeWarnings"), sourceChanges},
      {QStringLiteral("resumed"), resumed}};
  ShadowCommon::saveJson(
      QDir(receipts).filePath(generationName + QStringLiteral(".json")),
      receipt, nullptr);

  QString completionTitle = QStringLiteral("Home Shadow is current");
  QString completionNote = QStringLiteral("nothing is pruned automatically");
  if (exitedForChangingSource || !sourceChanges.isEmpty()) {
    completionTitle = QStringLiteral("Home Shadow captured a changing Home");
    completionNote =
        QStringLiteral("a few transient files vanished during the copy");
  }
  return finish(QStringLiteral("current"), completionTitle,
                QStringLiteral("%1 completed generation%2 · %3")
                    .arg(generationCount)
                    .arg(generationCount == 1 ? QString() : QStringLiteral("s"))
                    .arg(completionNote),
                receipt, 0);
}
