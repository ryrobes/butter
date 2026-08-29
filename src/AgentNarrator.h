#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>

#include <optional>

class QTemporaryFile;

class AgentNarrator : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString state READ state NOTIFY changed)
  Q_PROPERTY(QString agentName READ agentName NOTIFY changed)
  Q_PROPERTY(QString headline READ headline NOTIFY changed)
  Q_PROPERTY(QString summary READ summary NOTIFY changed)
  Q_PROPERTY(bool ready READ ready NOTIFY changed)

public:
  struct Response {
    QString headline;
    QString summary;
    QString trendDirection;
  };

  explicit AgentNarrator(QObject *parent = nullptr);

  QString state() const { return m_state; }
  QString agentName() const { return m_agentName; }
  QString headline() const { return m_headline; }
  QString summary() const { return m_summary; }
  bool ready() const { return m_state == QLatin1String("ready"); }

  Q_INVOKABLE void request(const QVariantMap &facts);

  static std::optional<Response> parseResponse(const QByteArray &response);
  static bool contradictsTrend(const Response &response,
                               const QString &expectedDirection);

signals:
  void changed();

private:
  QByteArray factsHash(const QVariantMap &facts) const;
  bool loadCached(const QByteArray &hash);
  void saveCached(const QByteArray &hash);
  void startCodex(const QVariantMap &facts, const QByteArray &hash);
  void finish(int exitCode, QProcess::ExitStatus exitStatus);
  void fail(const QString &state = QStringLiteral("fallback"));
  void runPending();
  QString defaultAgent() const;
  QString cachePath() const;
  static QString trendDirection(const QVariantMap &facts);

  QString m_state = QStringLiteral("idle");
  QString m_agentName;
  QString m_headline;
  QString m_summary;
  QString m_expectedTrendDirection = QStringLiteral("unknown");
  QByteArray m_requestedHash;
  QProcess m_process;
  QTimer m_timeout;
  QTemporaryFile *m_schemaFile = nullptr;
  QVariantMap m_pendingFacts;
  QByteArray m_pendingHash;
};
