#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QVariantList>

#include <atomic>
#include <memory>
#include <optional>

class QThread;

class HomeScanner : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString rootPath READ rootPath CONSTANT)
  Q_PROPERTY(QString focusPath READ focusPath NOTIFY stateChanged)
  Q_PROPERTY(QString focusLabel READ focusLabel NOTIFY stateChanged)
  Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY stateChanged)
  Q_PROPERTY(QVariantList entries READ entries NOTIFY stateChanged)
  Q_PROPERTY(QVariantList buildFindings READ buildFindings NOTIFY stateChanged)
  Q_PROPERTY(
      qulonglong buildOutputBytes READ buildOutputBytes NOTIFY stateChanged)
  Q_PROPERTY(qulonglong scannedBytes READ scannedBytes NOTIFY stateChanged)
  Q_PROPERTY(qulonglong scannedFiles READ scannedFiles NOTIFY stateChanged)
  Q_PROPERTY(
      qulonglong scannedDirectories READ scannedDirectories NOTIFY stateChanged)
  Q_PROPERTY(qulonglong skippedMounts READ skippedMounts NOTIFY stateChanged)
  Q_PROPERTY(QString scanDetail READ scanDetail NOTIFY stateChanged)
  Q_PROPERTY(QString dockerState READ dockerState NOTIFY stateChanged)
  Q_PROPERTY(QString dockerDetail READ dockerDetail NOTIFY stateChanged)
  Q_PROPERTY(
      QVariantList dockerFindings READ dockerFindings NOTIFY stateChanged)
  Q_PROPERTY(qulonglong dockerReviewableBytes READ dockerReviewableBytes NOTIFY
                 stateChanged)
  Q_PROPERTY(QString cleanupState READ cleanupState NOTIFY stateChanged)
  Q_PROPERTY(QString cleanupTitle READ cleanupTitle NOTIFY stateChanged)
  Q_PROPERTY(QString cleanupDetail READ cleanupDetail NOTIFY stateChanged)
  Q_PROPERTY(QString cleanupPath READ cleanupPath NOTIFY stateChanged)

public:
  explicit HomeScanner(QObject *parent = nullptr);
  ~HomeScanner() override;

  QString state() const { return m_state; }
  QString rootPath() const { return m_rootPath; }
  QString focusPath() const { return m_focusPath; }
  QString focusLabel() const;
  bool canGoUp() const { return m_focusPath != m_rootPath; }
  QVariantList entries() const { return m_entries; }
  QVariantList buildFindings() const { return m_buildFindings; }
  qulonglong buildOutputBytes() const { return m_buildOutputBytes; }
  qulonglong scannedBytes() const { return m_scannedBytes; }
  qulonglong scannedFiles() const { return m_scannedFiles; }
  qulonglong scannedDirectories() const { return m_scannedDirectories; }
  qulonglong skippedMounts() const { return m_skippedMounts; }
  QString scanDetail() const { return m_scanDetail; }
  QString dockerState() const { return m_dockerState; }
  QString dockerDetail() const { return m_dockerDetail; }
  QVariantList dockerFindings() const { return m_dockerFindings; }
  qulonglong dockerReviewableBytes() const { return m_dockerReviewableBytes; }
  QString cleanupState() const { return m_cleanupState; }
  QString cleanupTitle() const { return m_cleanupTitle; }
  QString cleanupDetail() const { return m_cleanupDetail; }
  QString cleanupPath() const { return m_cleanupPath; }

  Q_INVOKABLE void startScan();
  Q_INVOKABLE void cancelScan();
  Q_INVOKABLE void openPath(const QString &path);
  Q_INVOKABLE void goUp();
  Q_INVOKABLE void refreshDocker();
  Q_INVOKABLE void removeArtifact(const QString &path, const QString &type);

  static qulonglong parseHumanBytes(const QString &text);
  static QVariantList parseDockerReport(const QByteArray &output);
  static bool isRemovableArtifactType(const QString &type);

signals:
  void stateChanged();

private:
  struct DirectoryStats {
    qulonglong directBytes = 0;
    qulonglong totalBytes = 0;
    qulonglong directFiles = 0;
    qulonglong totalFiles = 0;
    QString parent;
    QString name;
  };

  struct FileStats {
    QString path;
    QString name;
    qulonglong bytes = 0;
  };

  struct Artifact {
    QString path;
    QString projectPath;
    QString type;
    QString title;
    QString consequence;
    qulonglong bytes = 0;
    qulonglong fileCount = 0;
  };

  struct ScanResult {
    QHash<QString, DirectoryStats> directories;
    QHash<QString, QVector<QString>> children;
    QHash<QString, QVector<FileStats>> files;
    QVector<Artifact> artifacts;
    qulonglong bytes = 0;
    qulonglong fileCount = 0;
    qulonglong directoryCount = 0;
    qulonglong skippedMounts = 0;
    bool cancelled = false;
    QString error;
  };

  void scanHome();
  void applyProgress(const QVariantList &entries, qulonglong bytes,
                     qulonglong files, qulonglong directories,
                     qulonglong skippedMounts, const QString &currentPath);
  void applyFinished(const std::shared_ptr<ScanResult> &result);
  void rebuildEntries();
  static QVariantList topEntries(const QHash<QString, DirectoryStats> &stats,
                                 const QString &rootPath, int limit = 18);
  static std::optional<Artifact>
  artifactForDirectory(const QString &path, const QString &rootPath,
                       const QVector<Artifact> &existing);
  static void keepLargeFile(QVector<FileStats> &files, const FileStats &file,
                            int limit = 12);

  QString m_state = QStringLiteral("idle");
  const QString m_rootPath;
  QString m_focusPath;
  QVariantList m_entries;
  QVariantList m_buildFindings;
  qulonglong m_buildOutputBytes = 0;
  qulonglong m_scannedBytes = 0;
  qulonglong m_scannedFiles = 0;
  qulonglong m_scannedDirectories = 0;
  qulonglong m_skippedMounts = 0;
  QString m_scanDetail;
  QHash<QString, DirectoryStats> m_directories;
  QHash<QString, QVector<QString>> m_children;
  QHash<QString, QVector<FileStats>> m_files;
  QThread *m_scanThread = nullptr;
  std::atomic_bool m_cancelled = false;

  QProcess *m_dockerProcess = nullptr;
  QString m_dockerState = QStringLiteral("checking");
  QString m_dockerDetail = QStringLiteral("Asking Docker what it owns…");
  QVariantList m_dockerFindings;
  qulonglong m_dockerReviewableBytes = 0;

  QProcess *m_cleanupProcess = nullptr;
  QString m_cleanupState = QStringLiteral("idle");
  QString m_cleanupTitle;
  QString m_cleanupDetail;
  QString m_cleanupPath;
  qulonglong m_cleanupBytes = 0;
};
