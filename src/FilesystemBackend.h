#pragma once

#include <QObject>
#include <QString>

class FilesystemBackend : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString providerId READ providerId CONSTANT)
  Q_PROPERTY(QString providerName READ providerName CONSTANT)
  Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
  Q_PROPERTY(QString lastUpdated READ lastUpdated NOTIFY stateChanged)

public:
  explicit FilesystemBackend(QObject *parent = nullptr);
  ~FilesystemBackend() override = default;

  virtual QString providerId() const = 0;
  virtual QString providerName() const = 0;
  bool busy() const { return m_busy; }
  QString lastUpdated() const { return m_lastUpdated; }

  Q_INVOKABLE virtual void refresh() = 0;

signals:
  void stateChanged();

protected:
  void setBusy(bool busy);
  void markUpdated();

private:
  bool m_busy = false;
  QString m_lastUpdated;
};

