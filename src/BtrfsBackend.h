#pragma once

#include "FilesystemBackend.h"

#include <QProcess>
#include <QVariantList>

class BtrfsBackend : public FilesystemBackend {
  Q_OBJECT

  Q_PROPERTY(bool supported READ supported NOTIFY stateChanged)
  Q_PROPERTY(QString source READ source NOTIFY stateChanged)
  Q_PROPERTY(QString mountSummary READ mountSummary NOTIFY stateChanged)
  Q_PROPERTY(qulonglong totalBytes READ totalBytes NOTIFY stateChanged)
  Q_PROPERTY(qulonglong usedBytes READ usedBytes NOTIFY stateChanged)
  Q_PROPERTY(qulonglong freeBytes READ freeBytes NOTIFY stateChanged)
  Q_PROPERTY(double usedPercent READ usedPercent NOTIFY stateChanged)
  Q_PROPERTY(double freePercent READ freePercent NOTIFY stateChanged)
  Q_PROPERTY(double dataChunkPercent READ dataChunkPercent NOTIFY stateChanged)
  Q_PROPERTY(QVariantList spaceHistory READ spaceHistory NOTIFY stateChanged)
  Q_PROPERTY(int deviceErrors READ deviceErrors NOTIFY stateChanged)
  Q_PROPERTY(int snapshotLimit READ snapshotLimit NOTIFY stateChanged)
  Q_PROPERTY(bool timelineEnabled READ timelineEnabled NOTIFY stateChanged)
  Q_PROPERTY(bool cleanupActive READ cleanupActive NOTIFY stateChanged)
  Q_PROPERTY(bool bootSyncActive READ bootSyncActive NOTIFY stateChanged)
  Q_PROPERTY(
      QVariantList recoveryPoints READ recoveryPoints NOTIFY stateChanged)
  Q_PROPERTY(int recoveryCount READ recoveryCount NOTIFY stateChanged)
  Q_PROPERTY(QString verdict READ verdict NOTIFY stateChanged)
  Q_PROPERTY(QString verdictDetail READ verdictDetail NOTIFY stateChanged)
  Q_PROPERTY(QString severity READ severity NOTIFY stateChanged)
  Q_PROPERTY(QString recoveryTitle READ recoveryTitle NOTIFY stateChanged)
  Q_PROPERTY(QString recoveryDetail READ recoveryDetail NOTIFY stateChanged)
  Q_PROPERTY(QString auditState READ auditState NOTIFY stateChanged)
  Q_PROPERTY(QString auditTitle READ auditTitle NOTIFY stateChanged)
  Q_PROPERTY(QString auditDetail READ auditDetail NOTIFY stateChanged)
  Q_PROPERTY(QVariantList auditFindings READ auditFindings NOTIFY stateChanged)
  Q_PROPERTY(int auditFindingCount READ auditFindingCount NOTIFY stateChanged)
  Q_PROPERTY(int orphanCount READ orphanCount NOTIFY stateChanged)
  Q_PROPERTY(qulonglong potentialRecoveryBytes READ potentialRecoveryBytes
                 NOTIFY stateChanged)
  Q_PROPERTY(QString remedyState READ remedyState NOTIFY stateChanged)
  Q_PROPERTY(QString remedyTitle READ remedyTitle NOTIFY stateChanged)
  Q_PROPERTY(QString remedyDetail READ remedyDetail NOTIFY stateChanged)
  Q_PROPERTY(bool reportCopied READ reportCopied NOTIFY stateChanged)

public:
  struct Usage {
    qulonglong deviceSize = 0;
    qulonglong used = 0;
    qulonglong estimatedFree = 0;
    qulonglong dataSize = 0;
    qulonglong dataUsed = 0;
  };

  explicit BtrfsBackend(QObject *parent = nullptr);

  QString providerId() const override { return QStringLiteral("btrfs"); }
  QString providerName() const override { return QStringLiteral("Btrfs"); }
  bool supported() const { return m_supported; }
  QString source() const { return m_source; }
  QString mountSummary() const { return m_mountSummary; }
  qulonglong totalBytes() const { return m_totalBytes; }
  qulonglong usedBytes() const { return m_usedBytes; }
  qulonglong freeBytes() const { return m_freeBytes; }
  double usedPercent() const { return m_usedPercent; }
  double freePercent() const { return m_freePercent; }
  double dataChunkPercent() const { return m_dataChunkPercent; }
  QVariantList spaceHistory() const { return m_spaceHistory; }
  int deviceErrors() const { return m_deviceErrors; }
  int snapshotLimit() const { return m_snapshotLimit; }
  bool timelineEnabled() const { return m_timelineEnabled; }
  bool cleanupActive() const { return m_cleanupActive; }
  bool bootSyncActive() const { return m_bootSyncActive; }
  QVariantList recoveryPoints() const { return m_recoveryPoints; }
  int recoveryCount() const { return m_recoveryPoints.size(); }
  QString verdict() const { return m_verdict; }
  QString verdictDetail() const { return m_verdictDetail; }
  QString severity() const { return m_severity; }
  QString recoveryTitle() const { return m_recoveryTitle; }
  QString recoveryDetail() const { return m_recoveryDetail; }
  QString auditState() const { return m_auditState; }
  QString auditTitle() const { return m_auditTitle; }
  QString auditDetail() const { return m_auditDetail; }
  QVariantList auditFindings() const { return m_auditFindings; }
  int auditFindingCount() const { return m_auditFindings.size(); }
  int orphanCount() const { return m_orphanCount; }
  qulonglong potentialRecoveryBytes() const { return m_potentialRecoveryBytes; }
  QString remedyState() const { return m_remedyState; }
  QString remedyTitle() const { return m_remedyTitle; }
  QString remedyDetail() const { return m_remedyDetail; }
  bool reportCopied() const { return m_reportCopied; }

  Q_INVOKABLE void refresh() override;
  Q_INVOKABLE void verifyRecovery();
  Q_INVOKABLE void removeOrphan(int snapshotNumber);
  Q_INVOKABLE void copyAuditReport();

  static Usage parseUsage(const QString &output);
  static int parseDeviceErrors(const QString &output);
  static QVariantList parseRecoveryPoints(const QByteArray &json);
  static QVariantList parseAuditFindings(const QByteArray &json, QString *error,
                                         int *actualSnapshots = nullptr,
                                         int *managedSnapshots = nullptr,
                                         qulonglong *potentialBytes = nullptr);

private:
  static QString run(const QString &program, const QStringList &arguments,
                     int timeoutMs = 2500);
  static QString valueFromConfig(const QString &path, const QString &key);
  static bool unitIs(const QString &unit, const QString &property,
                     const QString &expected);
  static QString findRecoveryMetadata();
  void composeMeaning();
  void composeAuditMeaning(int actualSnapshots = 0, int managedSnapshots = 0);
  void finishRecoveryAudit(int exitCode, QProcess::ExitStatus exitStatus);
  void finishRemedy(int exitCode, QProcess::ExitStatus exitStatus);
  QString auditReport() const;

  bool m_supported = false;
  QString m_source;
  QString m_mountSummary;
  qulonglong m_totalBytes = 0;
  qulonglong m_usedBytes = 0;
  qulonglong m_freeBytes = 0;
  double m_usedPercent = 0;
  double m_freePercent = 0;
  double m_dataChunkPercent = 0;
  QVariantList m_spaceHistory;
  bool m_historyRecorded = false;
  int m_deviceErrors = 0;
  int m_snapshotLimit = 0;
  bool m_timelineEnabled = false;
  bool m_cleanupActive = false;
  bool m_bootSyncActive = false;
  QVariantList m_recoveryPoints;
  QString m_verdict;
  QString m_verdictDetail;
  QString m_severity;
  QString m_recoveryTitle;
  QString m_recoveryDetail;
  QProcess *m_auditProcess = nullptr;
  QString m_auditState = QStringLiteral("idle");
  QString m_auditTitle;
  QString m_auditDetail;
  QVariantList m_auditFindings;
  int m_orphanCount = 0;
  qulonglong m_potentialRecoveryBytes = 0;
  QProcess *m_remedyProcess = nullptr;
  QString m_remedyState = QStringLiteral("idle");
  QString m_remedyTitle;
  QString m_remedyDetail;
  bool m_reportCopied = false;
};
