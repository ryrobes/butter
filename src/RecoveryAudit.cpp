#include "RecoveryAudit.h"

#include <QRegularExpression>

namespace RecoveryAudit {

QList<SubvolumeEntry> parseSubvolumes(const QString &output) {
  QList<SubvolumeEntry> subvolumes;
  const QRegularExpression linePattern(
      QStringLiteral("^\\s*(\\d+)\\s+\\d+\\s+\\d+\\s+(.+?)\\s*$"),
      QRegularExpression::MultilineOption);
  auto lines = linePattern.globalMatch(output);
  while (lines.hasNext()) {
    const auto line = lines.next();
    SubvolumeEntry item;
    item.subvolumeId = line.captured(1).toInt();
    item.path = line.captured(2);
    subvolumes.push_back(item);
  }
  return subvolumes;
}

QList<SnapshotSubvolume> parseSnapshotSubvolumes(const QString &output) {
  QList<SnapshotSubvolume> snapshots;
  const QRegularExpression snapshotPattern(
      QStringLiteral("(?:^|/)\\.snapshots/(\\d+)/snapshot$"));
  for (const SubvolumeEntry &subvolume : parseSubvolumes(output)) {
    const auto snapshot = snapshotPattern.match(subvolume.path);
    if (!snapshot.hasMatch())
      continue;
    snapshots.push_back(
        {subvolume.subvolumeId, snapshot.captured(1).toInt(), subvolume.path});
  }
  return snapshots;
}

QSet<int> parseSnapperNumbers(const QString &output) {
  QSet<int> numbers;
  const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  for (QString line : lines) {
    line = line.trimmed();
    line.remove(QLatin1Char('"'));
    bool ok = false;
    const int number =
        line.section(QLatin1Char(','), 0, 0).trimmed().toInt(&ok);
    if (ok && number > 0)
      numbers.insert(number);
  }
  return numbers;
}

SpaceUsage parseSpaceUsage(const QString &output) {
  SpaceUsage usage;
  const QRegularExpression linePattern(
      QStringLiteral("^\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+.+$"),
      QRegularExpression::MultilineOption);
  const auto match = linePattern.match(output);
  if (!match.hasMatch())
    return usage;

  usage.totalBytes = match.captured(1).toULongLong();
  usage.exclusiveBytes = match.captured(2).toULongLong();
  usage.sharedBytes = match.captured(3).toULongLong();
  usage.valid = true;
  return usage;
}

SubvolumeDetails parseSubvolumeDetails(const QString &output) {
  SubvolumeDetails details;
  const auto capture = [&output](const QString &label) {
    const QRegularExpression pattern(
        QStringLiteral("^\\s*%1:\\s*(.+?)\\s*$")
            .arg(QRegularExpression::escape(label)),
        QRegularExpression::MultilineOption);
    const auto match = pattern.match(output);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
  };

  bool idValid = false;
  details.subvolumeId = capture(QStringLiteral("Subvolume ID")).toInt(&idValid);
  details.creationTime = capture(QStringLiteral("Creation time"));
  details.uuid = capture(QStringLiteral("UUID"));
  const QString flags = capture(QStringLiteral("Flags"));
  details.readOnly = flags
                         .split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                Qt::SkipEmptyParts)
                         .contains(QStringLiteral("readonly"));
  details.valid = idValid && !details.uuid.isEmpty();
  return details;
}

} // namespace RecoveryAudit
