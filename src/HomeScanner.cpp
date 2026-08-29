#include "HomeScanner.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <functional>
#include <linux/ioprio.h>
#include <memory>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>

namespace {

QString normalized(const QString &path) {
  const QString canonical = QFileInfo(path).canonicalFilePath();
  return canonical.isEmpty() ? QDir::cleanPath(path) : canonical;
}

QVariantMap entryMap(const QString &path, const QString &name, qulonglong bytes,
                     qulonglong files, bool directory,
                     const QString &kind = QStringLiteral("path")) {
  QVariantMap entry;
  entry.insert(QStringLiteral("path"), path);
  entry.insert(QStringLiteral("name"), name);
  entry.insert(QStringLiteral("bytes"), bytes);
  entry.insert(QStringLiteral("fileCount"), files);
  entry.insert(QStringLiteral("directory"), directory);
  entry.insert(QStringLiteral("kind"), kind);
  return entry;
}

bool isInside(const QString &path, const QString &parent) {
  return path == parent || path.startsWith(parent + QLatin1Char('/'));
}

bool hasPythonDependencyManifest(const QString &projectPath) {
  static const QStringList manifests = {
      QStringLiteral("pyproject.toml"),   QStringLiteral("uv.lock"),
      QStringLiteral("poetry.lock"),      QStringLiteral("Pipfile"),
      QStringLiteral("requirements.txt"), QStringLiteral("environment.yml"),
      QStringLiteral("environment.yaml"), QStringLiteral("setup.py")};
  const QDir project(projectPath);
  return std::any_of(manifests.cbegin(), manifests.cend(),
                     [&project](const QString &manifest) {
                       return QFileInfo::exists(project.filePath(manifest));
                     });
}

} // namespace

HomeScanner::HomeScanner(QObject *parent)
    : QObject(parent), m_rootPath(normalized(QDir::homePath())),
      m_focusPath(m_rootPath) {
  refreshDocker();
}

HomeScanner::~HomeScanner() {
  m_cancelled.store(true);
  if (m_scanThread) {
    m_scanThread->requestInterruption();
    m_scanThread->wait();
  }
}

QString HomeScanner::focusLabel() const {
  if (m_focusPath == m_rootPath)
    return QStringLiteral("Home");
  return QStringLiteral("Home / %1")
      .arg(QDir(m_rootPath).relativeFilePath(m_focusPath));
}

void HomeScanner::startScan() {
  if ((m_scanThread && m_scanThread->isRunning()) ||
      (m_cleanupProcess && m_cleanupProcess->state() != QProcess::NotRunning))
    return;

  m_cancelled.store(false);
  m_state = QStringLiteral("scanning");
  m_focusPath = m_rootPath;
  m_entries.clear();
  m_buildFindings.clear();
  m_shadowFindings.clear();
  m_buildOutputBytes = 0;
  m_cleanupState = QStringLiteral("idle");
  m_cleanupTitle.clear();
  m_cleanupDetail.clear();
  m_cleanupPath.clear();
  m_cleanupBytes = 0;
  m_directories.clear();
  m_children.clear();
  m_files.clear();
  m_scannedBytes = 0;
  m_scannedFiles = 0;
  m_scannedDirectories = 0;
  m_skippedMounts = 0;
  m_scanDetail = QStringLiteral("Starting inside your home folder…");
  emit stateChanged();

  QThread *thread = QThread::create([this] { scanHome(); });
  m_scanThread = thread;
  connect(thread, &QThread::finished, this, [this, thread] {
    if (m_scanThread == thread)
      m_scanThread = nullptr;
    thread->deleteLater();
  });
  thread->start(QThread::LowPriority);
}

void HomeScanner::cancelScan() {
  if (m_state != QLatin1String("scanning"))
    return;
  m_cancelled.store(true);
  m_scanDetail = QStringLiteral("Stopping after the current folder…");
  emit stateChanged();
}

void HomeScanner::openPath(const QString &path) {
  const QString clean = normalized(path);
  if (!isInside(clean, m_rootPath) || !m_directories.contains(clean))
    return;
  m_focusPath = clean;
  rebuildEntries();
  emit stateChanged();
}

void HomeScanner::goUp() {
  if (!canGoUp())
    return;
  QString parent = QFileInfo(m_focusPath).dir().absolutePath();
  if (!isInside(parent, m_rootPath))
    parent = m_rootPath;
  m_focusPath = parent;
  rebuildEntries();
  emit stateChanged();
}

void HomeScanner::keepLargeFile(QVector<FileStats> &files,
                                const FileStats &file, int limit) {
  if (file.bytes == 0)
    return;
  if (files.size() < limit) {
    files.push_back(file);
    return;
  }
  const auto smallest = std::min_element(
      files.begin(), files.end(),
      [](const FileStats &a, const FileStats &b) { return a.bytes < b.bytes; });
  if (smallest != files.end() && file.bytes > smallest->bytes)
    *smallest = file;
}

std::optional<HomeScanner::Artifact>
HomeScanner::artifactForDirectory(const QString &path, const QString &rootPath,
                                  const QVector<Artifact> &existing) {
  for (const Artifact &artifact : existing) {
    if (isInside(path, artifact.path))
      return std::nullopt;
  }

  const QFileInfo info(path);
  const QString name = info.fileName();
  const QString parent = info.dir().absolutePath();
  const auto make = [&](const QString &type, const QString &title,
                        const QString &consequence,
                        const QString &project = QString()) {
    return Artifact{
        path, project.isEmpty() ? parent : project, type, title, consequence, 0,
        0};
  };

  if (name == QLatin1String("node_modules") &&
      QFileInfo::exists(QDir(parent).filePath(QStringLiteral("package.json"))))
    return make(
        QStringLiteral("node"), QStringLiteral("Node dependencies"),
        QStringLiteral("Can be restored from the project's package manifest."));

  if ((name == QLatin1String("target") ||
       name.startsWith(QLatin1String("target-"))) &&
      QFileInfo::exists(QDir(parent).filePath(QStringLiteral("Cargo.toml"))))
    return make(QStringLiteral("rust"), QStringLiteral("Rust build output"),
                QStringLiteral(
                    "Cargo can rebuild it; the next compile will be slower."));

  if (name == QLatin1String("build") &&
      (QFileInfo::exists(
           QDir(path).filePath(QStringLiteral("CMakeCache.txt"))) ||
       QFileInfo::exists(
           QDir(parent).filePath(QStringLiteral("CMakeLists.txt")))))
    return make(
        QStringLiteral("cmake"), QStringLiteral("CMake build output"),
        QStringLiteral(
            "The project can regenerate it from its source configuration."));

  if ((name == QLatin1String(".next") || name == QLatin1String(".turbo")) &&
      QFileInfo::exists(QDir(parent).filePath(QStringLiteral("package.json"))))
    return make(
        QStringLiteral("web"), QStringLiteral("Web build cache"),
        QStringLiteral(
            "The project can regenerate it; the next build will be slower."));

  if ((name == QLatin1String(".venv") || name == QLatin1String("venv")) &&
      QFileInfo::exists(QDir(path).filePath(QStringLiteral("pyvenv.cfg")))) {
    const bool manifested = hasPythonDependencyManifest(parent);
    return make(
        manifested ? QStringLiteral("python-manifest")
                   : QStringLiteral("python"),
        QStringLiteral("Python environment"),
        manifested
            ? QStringLiteral("The Shadow copy can omit it because the project "
                             "records how to recreate its dependencies.")
            : QStringLiteral("Usually reproducible, but no nearby dependency "
                             "manifest proved that; review first."));
  }

  const QString relative = QDir(rootPath).relativeFilePath(path);
  if (relative == QLatin1String(".cargo/registry") ||
      relative == QLatin1String(".cargo/git"))
    return make(
        QStringLiteral("cargo-cache"), QStringLiteral("Cargo download cache"),
        QStringLiteral(
            "Safe to download again; future Rust builds may take longer."),
        QDir(rootPath).filePath(QStringLiteral(".cargo")));
  if (relative == QLatin1String(".npm/_cacache"))
    return make(QStringLiteral("npm-cache"),
                QStringLiteral("npm download cache"),
                QStringLiteral(
                    "Safe to download again; future installs may take longer."),
                QDir(rootPath).filePath(QStringLiteral(".npm")));

  return std::nullopt;
}

QVariantList
HomeScanner::topEntries(const QHash<QString, DirectoryStats> &stats,
                        const QString &rootPath, int limit) {
  QVariantList entries;
  QVector<QVariantMap> sorted;
  for (auto it = stats.cbegin(); it != stats.cend(); ++it) {
    if (it.key() == rootPath || it.value().parent != rootPath)
      continue;
    sorted.push_back(entryMap(it.key(), it.value().name, it.value().totalBytes,
                              it.value().totalFiles, true));
  }
  std::sort(sorted.begin(), sorted.end(),
            [](const QVariantMap &a, const QVariantMap &b) {
              return a.value(QStringLiteral("bytes")).toULongLong() >
                     b.value(QStringLiteral("bytes")).toULongLong();
            });
  for (int i = 0; i < std::min<qsizetype>(limit, sorted.size()); ++i)
    entries.push_back(sorted.at(i));
  return entries;
}

void HomeScanner::scanHome() {
  // QThread::LowPriority is only a scheduling hint on Linux. Keep a scan from
  // competing with the user's editor, compiler, or game for CPU and storage.
  ::setpriority(PRIO_PROCESS, 0, 10);
  ::syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, 0,
            IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0));

  auto result = std::make_shared<ScanResult>();
  struct stat rootStat{};
  if (::stat(m_rootPath.toUtf8().constData(), &rootStat) != 0) {
    result->error = QStringLiteral("Butter could not read your home folder.");
    QMetaObject::invokeMethod(
        this, [this, result] { applyFinished(result); }, Qt::QueuedConnection);
    return;
  }

  const dev_t homeDevice = rootStat.st_dev;
  constexpr int retainedDepth = 4;
  QHash<QString, DirectoryStats> liveTop;
  QElapsedTimer updateTimer;
  updateTimer.start();

  std::function<DirectoryStats(const QString &, const QString &, int)> scanDir;
  scanDir = [&](const QString &current, const QString &parent,
                int depth) -> DirectoryStats {
    DirectoryStats stats;
    stats.parent = parent;
    stats.name =
        depth == 0 ? QStringLiteral("Home") : QFileInfo(current).fileName();
    ++result->directoryCount;

    qsizetype artifactIndex = -1;
    if (depth > 0) {
      if (const auto artifact =
              artifactForDirectory(current, m_rootPath, result->artifacts)) {
        artifactIndex = result->artifacts.size();
        result->artifacts.push_back(*artifact);
      }
    }

    QDir directory(current);
    const QFileInfoList children = directory.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDir::NoSort);

    for (const QFileInfo &info : children) {
      if (m_cancelled.load())
        break;
      if (info.isSymbolicLink())
        continue;

      struct stat itemStat{};
      const QByteArray encoded = QFile::encodeName(info.absoluteFilePath());
      if (::lstat(encoded.constData(), &itemStat) != 0)
        continue;
      if (itemStat.st_dev != homeDevice) {
        if (S_ISDIR(itemStat.st_mode))
          ++result->skippedMounts;
        continue;
      }

      if (S_ISDIR(itemStat.st_mode)) {
        const QString path = QDir::cleanPath(info.absoluteFilePath());
        const DirectoryStats childStats = scanDir(path, current, depth + 1);
        stats.totalBytes += childStats.totalBytes;
        stats.totalFiles += childStats.totalFiles;
        if (depth + 1 <= retainedDepth)
          result->children[current].push_back(path);
      } else if (S_ISREG(itemStat.st_mode)) {
        const qulonglong bytes = std::max<qint64>(0, itemStat.st_size);
        stats.directBytes += bytes;
        ++stats.directFiles;
        result->bytes += bytes;
        ++result->fileCount;
        if (depth <= retainedDepth)
          keepLargeFile(result->files[current],
                        {info.absoluteFilePath(), info.fileName(), bytes});

        const QString relative =
            QDir(m_rootPath).relativeFilePath(info.absoluteFilePath());
        const QString first = relative.section(QLatin1Char('/'), 0, 0);
        if (!first.isEmpty() && first != QLatin1String(".")) {
          const QString topPath = QDir(m_rootPath).filePath(first);
          DirectoryStats &top = liveTop[topPath];
          top.name = first;
          top.parent = m_rootPath;
          top.totalBytes += bytes;
          ++top.totalFiles;
        }
      }
    }

    if (updateTimer.elapsed() >= 300) {
      const QVariantList entries = topEntries(liveTop, m_rootPath);
      const auto bytes = result->bytes;
      const auto files = result->fileCount;
      const auto directories = result->directoryCount;
      const auto skipped = result->skippedMounts;
      QMetaObject::invokeMethod(
          this,
          [this, entries, bytes, files, directories, skipped, current] {
            applyProgress(entries, bytes, files, directories, skipped, current);
          },
          Qt::QueuedConnection);
      updateTimer.restart();
    }

    stats.totalBytes += stats.directBytes;
    stats.totalFiles += stats.directFiles;
    if (artifactIndex >= 0) {
      result->artifacts[artifactIndex].bytes = stats.totalBytes;
      result->artifacts[artifactIndex].fileCount = stats.totalFiles;
    }
    if (depth <= retainedDepth)
      result->directories.insert(current, stats);
    return stats;
  };

  scanDir(m_rootPath, QString(), 0);
  result->cancelled = m_cancelled.load();

  QMetaObject::invokeMethod(
      this, [this, result] { applyFinished(result); }, Qt::QueuedConnection);
}

void HomeScanner::applyProgress(const QVariantList &entries, qulonglong bytes,
                                qulonglong files, qulonglong directories,
                                qulonglong skippedMounts,
                                const QString &currentPath) {
  if (m_state != QLatin1String("scanning"))
    return;
  m_entries = entries;
  m_scannedBytes = bytes;
  m_scannedFiles = files;
  m_scannedDirectories = directories;
  m_skippedMounts = skippedMounts;
  const QString relative = QDir(m_rootPath).relativeFilePath(currentPath);
  m_scanDetail = QStringLiteral("Reading Home / %1").arg(relative);
  emit stateChanged();
}

void HomeScanner::applyFinished(const std::shared_ptr<ScanResult> &result) {
  if (!result->error.isEmpty()) {
    m_state = QStringLiteral("error");
    m_scanDetail = result->error;
    emit stateChanged();
    return;
  }

  m_directories = result->directories;
  m_children = result->children;
  m_files = result->files;
  m_scannedBytes = result->bytes;
  m_scannedFiles = result->fileCount;
  m_scannedDirectories = result->directoryCount;
  m_skippedMounts = result->skippedMounts;
  m_state = result->cancelled ? QStringLiteral("cancelled")
                              : QStringLiteral("complete");
  m_scanDetail =
      result->cancelled
          ? QStringLiteral("Scan stopped. Partial results are shown.")
          : QStringLiteral("Home scan complete. No system folders were read.");

  m_buildFindings.clear();
  m_shadowFindings.clear();
  m_buildOutputBytes = 0;
  QVector<QVariantMap> findings;
  for (const Artifact &artifact : result->artifacts) {
    QVariantMap finding;
    finding.insert(QStringLiteral("path"), artifact.path);
    finding.insert(QStringLiteral("projectPath"), artifact.projectPath);
    finding.insert(QStringLiteral("type"), artifact.type);
    finding.insert(QStringLiteral("title"), artifact.title);
    finding.insert(QStringLiteral("bytes"), artifact.bytes);
    finding.insert(QStringLiteral("fileCount"), artifact.fileCount);
    finding.insert(QStringLiteral("consequence"), artifact.consequence);
    const bool pythonEnvironment =
        artifact.type == QLatin1String("python") ||
        artifact.type == QLatin1String("python-manifest");
    const bool shadowSafe = artifact.type != QLatin1String("python");
    if (shadowSafe) {
      QVariantMap shadowFinding = finding;
      shadowFinding.insert(QStringLiteral("safety"),
                           QStringLiteral("regenerable"));
      m_shadowFindings.push_back(shadowFinding);
    }
    if (artifact.bytes >= 100'000'000) {
      finding.insert(QStringLiteral("safety"),
                     pythonEnvironment ? QStringLiteral("review")
                                       : QStringLiteral("regenerable"));
      findings.push_back(finding);
      m_buildOutputBytes += artifact.bytes;
    }
  }
  std::sort(findings.begin(), findings.end(),
            [](const QVariantMap &a, const QVariantMap &b) {
              return a.value(QStringLiteral("bytes")).toULongLong() >
                     b.value(QStringLiteral("bytes")).toULongLong();
            });
  for (const QVariantMap &finding : findings)
    m_buildFindings.push_back(finding);

  m_focusPath = m_rootPath;
  rebuildEntries();
  emit stateChanged();
}

void HomeScanner::rebuildEntries() {
  QVector<QVariantMap> sorted;
  const QVector<QString> childPaths = m_children.value(m_focusPath);
  for (const QString &path : childPaths) {
    const DirectoryStats stats = m_directories.value(path);
    QString kind = QStringLiteral("path");
    const bool isArtifact = std::any_of(
        m_buildFindings.cbegin(), m_buildFindings.cend(),
        [&path](const QVariant &finding) {
          return finding.toMap().value(QStringLiteral("path")).toString() ==
                 path;
        });
    if (isArtifact)
      kind = QStringLiteral("build");
    sorted.push_back(entryMap(path, stats.name, stats.totalBytes,
                              stats.totalFiles, true, kind));
  }
  for (const FileStats &file : m_files.value(m_focusPath))
    sorted.push_back(entryMap(file.path, file.name, file.bytes, 1, false,
                              QStringLiteral("file")));

  std::sort(sorted.begin(), sorted.end(),
            [](const QVariantMap &a, const QVariantMap &b) {
              return a.value(QStringLiteral("bytes")).toULongLong() >
                     b.value(QStringLiteral("bytes")).toULongLong();
            });
  m_entries.clear();
  for (int i = 0; i < std::min<qsizetype>(24, sorted.size()); ++i)
    m_entries.push_back(sorted.at(i));
}

qulonglong HomeScanner::parseHumanBytes(const QString &text) {
  const QRegularExpression pattern(
      QStringLiteral("([0-9]+(?:\\.[0-9]+)?)\\s*([KMGTPE]?B)"),
      QRegularExpression::CaseInsensitiveOption);
  const auto match = pattern.match(text.trimmed());
  if (!match.hasMatch())
    return 0;
  const double value = match.captured(1).toDouble();
  const QString unit = match.captured(2).toUpper();
  const QStringList units = {QStringLiteral("B"),  QStringLiteral("KB"),
                             QStringLiteral("MB"), QStringLiteral("GB"),
                             QStringLiteral("TB"), QStringLiteral("PB"),
                             QStringLiteral("EB")};
  const int exponent =
      static_cast<int>(std::max<qsizetype>(0, units.indexOf(unit)));
  double multiplier = 1.0;
  for (int i = 0; i < exponent; ++i)
    multiplier *= 1000.0;
  return static_cast<qulonglong>(value * multiplier);
}

QVariantList HomeScanner::parseDockerReport(const QByteArray &output) {
  QVariantList findings;
  const QList<QByteArray> lines = output.split('\n');
  for (const QByteArray &line : lines) {
    if (line.trimmed().isEmpty())
      continue;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
      continue;
    const QJsonObject object = document.object();
    const QString type = object.value(QStringLiteral("Type")).toString();
    QVariantMap finding;
    finding.insert(QStringLiteral("type"), type);
    finding.insert(
        QStringLiteral("total"),
        object.value(QStringLiteral("TotalCount")).toString().toInt());
    finding.insert(QStringLiteral("active"),
                   object.value(QStringLiteral("Active")).toString().toInt());
    finding.insert(
        QStringLiteral("bytes"),
        parseHumanBytes(object.value(QStringLiteral("Size")).toString()));
    finding.insert(QStringLiteral("reclaimableBytes"),
                   parseHumanBytes(
                       object.value(QStringLiteral("Reclaimable")).toString()));

    if (type == QLatin1String("Build Cache")) {
      finding.insert(QStringLiteral("title"),
                     QStringLiteral("Docker build cache"));
      finding.insert(QStringLiteral("safety"), QStringLiteral("regenerable"));
      finding.insert(QStringLiteral("meaning"),
                     QStringLiteral("Old build layers; future image builds may "
                                    "be slower after cleanup."));
    } else if (type == QLatin1String("Images")) {
      finding.insert(QStringLiteral("title"),
                     QStringLiteral("Unused Docker images"));
      finding.insert(QStringLiteral("safety"), QStringLiteral("review"));
      finding.insert(QStringLiteral("meaning"),
                     QStringLiteral("Can be downloaded or rebuilt again, but "
                                    "may be useful offline."));
    } else if (type == QLatin1String("Containers")) {
      finding.insert(QStringLiteral("title"),
                     QStringLiteral("Stopped containers"));
      finding.insert(QStringLiteral("safety"), QStringLiteral("review"));
      finding.insert(QStringLiteral("meaning"),
                     QStringLiteral("May contain writable state that was never "
                                    "moved into a volume."));
    } else if (type == QLatin1String("Local Volumes")) {
      finding.insert(QStringLiteral("title"),
                     QStringLiteral("Unused Docker volumes"));
      finding.insert(QStringLiteral("safety"), QStringLiteral("protected"));
      finding.insert(
          QStringLiteral("meaning"),
          QStringLiteral("May contain irreplaceable databases or app data. "
                         "Butter will not clean these automatically."));
    } else {
      continue;
    }
    findings.push_back(finding);
  }
  std::sort(findings.begin(), findings.end(),
            [](const QVariant &a, const QVariant &b) {
              return a.toMap()
                         .value(QStringLiteral("reclaimableBytes"))
                         .toULongLong() >
                     b.toMap()
                         .value(QStringLiteral("reclaimableBytes"))
                         .toULongLong();
            });
  return findings;
}

bool HomeScanner::isRemovableArtifactType(const QString &type) {
  return type == QLatin1String("node") || type == QLatin1String("rust") ||
         type == QLatin1String("cmake") || type == QLatin1String("web") ||
         type == QLatin1String("cargo-cache") ||
         type == QLatin1String("npm-cache");
}

void HomeScanner::removeArtifact(const QString &path, const QString &type) {
  if ((m_cleanupProcess && m_cleanupProcess->state() != QProcess::NotRunning) ||
      m_state == QLatin1String("scanning"))
    return;

  m_cleanupPath = QDir::cleanPath(path);
  m_cleanupBytes = 0;
  QVariantMap reviewed;
  for (const QVariant &findingValue : std::as_const(m_buildFindings)) {
    const QVariantMap finding = findingValue.toMap();
    if (finding.value(QStringLiteral("path")).toString() == m_cleanupPath &&
        finding.value(QStringLiteral("type")).toString() == type) {
      reviewed = finding;
      break;
    }
  }

  const QString clean = normalized(m_cleanupPath);
  const auto detected =
      artifactForDirectory(clean, m_rootPath, QVector<Artifact>{});
  const bool exactKnownFinding =
      !reviewed.isEmpty() &&
      reviewed.value(QStringLiteral("safety")).toString() ==
          QLatin1String("regenerable");
  const bool safePath = clean == m_cleanupPath && clean != m_rootPath &&
                        isInside(clean, m_rootPath) &&
                        QFileInfo(clean).isDir() &&
                        !QFileInfo(clean).isSymbolicLink();
  const bool markerMatches = detected && detected->type == type;

  if (!exactKnownFinding || !safePath || !markerMatches ||
      !isRemovableArtifactType(type)) {
    m_cleanupState = QStringLiteral("error");
    m_cleanupTitle = QStringLiteral("Butter stopped safely");
    m_cleanupDetail = QStringLiteral(
        "The folder no longer matches the regenerable artifact that was "
        "reviewed. Nothing was removed; scan Home again before retrying.");
    emit stateChanged();
    return;
  }

  const QString remover = QStandardPaths::findExecutable(QStringLiteral("rm"));
  if (remover.isEmpty()) {
    m_cleanupState = QStringLiteral("error");
    m_cleanupTitle = QStringLiteral("Removal is unavailable");
    m_cleanupDetail =
        QStringLiteral("Butter could not find the system removal tool.");
    emit stateChanged();
    return;
  }

  if (!m_cleanupProcess) {
    m_cleanupProcess = new QProcess(this);
    m_cleanupProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(
        m_cleanupProcess,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
          if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            const QString error =
                QString::fromUtf8(m_cleanupProcess->readAllStandardError())
                    .trimmed();
            m_cleanupState = QStringLiteral("error");
            m_cleanupTitle = QStringLiteral("Cleanup stopped");
            m_cleanupDetail =
                error.isEmpty()
                    ? QStringLiteral("The folder could not be fully "
                                     "removed. Nothing outside it was "
                                     "touched.")
                    : error.section(QLatin1Char('\n'), -1);
            emit stateChanged();
            return;
          }

          QVariantList remaining;
          for (const QVariant &findingValue : std::as_const(m_buildFindings)) {
            if (findingValue.toMap().value(QStringLiteral("path")).toString() !=
                m_cleanupPath)
              remaining.push_back(findingValue);
          }
          m_buildFindings = remaining;
          m_buildOutputBytes = m_buildOutputBytes > m_cleanupBytes
                                   ? m_buildOutputBytes - m_cleanupBytes
                                   : 0;
          m_cleanupState = QStringLiteral("removed");
          m_cleanupTitle = QStringLiteral("Regenerable weight removed");
          m_cleanupDetail = QStringLiteral(
              "The reviewed folder is gone. Scan Home again to refresh "
              "the map; the next project build or install may be slower.");
          emit stateChanged();
        });
    connect(m_cleanupProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
              if (error != QProcess::FailedToStart)
                return;
              m_cleanupState = QStringLiteral("error");
              m_cleanupTitle = QStringLiteral("Removal is unavailable");
              m_cleanupDetail =
                  QStringLiteral("Butter could not start the system removal "
                                 "tool. Nothing was removed.");
              emit stateChanged();
            });
  }

  m_cleanupBytes = reviewed.value(QStringLiteral("bytes")).toULongLong();
  m_cleanupState = QStringLiteral("running");
  m_cleanupTitle = QStringLiteral("Removing reviewed output");
  m_cleanupDetail = QStringLiteral(
      "Butter re-checked the exact path and is staying on this filesystem.");
  emit stateChanged();
  m_cleanupProcess->setProgram(remover);
  m_cleanupProcess->setArguments(
      {QStringLiteral("--recursive"), QStringLiteral("--force"),
       QStringLiteral("--one-file-system"), QStringLiteral("--"), clean});
  m_cleanupProcess->start();
}

void HomeScanner::refreshDocker() {
  if (m_dockerProcess && m_dockerProcess->state() != QProcess::NotRunning)
    return;
  if (!m_dockerProcess) {
    m_dockerProcess = new QProcess(this);
    m_dockerProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(
        m_dockerProcess,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this](int exitCode, QProcess::ExitStatus exitStatus) {
          if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            m_dockerState = QStringLiteral("unavailable");
            m_dockerDetail =
                QStringLiteral("Docker is not running or this user cannot ask "
                               "it for storage totals.");
            m_dockerFindings.clear();
            m_dockerReviewableBytes = 0;
            emit stateChanged();
            return;
          }
          m_dockerFindings =
              parseDockerReport(m_dockerProcess->readAllStandardOutput());
          m_dockerReviewableBytes = 0;
          for (const QVariant &findingValue : std::as_const(m_dockerFindings)) {
            const QVariantMap finding = findingValue.toMap();
            if (finding.value(QStringLiteral("safety")).toString() !=
                QLatin1String("protected"))
              m_dockerReviewableBytes +=
                  finding.value(QStringLiteral("reclaimableBytes"))
                      .toULongLong();
          }
          m_dockerState = m_dockerFindings.isEmpty() ? QStringLiteral("empty")
                                                     : QStringLiteral("ready");
          m_dockerDetail =
              m_dockerFindings.isEmpty()
                  ? QStringLiteral("Docker reports no storage use.")
                  : QStringLiteral("Docker supplied these totals directly; "
                                   "Butter did not scan its private storage.");
          emit stateChanged();
        });
  }

  m_dockerState = QStringLiteral("checking");
  m_dockerDetail = QStringLiteral("Asking Docker what it owns…");
  emit stateChanged();
  m_dockerProcess->setProgram(QStringLiteral("docker"));
  m_dockerProcess->setArguments({QStringLiteral("system"), QStringLiteral("df"),
                                 QStringLiteral("--format"),
                                 QStringLiteral("{{json .}}")});
  m_dockerProcess->start();
  QTimer::singleShot(15000, this, [this] {
    if (m_dockerProcess && m_dockerProcess->state() != QProcess::NotRunning)
      m_dockerProcess->kill();
  });
}
