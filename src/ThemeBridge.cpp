#include "ThemeBridge.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

#include <algorithm>

ThemeBridge::ThemeBridge(QObject *parent) : QObject(parent) {
  m_currentDir = QDir::home().filePath(QStringLiteral(".local/state/omarchy/current"));
  m_themeDir = QDir(m_currentDir).filePath(QStringLiteral("theme"));
  m_timer.setSingleShot(true);
  m_timer.setInterval(70);
  connect(&m_timer, &QTimer::timeout, this, &ThemeBridge::reload);
  connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
          [this] { m_timer.start(); });
  connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
          [this] { m_timer.start(); });
  reload();
}

QColor ThemeBridge::alpha(const QColor &color, qreal amount) const {
  QColor result = color;
  result.setAlphaF(std::clamp(amount, 0.0, 1.0));
  return result;
}

QColor ThemeBridge::blend(const QColor &a, const QColor &b, qreal amount) const {
  const qreal t = std::clamp(amount, 0.0, 1.0);
  return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                          a.greenF() + (b.greenF() - a.greenF()) * t,
                          a.blueF() + (b.blueF() - a.blueF()) * t,
                          a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

QColor ThemeBridge::readColor(const QString &text, const QString &key,
                              const QColor &fallback) {
  const QRegularExpression re(
      QStringLiteral("^\\s*%1\\s*=\\s*[\"'](#[0-9A-Fa-f]{6})[\"']")
          .arg(QRegularExpression::escape(key)),
      QRegularExpression::MultilineOption);
  const auto match = re.match(text);
  const QColor parsed(match.hasMatch() ? match.captured(1) : QString());
  return parsed.isValid() ? parsed : fallback;
}

void ThemeBridge::rearm() {
  const auto add = [this](const QString &path) {
    if (QFileInfo::exists(path) && !m_watcher.files().contains(path) &&
        !m_watcher.directories().contains(path))
      m_watcher.addPath(path);
  };
  add(m_currentDir);
  add(m_themeDir);
  add(QDir(m_themeDir).filePath(QStringLiteral("colors.toml")));
  add(QDir(m_themeDir).filePath(QStringLiteral("shell.toml")));
}

void ThemeBridge::reload() {
  rearm();
  QFile colorsFile(QDir(m_themeDir).filePath(QStringLiteral("colors.toml")));
  if (!colorsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    emit changed();
    return;
  }
  const QString text = QString::fromUtf8(colorsFile.readAll());
  m_background = readColor(text, QStringLiteral("background"), m_background);
  m_darkBackground = readColor(text, QStringLiteral("dark_background"), m_background);
  m_darkerBackground = readColor(text, QStringLiteral("darker_background"), m_darkBackground);
  m_lighterBackground = readColor(text, QStringLiteral("lighter_background"),
                                  blend(m_background, m_foreground, 0.12));
  m_foreground = readColor(text, QStringLiteral("foreground"), m_foreground);
  m_brightForeground = readColor(text, QStringLiteral("bright_foreground"), m_foreground);
  m_muted = readColor(text, QStringLiteral("muted"), m_muted);
  m_accent = readColor(text, QStringLiteral("accent"), m_accent);
  m_urgent = readColor(text, QStringLiteral("red"), m_urgent);

  QProcess hyprctl;
  hyprctl.start(QStringLiteral("hyprctl"),
                {QStringLiteral("getoption"), QStringLiteral("decoration:rounding"),
                 QStringLiteral("-j")});
  if (hyprctl.waitForFinished(400)) {
    const QString output = QString::fromUtf8(hyprctl.readAllStandardOutput());
    const QRegularExpression re(QStringLiteral("\"int\"\\s*:\\s*(\\d+)"));
    const auto match = re.match(output);
    if (match.hasMatch())
      m_radius = std::max(6, match.captured(1).toInt());
  }
  rearm();
  emit changed();
}

