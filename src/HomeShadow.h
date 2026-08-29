#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

class HomeShadow : public QObject {
  Q_OBJECT

  Q_PROPERTY(bool configured READ configured NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString state READ state NOTIFY changed)
  Q_PROPERTY(QString title READ title NOTIFY changed)
  Q_PROPERTY(QString detail READ detail NOTIFY changed)
  Q_PROPERTY(QString destination READ destination NOTIFY changed)
  Q_PROPERTY(QString filesystem READ filesystem NOTIFY changed)
  Q_PROPERTY(QString lastRun READ lastRun NOTIFY changed)
  Q_PROPERTY(QString nextRun READ nextRun NOTIFY changed)
  Q_PROPERTY(int generationCount READ generationCount NOTIFY changed)
  Q_PROPERTY(int exclusionCount READ exclusionCount NOTIFY changed)
  Q_PROPERTY(qulonglong excludedBytes READ excludedBytes NOTIFY changed)

public:
  explicit HomeShadow(QObject *parent = nullptr);

  bool configured() const { return m_configured; }
  bool busy() const { return m_busy; }
  QString state() const { return m_state; }
  QString title() const { return m_title; }
  QString detail() const { return m_detail; }
  QString destination() const { return m_destination; }
  QString filesystem() const { return m_filesystem; }
  QString lastRun() const { return m_lastRun; }
  QString nextRun() const { return m_nextRun; }
  int generationCount() const { return m_generationCount; }
  int exclusionCount() const { return m_exclusionCount; }
  qulonglong excludedBytes() const { return m_excludedBytes; }

  Q_INVOKABLE QVariantMap inspectDestination(const QUrl &folder) const;
  Q_INVOKABLE void configure(const QUrl &folder, const QVariantList &findings);
  Q_INVOKABLE void runNow();
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void updateExclusions(const QVariantList &findings);

signals:
  void changed();

private:
  bool writeUnits(QString *error);
  void setError(const QString &title, const QString &detail);
  void refreshSoon();

  bool m_configured = false;
  bool m_busy = false;
  QString m_state = QStringLiteral("unconfigured");
  QString m_title = QStringLiteral("Your files still live on one drive");
  QString m_detail = QStringLiteral(
      "Create a quiet second copy of Home without rebuildable weight.");
  QString m_destination;
  QString m_filesystem;
  QString m_lastRun;
  QString m_nextRun = QStringLiteral("Daily when connected");
  int m_generationCount = 0;
  int m_exclusionCount = 0;
  qulonglong m_excludedBytes = 0;
  QProcess *m_controlProcess = nullptr;
  QTimer m_refreshTimer;
};
