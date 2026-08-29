#pragma once

#include <QList>
#include <QSet>
#include <QString>

namespace RecoveryAudit {

struct SubvolumeEntry {
  int subvolumeId = 0;
  QString path;
};

struct SnapshotSubvolume {
  int subvolumeId = 0;
  int snapshotNumber = 0;
  QString path;
};

struct SpaceUsage {
  qulonglong totalBytes = 0;
  qulonglong exclusiveBytes = 0;
  qulonglong sharedBytes = 0;
  bool valid = false;
};

struct SubvolumeDetails {
  int subvolumeId = 0;
  QString creationTime;
  QString uuid;
  bool readOnly = false;
  bool valid = false;
};

QList<SubvolumeEntry> parseSubvolumes(const QString &output);
QList<SnapshotSubvolume> parseSnapshotSubvolumes(const QString &output);
QSet<int> parseSnapperNumbers(const QString &output);
SpaceUsage parseSpaceUsage(const QString &output);
SubvolumeDetails parseSubvolumeDetails(const QString &output);

} // namespace RecoveryAudit
