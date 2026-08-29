#include "FilesystemBackend.h"

#include <QDateTime>

FilesystemBackend::FilesystemBackend(QObject *parent) : QObject(parent) {}

void FilesystemBackend::setBusy(bool busy) {
  if (m_busy == busy)
    return;
  m_busy = busy;
  emit stateChanged();
}

void FilesystemBackend::markUpdated() {
  m_lastUpdated = QDateTime::currentDateTime().toString(QStringLiteral("h:mm AP"));
}

