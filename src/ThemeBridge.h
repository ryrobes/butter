#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

class ThemeBridge : public QObject {
  Q_OBJECT

  Q_PROPERTY(QColor background READ background NOTIFY changed)
  Q_PROPERTY(QColor darkBackground READ darkBackground NOTIFY changed)
  Q_PROPERTY(QColor darkerBackground READ darkerBackground NOTIFY changed)
  Q_PROPERTY(QColor lighterBackground READ lighterBackground NOTIFY changed)
  Q_PROPERTY(QColor foreground READ foreground NOTIFY changed)
  Q_PROPERTY(QColor brightForeground READ brightForeground NOTIFY changed)
  Q_PROPERTY(QColor muted READ muted NOTIFY changed)
  Q_PROPERTY(QColor accent READ accent NOTIFY changed)
  Q_PROPERTY(QColor urgent READ urgent NOTIFY changed)
  Q_PROPERTY(int radius READ radius NOTIFY changed)
  Q_PROPERTY(QString sansFont READ sansFont CONSTANT)
  Q_PROPERTY(QString monoFont READ monoFont CONSTANT)

public:
  explicit ThemeBridge(QObject *parent = nullptr);

  QColor background() const { return m_background; }
  QColor darkBackground() const { return m_darkBackground; }
  QColor darkerBackground() const { return m_darkerBackground; }
  QColor lighterBackground() const { return m_lighterBackground; }
  QColor foreground() const { return m_foreground; }
  QColor brightForeground() const { return m_brightForeground; }
  QColor muted() const { return m_muted; }
  QColor accent() const { return m_accent; }
  QColor urgent() const { return m_urgent; }
  int radius() const { return m_radius; }
  QString sansFont() const { return QStringLiteral("sans-serif"); }
  QString monoFont() const { return QStringLiteral("monospace"); }

  Q_INVOKABLE QColor alpha(const QColor &color, qreal amount) const;
  Q_INVOKABLE QColor blend(const QColor &a, const QColor &b, qreal amount) const;

signals:
  void changed();

private:
  void reload();
  void rearm();
  static QColor readColor(const QString &text, const QString &key,
                          const QColor &fallback);

  QFileSystemWatcher m_watcher;
  QTimer m_timer;
  QString m_currentDir;
  QString m_themeDir;
  QColor m_background{QStringLiteral("#080605")};
  QColor m_darkBackground{QStringLiteral("#050403")};
  QColor m_darkerBackground{QStringLiteral("#030202")};
  QColor m_lighterBackground{QStringLiteral("#211d19")};
  QColor m_foreground{QStringLiteral("#f6ead8")};
  QColor m_brightForeground{QStringLiteral("#fff7ea")};
  QColor m_muted{QStringLiteral("#8e857a")};
  QColor m_accent{QStringLiteral("#d5aa73")};
  QColor m_urgent{QStringLiteral("#d98972")};
  int m_radius = 12;
};

