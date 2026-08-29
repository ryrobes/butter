import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs

Window {
    id: root
    width: 1100
    height: 720
    minimumWidth: 760
    minimumHeight: 560
    visible: true
    title: "Butter"
    color: "transparent"
    property bool reviewExpanded: false
    property var reviewedSpaceFinding: ({})
    property var shadowCandidate: ({})
    property bool shadowFiltersCaptured: false
    readonly property var reviewedFinding: filesystem.auditFindingCount > 0
                                           ? filesystem.auditFindings[0] : ({})

    function formatBytes(bytes) {
        if (!bytes || bytes < 0) return "—"
        var units = ["B", "KB", "MB", "GB", "TB"]
        var value = Number(bytes)
        var unit = 0
        while (value >= 1000 && unit < units.length - 1) {
            value /= 1000
            unit++
        }
        var digits = value >= 100 || unit === 0 ? 0 : 1
        return value.toFixed(digits) + " " + units[unit]
    }

    readonly property color statusColor:
        filesystem.severity === "danger" ? appTheme.urgent
        : filesystem.severity === "warning"
          ? appTheme.blend(appTheme.accent, appTheme.foreground, 0.34)
          : appTheme.accent

    Rectangle {
        anchors.fill: parent
        color: appTheme.alpha(appTheme.background, 0.96)

        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: appTheme.alpha(appTheme.lighterBackground, 0.91) }
            GradientStop { position: 0.32; color: appTheme.alpha(appTheme.background, 0.94) }
            GradientStop { position: 1.0; color: appTheme.alpha(appTheme.darkerBackground, 0.97) }
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: appTheme.alpha(appTheme.accent, 0.40)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 76

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 26
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12

                ButterMark { anchors.verticalCenter: parent.verticalCenter }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    ButterWordmark {}
                    Text {
                        text: "YOUR FILESYSTEM, EXPLAINED."
                        color: appTheme.muted
                        font.family: appTheme.monoFont
                        font.pixelSize: 9
                        font.letterSpacing: 1.0
                    }
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 26
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: readonlyLabel.implicitWidth + 20
                    height: 30
                    radius: 15
                    color: appTheme.alpha(appTheme.accent, 0.10)
                    border.width: 1
                    border.color: appTheme.alpha(appTheme.accent, 0.28)
                    Text {
                        id: readonlyLabel
                        anchors.centerIn: parent
                        text: "READ ONLY"
                        color: appTheme.blend(appTheme.accent, appTheme.foreground, 0.42)
                        font.family: appTheme.monoFont
                        font.pixelSize: 9
                        font.letterSpacing: 1.2
                    }
                }

                ChromeButton {
                    label: filesystem.busy ? "Checking" : "Check again"
                    glyph: filesystem.busy ? "·" : "↻"
                    enabled: !filesystem.busy
                    onTriggered: filesystem.refresh()
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: appTheme.alpha(appTheme.foreground, 0.10)
            }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Item {
                width: scroll.availableWidth
                implicitHeight: content.implicitHeight + 52

                ColumnLayout {
                    id: content
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 26
                    spacing: 16

                    Surface {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 266
                        elevated: true
                        tint: appTheme.alpha(appTheme.blend(appTheme.lighterBackground,
                                                            root.statusColor, 0.08), 0.78)

                        SpaceHistoryArea {
                            anchors.fill: parent
                            anchors.margins: 1
                            samples: filesystem.spaceHistory
                            traceColor: root.statusColor
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 28
                            spacing: 26

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 0

                                Text {
                                    text: "RIGHT NOW"
                                    color: root.statusColor
                                    font.family: appTheme.monoFont
                                    font.pixelSize: 10
                                    font.letterSpacing: 1.6
                                }

                                Item { Layout.preferredHeight: 13 }

                                Text {
                                    Layout.fillWidth: true
                                    text: filesystem.verdict
                                    color: appTheme.brightForeground
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 34
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.WordWrap
                                }

                                Item { Layout.preferredHeight: 8 }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 620
                                    text: filesystem.verdictDetail
                                    color: appTheme.blend(appTheme.foreground, appTheme.muted, 0.20)
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 15
                                    lineHeight: 1.34
                                    wrapMode: Text.WordWrap
                                }

                                Item { Layout.fillHeight: true }

                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Repeater {
                                        model: [
                                            { text: filesystem.deviceErrors === 0
                                                    ? "No device errors reported"
                                                    : filesystem.deviceErrors + " device errors",
                                              good: filesystem.deviceErrors === 0 },
                                            { text: filesystem.recoveryCount + " startup recovery points",
                                              good: filesystem.recoveryCount > 0 },
                                            { text: filesystem.timelineEnabled
                                                    ? "Hourly copies are on"
                                                    : "No hourly copies",
                                              good: !filesystem.timelineEnabled }
                                        ]

                                        Rectangle {
                                            required property var modelData
                                            width: chipText.implicitWidth + 24
                                            height: 30
                                            radius: 15
                                            color: appTheme.alpha(modelData.good ? appTheme.accent
                                                                                 : appTheme.urgent, 0.09)
                                            border.width: 1
                                            border.color: appTheme.alpha(modelData.good ? appTheme.accent
                                                                                        : appTheme.urgent, 0.22)
                                            Text {
                                                id: chipText
                                                anchors.centerIn: parent
                                                text: (modelData.good ? "✓  " : "!  ") + modelData.text
                                                color: appTheme.foreground
                                                font.family: appTheme.sansFont
                                                font.pixelSize: 11
                                            }
                                        }
                                    }
                                }
                            }

                            SpaceArc {
                                Layout.preferredWidth: 182
                                Layout.preferredHeight: 182
                                Layout.alignment: Qt.AlignVCenter
                                percent: filesystem.usedPercent
                                progressColor: root.statusColor
                                primary: root.formatBytes(filesystem.freeBytes)
                                secondary: "free"
                                tertiary: "of " + root.formatBytes(filesystem.totalBytes) + " total"
                            }
                        }
                    }

                    Surface {
                        Layout.fillWidth: true
                        Layout.preferredHeight: filesystem.auditState === "verified" &&
                                                filesystem.auditFindingCount > 0 ? 128 : 108
                        tint: filesystem.auditState === "verified" &&
                              filesystem.auditFindingCount > 0
                              ? appTheme.alpha(appTheme.blend(appTheme.darkBackground,
                                                              root.statusColor, 0.10), 0.78)
                              : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 18

                            Rectangle {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                Layout.alignment: Qt.AlignVCenter
                                radius: 21
                                color: appTheme.alpha(filesystem.auditState === "verified" &&
                                                      filesystem.auditFindingCount === 0
                                                      ? appTheme.accent : root.statusColor, 0.11)
                                border.width: 1
                                border.color: appTheme.alpha(filesystem.auditState === "verified" &&
                                                             filesystem.auditFindingCount === 0
                                                             ? appTheme.accent : root.statusColor, 0.28)
                                Text {
                                    anchors.centerIn: parent
                                    text: filesystem.auditState === "running" ? "···"
                                          : filesystem.auditState === "verified" &&
                                            filesystem.auditFindingCount === 0 ? "✓" : "⌁"
                                    color: filesystem.auditState === "verified" &&
                                           filesystem.auditFindingCount === 0
                                           ? appTheme.accent : root.statusColor
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 17
                                    font.weight: Font.DemiBold
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                Text {
                                    text: "DEEP RECOVERY CHECK"
                                    color: appTheme.muted
                                    font.family: appTheme.monoFont
                                    font.pixelSize: 9
                                    font.letterSpacing: 1.4
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: filesystem.auditTitle
                                    color: appTheme.brightForeground
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 17
                                    font.weight: Font.DemiBold
                                    wrapMode: Text.WordWrap
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: filesystem.auditDetail
                                    color: appTheme.muted
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                            }

                            ChromeButton {
                                Layout.alignment: Qt.AlignVCenter
                                visible: filesystem.auditState !== "verified"
                                enabled: filesystem.auditState !== "running"
                                label: filesystem.auditState === "running" ? "Checking"
                                       : filesystem.auditState === "cancelled" ||
                                         filesystem.auditState === "error" ? "Try again"
                                       : "Verify copies"
                                glyph: filesystem.auditState === "running" ? "·" : "✓"
                                onTriggered: filesystem.verifyRecovery()
                            }

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                visible: filesystem.auditState === "verified" &&
                                         filesystem.auditFindingCount === 0
                                implicitWidth: verifiedText.implicitWidth + 22
                                implicitHeight: 30
                                radius: 15
                                color: appTheme.alpha(filesystem.auditFindingCount === 0
                                                      ? appTheme.accent : root.statusColor, 0.09)
                                border.width: 1
                                border.color: appTheme.alpha(filesystem.auditFindingCount === 0
                                                             ? appTheme.accent : root.statusColor, 0.24)
                                Text {
                                    id: verifiedText
                                    anchors.centerIn: parent
                                    text: "VERIFIED"
                                    color: appTheme.foreground
                                    font.family: appTheme.monoFont
                                    font.pixelSize: 9
                                    font.letterSpacing: 1.0
                                }
                            }

                            ChromeButton {
                                Layout.alignment: Qt.AlignVCenter
                                visible: filesystem.auditState === "verified" &&
                                         filesystem.auditFindingCount > 0
                                label: root.reviewExpanded ? "Close" : "Review"
                                glyph: root.reviewExpanded ? "×" : "→"
                                onTriggered: root.reviewExpanded = !root.reviewExpanded
                            }
                        }
                    }

                    Surface {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 294
                        visible: root.reviewExpanded && filesystem.auditFindingCount > 0
                        tint: appTheme.alpha(appTheme.lighterBackground, 0.32)

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 22
                            spacing: 28

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.preferredWidth: 590
                                spacing: 0

                                Text {
                                    text: "EVIDENCE"
                                    color: appTheme.muted
                                    font.family: appTheme.monoFont
                                    font.pixelSize: 9
                                    font.letterSpacing: 1.4
                                }
                                Item { Layout.preferredHeight: 7 }
                                Text {
                                    text: "Snapshot " + (root.reviewedFinding.snapshotNumber || "—")
                                    color: appTheme.brightForeground
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 22
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: root.reviewedFinding.creationTime
                                          ? "Created " + root.reviewedFinding.creationTime
                                          : "Creation time unavailable"
                                    color: appTheme.muted
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 11
                                }
                                Item { Layout.preferredHeight: 13 }

                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: 2
                                    columnSpacing: 22
                                    rowSpacing: 8

                                    Repeater {
                                        model: [
                                            { label: "BTRFS · FILESYSTEM ID",
                                              value: String(root.reviewedFinding.subvolumeId || "—") },
                                            { label: "UNIQUE SPACE",
                                              value: root.formatBytes(root.reviewedFinding.exclusiveBytes) },
                                            { label: "SNAPPER · TRACKER",
                                              value: root.reviewedFinding.metadata === "empty" ? "Empty"
                                                     : root.reviewedFinding.metadata === "missing" ? "Missing"
                                                     : "Present" },
                                            { label: "LIMINE · STARTUP",
                                              value: root.reviewedFinding.bootVisible ? "Visible" : "Not present" },
                                            { label: "SNAPSHOT MODE",
                                              value: root.reviewedFinding.readOnly ? "Read-only" : "Writable" },
                                            { label: "TOTAL REFERENCED",
                                              value: root.formatBytes(root.reviewedFinding.totalBytes) }
                                        ]

                                        Column {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Text {
                                                text: modelData.label
                                                color: appTheme.alpha(appTheme.muted, 0.86)
                                                font.family: appTheme.monoFont
                                                font.pixelSize: 9
                                                font.letterSpacing: 0.8
                                            }
                                            Text {
                                                text: modelData.value
                                                color: appTheme.foreground
                                                font.family: appTheme.sansFont
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                            }
                                        }
                                    }
                                }

                                Item { Layout.fillHeight: true }
                                Flow {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 25
                                    spacing: 6

                                    Repeater {
                                        model: [
                                            "Btrfs · stores your files",
                                            "Snapper · tracks recovery copies",
                                            "Limine · offers them at startup"
                                        ]
                                        Rectangle {
                                            required property string modelData
                                            width: decodedText.implicitWidth + 16
                                            height: 23
                                            radius: 12
                                            color: appTheme.alpha(appTheme.foreground, 0.055)
                                            border.width: 1
                                            border.color: appTheme.alpha(appTheme.foreground, 0.11)
                                            Text {
                                                id: decodedText
                                                anchors.centerIn: parent
                                                text: modelData
                                                color: appTheme.alpha(appTheme.foreground, 0.76)
                                                font.family: appTheme.sansFont
                                                font.pixelSize: 10
                                            }
                                        }
                                    }
                                }
                                Item { Layout.preferredHeight: 5 }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Exclusive space is an estimate of what can be reclaimed. " +
                                          "Btrfs may release it gradually after removal."
                                    color: appTheme.muted
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                }
                            }

                            Rectangle {
                                Layout.fillHeight: true
                                Layout.preferredWidth: 350
                                radius: appTheme.radius
                                color: appTheme.alpha(appTheme.darkBackground, 0.58)
                                border.width: 1
                                border.color: appTheme.alpha(root.reviewedFinding.remedyEligible
                                                             ? appTheme.accent : appTheme.urgent, 0.22)

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    spacing: 0

                                    Text {
                                        text: "REMEDY"
                                        color: appTheme.muted
                                        font.family: appTheme.monoFont
                                        font.pixelSize: 9
                                        font.letterSpacing: 1.4
                                    }
                                    Item { Layout.preferredHeight: 8 }
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.reviewedFinding.remedyEligible
                                              ? "Safe for Butter to remove"
                                              : "Needs an Omarchy agent"
                                        color: appTheme.brightForeground
                                        font.family: appTheme.sansFont
                                        font.pixelSize: 17
                                        font.weight: Font.DemiBold
                                        wrapMode: Text.WordWrap
                                    }
                                    Item { Layout.preferredHeight: 6 }
                                    Text {
                                        Layout.fillWidth: true
                                        text: root.reviewedFinding.remedyEligible
                                              ? "Before removal, Butter will verify every condition again and stop if anything changed."
                                              : "Butter cannot prove automatic removal is safe. Copy the evidence for your Omarchy agent."
                                        color: appTheme.muted
                                        font.family: appTheme.sansFont
                                        font.pixelSize: 11
                                        lineHeight: 1.3
                                        wrapMode: Text.WordWrap
                                    }
                                    Item { Layout.preferredHeight: 9 }
                                    Text {
                                        Layout.fillWidth: true
                                        visible: filesystem.remedyState !== "idle"
                                        text: filesystem.remedyTitle + "\n" + filesystem.remedyDetail
                                        color: filesystem.remedyState === "error"
                                               ? appTheme.urgent : appTheme.foreground
                                        font.family: appTheme.sansFont
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }
                                    Item { Layout.fillHeight: true }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        ChromeButton {
                                            Layout.fillWidth: true
                                            quiet: true
                                            label: filesystem.reportCopied ? "Copied" : "Copy report"
                                            glyph: filesystem.reportCopied ? "✓" : "⧉"
                                            onTriggered: filesystem.copyAuditReport()
                                        }
                                        ChromeButton {
                                            Layout.fillWidth: true
                                            visible: Boolean(root.reviewedFinding.remedyEligible)
                                            enabled: filesystem.remedyState !== "running"
                                            label: filesystem.remedyState === "running"
                                                   ? "Removing" : "Remove safely"
                                            glyph: filesystem.remedyState === "running" ? "·" : "−"
                                            onTriggered: removeDialog.open()
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 218
                        spacing: 16

                        Surface {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 650

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 22
                                spacing: 0

                                Text {
                                    text: "RECOVERY"
                                    color: appTheme.muted
                                    font.family: appTheme.monoFont
                                    font.pixelSize: 9
                                    font.letterSpacing: 1.4
                                }
                                Item { Layout.preferredHeight: 8 }
                                Text {
                                    text: filesystem.recoveryTitle
                                    color: appTheme.brightForeground
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 22
                                    font.weight: Font.DemiBold
                                }
                                Item { Layout.preferredHeight: 5 }
                                Text {
                                    Layout.fillWidth: true
                                    text: filesystem.recoveryDetail
                                    color: appTheme.muted
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                                Item { Layout.preferredHeight: 12 }
                                RecoveryRail {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    points: filesystem.recoveryPoints
                                }
                            }
                        }

                        Surface {
                            Layout.fillHeight: true
                            Layout.preferredWidth: 330

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 22
                                spacing: 0

                                Text {
                                    text: "SPACE"
                                    color: appTheme.muted
                                    font.family: appTheme.monoFont
                                    font.pixelSize: 9
                                    font.letterSpacing: 1.4
                                }
                                Item { Layout.preferredHeight: 10 }
                                Text {
                                    text: Math.round(filesystem.usedPercent) + "% used"
                                    color: appTheme.brightForeground
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 22
                                    font.weight: Font.DemiBold
                                }
                                Item { Layout.preferredHeight: 13 }
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 9
                                    radius: 5
                                    color: appTheme.alpha(appTheme.foreground, 0.09)
                                    Rectangle {
                                        width: parent.width * Math.max(0, Math.min(100,
                                                               filesystem.usedPercent)) / 100
                                        height: parent.height
                                        radius: parent.radius
                                        color: root.statusColor
                                    }
                                }
                                Item { Layout.preferredHeight: 13 }
                                Text {
                                    Layout.fillWidth: true
                                    text: "Btrfs, the filesystem storing your files, has filled " + Math.round(filesystem.dataChunkPercent) +
                                          "% of the space already set aside for files. " +
                                          root.formatBytes(filesystem.freeBytes) + " remains available."
                                    color: appTheme.muted
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 12
                                    lineHeight: 1.34
                                    wrapMode: Text.WordWrap
                                }
                                Item { Layout.fillHeight: true }
                                Text {
                                    text: root.formatBytes(filesystem.usedBytes) + " of " +
                                          root.formatBytes(filesystem.totalBytes)
                                    color: appTheme.alpha(appTheme.foreground, 0.68)
                                    font.family: appTheme.monoFont
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }

                    Surface {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 124
                        elevated: homeShadow.state === "current" ||
                                  homeShadow.state === "running"
                        tint: homeShadow.state === "current"
                              ? appTheme.alpha(appTheme.blend(appTheme.darkBackground,
                                                              appTheme.accent, 0.08), 0.78)
                              : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 17

                            Rectangle {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                Layout.alignment: Qt.AlignVCenter
                                radius: 21
                                color: appTheme.alpha(homeShadow.state === "error"
                                                      ? appTheme.urgent : appTheme.accent, 0.10)
                                border.width: 1
                                border.color: appTheme.alpha(homeShadow.state === "error"
                                                             ? appTheme.urgent : appTheme.accent, 0.28)
                                Text {
                                    anchors.centerIn: parent
                                    text: homeShadow.state === "current" ? "✓"
                                          : homeShadow.state === "running" ? "···"
                                          : homeShadow.state === "error" ? "!" : "◐"
                                    color: homeShadow.state === "error"
                                           ? appTheme.urgent : appTheme.accent
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 17
                                    font.weight: Font.DemiBold
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    text: "HOME SHADOW  ·  NO AUTOMATIC PRUNING"
                                    color: appTheme.muted
                                    font.family: appTheme.monoFont
                                    font.pixelSize: 9
                                    font.letterSpacing: 1.1
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: homeShadow.title
                                    color: appTheme.brightForeground
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 17
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: homeShadow.detail
                                    color: appTheme.muted
                                    font.family: appTheme.sansFont
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            RowLayout {
                                visible: homeShadow.configured
                                Layout.preferredWidth: 250
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 12

                                ButterGlyph {
                                    Layout.preferredWidth: 54
                                    Layout.preferredHeight: 40
                                    Layout.alignment: Qt.AlignVCenter
                                    variant: "stick"
                                    lineColor: appTheme.accent
                                    opacity: 0.42
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: homeShadow.generationCount +
                                              (homeShadow.generationCount === 1
                                               ? " completed generation"
                                               : " completed generations")
                                        color: appTheme.foreground
                                        font.family: appTheme.sansFont
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: homeShadow.exclusionCount + " excluded paths" +
                                              (homeShadow.excludedBytes > 0
                                               ? "  ·  " + root.formatBytes(homeShadow.excludedBytes)
                                               : "")
                                        color: appTheme.muted
                                        font.family: appTheme.monoFont
                                        font.pixelSize: 9
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: homeShadow.lastRun.length
                                              ? "Last complete " + homeShadow.lastRun
                                              : homeShadow.nextRun
                                        color: appTheme.muted
                                        font.family: appTheme.sansFont
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            ChromeButton {
                                Layout.alignment: Qt.AlignVCenter
                                label: homeShadow.configured
                                       ? (homeShadow.busy ? "Copying"
                                          : homeShadow.state === "error"
                                            ? "Try again" : "Run now")
                                       : "Choose a drive"
                                glyph: homeShadow.configured
                                       ? (homeShadow.busy ? "·" : "↻") : "＋"
                                enabled: !homeShadow.busy &&
                                         homeShadow.state !== "waiting"
                                onTriggered: {
                                    if (homeShadow.configured)
                                        homeShadow.runNow()
                                    else
                                        shadowFolderDialog.open()
                                }
                            }
                        }
                    }

                    Surface {
                        Layout.fillWidth: true
                        Layout.preferredHeight: homeSpace.state === "idle" ? 176 : 438
                        elevated: homeSpace.state === "scanning"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 22
                            spacing: 0

                            RowLayout {
                                Layout.fillWidth: true

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Text {
                                        text: "WHERE YOUR SPACE WENT"
                                        color: appTheme.muted
                                        font.family: appTheme.monoFont
                                        font.pixelSize: 9
                                        font.letterSpacing: 1.4
                                    }
                                    Text {
                                        text: homeSpace.state === "idle"
                                              ? "Map your home folder"
                                              : homeSpace.focusLabel
                                        color: appTheme.brightForeground
                                        font.family: appTheme.sansFont
                                        font.pixelSize: 21
                                        font.weight: Font.DemiBold
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: homeSpace.state === "idle"
                                              ? "Exact paths, not broad categories. The scan stays inside Home and never asks for administrator access."
                                              : homeSpace.scanDetail
                                        color: appTheme.muted
                                        font.family: appTheme.sansFont
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                Rectangle {
                                    implicitWidth: homeOnlyText.implicitWidth + 18
                                    implicitHeight: 30
                                    radius: 15
                                    color: appTheme.alpha(appTheme.accent, 0.08)
                                    border.width: 1
                                    border.color: appTheme.alpha(appTheme.accent, 0.20)
                                    Text {
                                        id: homeOnlyText
                                        anchors.centerIn: parent
                                        text: "HOME ONLY"
                                        color: appTheme.alpha(appTheme.foreground, 0.78)
                                        font.family: appTheme.monoFont
                                        font.pixelSize: 9
                                        font.letterSpacing: 0.9
                                    }
                                }

                                ChromeButton {
                                    visible: homeSpace.state !== "scanning"
                                    label: homeSpace.state === "idle" ? "Scan home" : "Scan again"
                                    glyph: homeSpace.state === "idle" ? "→" : "↻"
                                    onTriggered: homeSpace.startScan()
                                }
                                ChromeButton {
                                    visible: homeSpace.state === "scanning"
                                    label: "Stop"
                                    glyph: "×"
                                    onTriggered: homeSpace.cancelScan()
                                }
                            }

                            Item { Layout.preferredHeight: homeSpace.state === "idle" ? 14 : 12 }

                            RowLayout {
                                Layout.fillWidth: true
                                visible: homeSpace.state === "idle"
                                spacing: 18

                                Repeater {
                                    model: [
                                        { glyph: "▦", title: "Treemap",
                                          detail: "See the shape of large paths." },
                                        { glyph: "≡", title: "Ranked paths",
                                          detail: "Compare exact sizes quickly." },
                                        { glyph: "◇", title: "Build evidence",
                                          detail: "Find output that can be rebuilt." }
                                    ]
                                    Rectangle {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 62
                                        radius: appTheme.radius
                                        color: appTheme.alpha(appTheme.foreground, 0.035)
                                        border.width: 1
                                        border.color: appTheme.alpha(appTheme.foreground, 0.09)
                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 9
                                            Text {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: modelData.glyph
                                                color: appTheme.accent
                                                font.family: appTheme.sansFont
                                                font.pixelSize: 16
                                            }
                                            Column {
                                                anchors.verticalCenter: parent.verticalCenter
                                                spacing: 1
                                                Text {
                                                    text: modelData.title
                                                    color: appTheme.foreground
                                                    font.family: appTheme.sansFont
                                                    font.pixelSize: 11
                                                    font.weight: Font.DemiBold
                                                }
                                                Text {
                                                    text: modelData.detail
                                                    color: appTheme.muted
                                                    font.family: appTheme.sansFont
                                                    font.pixelSize: 10
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                visible: homeSpace.state !== "idle"
                                spacing: 16

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.preferredWidth: 650
                                    spacing: 8

                                    RowLayout {
                                        Layout.fillWidth: true
                                        ChromeButton {
                                            visible: homeSpace.canGoUp
                                            label: "Up"
                                            glyph: "←"
                                            quiet: true
                                            onTriggered: homeSpace.goUp()
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: homeSpace.state === "scanning"
                                                  ? "The map sharpens as files arrive"
                                                  : "Click a folder to look inside"
                                            color: appTheme.muted
                                            font.family: appTheme.sansFont
                                            font.pixelSize: 10
                                        }
                                        Text {
                                            text: root.formatBytes(homeSpace.scannedBytes) + " of file contents"
                                            color: appTheme.foreground
                                            font.family: appTheme.monoFont
                                            font.pixelSize: 10
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        radius: appTheme.radius
                                        color: appTheme.alpha(appTheme.darkerBackground, 0.72)
                                        border.width: 1
                                        border.color: appTheme.alpha(appTheme.foreground, 0.10)
                                        clip: true

                                        Treemap {
                                            anchors.fill: parent
                                            anchors.margins: 5
                                            entries: homeSpace.entries
                                            onActivated: function(entry) {
                                                if (homeSpace.state !== "scanning" && entry.directory)
                                                    homeSpace.openPath(entry.path)
                                            }
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            visible: homeSpace.entries.length === 0
                                            text: homeSpace.state === "scanning"
                                                  ? "Reading folders…" : "Nothing sizeable here"
                                            color: appTheme.muted
                                            font.family: appTheme.sansFont
                                            font.pixelSize: 12
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillHeight: true
                                    Layout.preferredWidth: 340
                                    spacing: 7

                                    Text {
                                        text: "BIGGEST HERE"
                                        color: appTheme.muted
                                        font.family: appTheme.monoFont
                                        font.pixelSize: 9
                                        font.letterSpacing: 1.1
                                    }

                                    Repeater {
                                        model: Math.min(8, homeSpace.entries.length)
                                        Rectangle {
                                            required property int index
                                            readonly property var entry: homeSpace.entries[index]
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 30
                                            radius: 6
                                            color: barHover.hovered
                                                   ? appTheme.alpha(appTheme.foreground, 0.07)
                                                   : "transparent"

                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: parent.width * Math.max(0.025,
                                                       Number(entry.bytes || 0) /
                                                       Math.max(1, Number(homeSpace.entries[0].bytes || 1)))
                                                height: parent.height - 4
                                                radius: 5
                                                color: appTheme.alpha(entry.kind === "build"
                                                                      ? appTheme.urgent : appTheme.accent, 0.09)
                                            }
                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 7
                                                Text {
                                                    Layout.fillWidth: true
                                                    text: entry.name
                                                    elide: Text.ElideMiddle
                                                    color: appTheme.foreground
                                                    font.family: appTheme.sansFont
                                                    font.pixelSize: 11
                                                }
                                                Text {
                                                    text: root.formatBytes(entry.bytes)
                                                    color: appTheme.alpha(appTheme.foreground, 0.72)
                                                    font.family: appTheme.monoFont
                                                    font.pixelSize: 9
                                                }
                                            }
                                            HoverHandler { id: barHover }
                                            TapHandler {
                                                enabled: homeSpace.state !== "scanning" && entry.directory
                                                onTapped: homeSpace.openPath(entry.path)
                                            }
                                        }
                                    }

                                    Item { Layout.fillHeight: true }
                                    Text {
                                        Layout.fillWidth: true
                                        text: homeSpace.scannedFiles.toLocaleString(Qt.locale(), "f", 0) +
                                              " files  ·  " +
                                              homeSpace.scannedDirectories.toLocaleString(Qt.locale(), "f", 0) +
                                              " folders" +
                                              (homeSpace.skippedMounts > 0
                                               ? "  ·  " + homeSpace.skippedMounts + " mounted paths skipped"
                                               : "")
                                        color: appTheme.muted
                                        font.family: appTheme.monoFont
                                        font.pixelSize: 9
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }

                    Surface {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 346
                        clip: true
                        visible: homeSpace.dockerState === "ready" ||
                                 homeSpace.buildFindings.length > 0

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 22
                            spacing: 0

                            Text {
                                text: "SPACE YOU CAN GET BACK"
                                color: appTheme.muted
                                font.family: appTheme.monoFont
                                font.pixelSize: 9
                                font.letterSpacing: 1.4
                            }
                            Item { Layout.preferredHeight: 4 }
                            Text {
                                text: homeSpace.buildOutputBytes > 0 &&
                                      homeSpace.dockerReviewableBytes > 0
                                      ? root.formatBytes(homeSpace.buildOutputBytes) +
                                        " project output  ·  " +
                                        root.formatBytes(homeSpace.dockerReviewableBytes) +
                                        " Docker"
                                      : homeSpace.buildOutputBytes > 0
                                        ? root.formatBytes(homeSpace.buildOutputBytes) +
                                          " regenerable project output"
                                        : root.formatBytes(homeSpace.dockerReviewableBytes) +
                                          " worth reviewing in Docker"
                                color: appTheme.brightForeground
                                font.family: appTheme.sansFont
                                font.pixelSize: 20
                                font.weight: Font.DemiBold
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Butter separates rebuildable weight from data that may be irreplaceable. Known regenerable output can be reviewed before cleanup."
                                color: appTheme.muted
                                font.family: appTheme.sansFont
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }
                            Item { Layout.preferredHeight: 13 }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 16

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    radius: appTheme.radius
                                    color: appTheme.alpha(appTheme.darkBackground, 0.52)
                                    border.width: 1
                                    border.color: appTheme.alpha(appTheme.foreground, 0.10)

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 14
                                        spacing: 5
                                        Text {
                                            text: "PROJECT OUTPUT"
                                            color: appTheme.muted
                                            font.family: appTheme.monoFont
                                            font.pixelSize: 9
                                            font.letterSpacing: 1.0
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            visible: homeSpace.buildFindings.length === 0
                                            text: homeSpace.state === "scanning"
                                                  ? "Build evidence appears when the scan finishes."
                                                  : "Scan Home to find project output and dependency trees."
                                            color: appTheme.muted
                                            font.family: appTheme.sansFont
                                            font.pixelSize: 11
                                            wrapMode: Text.WordWrap
                                        }
                                        Repeater {
                                            model: Math.min(4, homeSpace.buildFindings.length)
                                            Rectangle {
                                                required property int index
                                                readonly property var finding: homeSpace.buildFindings[index]
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 43
                                                color: "transparent"
                                                RowLayout {
                                                    anchors.fill: parent
                                                    spacing: 9
                                                    Rectangle {
                                                        Layout.preferredWidth: 7
                                                        Layout.preferredHeight: 7
                                                        radius: 4
                                                        color: appTheme.accent
                                                    }
                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 0
                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: finding.title
                                                            elide: Text.ElideRight
                                                            color: appTheme.foreground
                                                            font.family: appTheme.sansFont
                                                            font.pixelSize: 11
                                                            font.weight: Font.DemiBold
                                                        }
                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: finding.path
                                                            elide: Text.ElideMiddle
                                                            color: appTheme.muted
                                                            font.family: appTheme.monoFont
                                                            font.pixelSize: 9
                                                        }
                                                    }
                                                    Text {
                                                        text: root.formatBytes(finding.bytes)
                                                        color: appTheme.foreground
                                                        font.family: appTheme.monoFont
                                                        font.pixelSize: 10
                                                    }
                                                    ChromeButton {
                                                        implicitHeight: 27
                                                        label: "Review"
                                                        quiet: true
                                                        onTriggered: {
                                                            root.reviewedSpaceFinding = finding
                                                            artifactDialog.open()
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        Item { Layout.fillHeight: true }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    radius: appTheme.radius
                                    color: appTheme.alpha(appTheme.darkBackground, 0.52)
                                    border.width: 1
                                    border.color: appTheme.alpha(appTheme.foreground, 0.10)

                                    ColumnLayout {
                                        anchors.fill: parent
                                        anchors.margins: 14
                                        spacing: 4
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Text {
                                                Layout.fillWidth: true
                                                text: "DOCKER · CONTAINERS"
                                                color: appTheme.muted
                                                font.family: appTheme.monoFont
                                                font.pixelSize: 9
                                                font.letterSpacing: 1.0
                                            }
                                            ChromeButton {
                                                implicitHeight: 26
                                                label: "Refresh"
                                                quiet: true
                                                onTriggered: homeSpace.refreshDocker()
                                            }
                                        }
                                        Repeater {
                                            model: homeSpace.dockerFindings
                                            Rectangle {
                                                required property var modelData
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 42
                                                color: "transparent"
                                                RowLayout {
                                                    anchors.fill: parent
                                                    spacing: 8
                                                    Rectangle {
                                                        Layout.preferredWidth: 7
                                                        Layout.preferredHeight: 7
                                                        radius: 4
                                                        color: modelData.safety === "protected"
                                                               ? appTheme.urgent
                                                               : modelData.safety === "regenerable"
                                                                 ? appTheme.accent
                                                                 : appTheme.blend(appTheme.accent,
                                                                                  appTheme.foreground, 0.44)
                                                    }
                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 0
                                                        Text {
                                                            text: modelData.title
                                                            color: appTheme.foreground
                                                            font.family: appTheme.sansFont
                                                            font.pixelSize: 11
                                                            font.weight: Font.DemiBold
                                                        }
                                                        Text {
                                                            Layout.fillWidth: true
                                                            text: modelData.safety === "protected"
                                                                  ? "Protected · agent review"
                                                                  : modelData.safety === "regenerable"
                                                                    ? "Regenerable · slower next build"
                                                                    : "Review before removal"
                                                            color: appTheme.muted
                                                            font.family: appTheme.sansFont
                                                            font.pixelSize: 10
                                                        }
                                                    }
                                                    Text {
                                                        text: root.formatBytes(modelData.reclaimableBytes)
                                                        color: appTheme.foreground
                                                        font.family: appTheme.monoFont
                                                        font.pixelSize: 10
                                                    }
                                                }
                                            }
                                        }
                                        Item { Layout.fillHeight: true }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: appTheme.alpha(appTheme.darkerBackground, 0.90)

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: appTheme.alpha(appTheme.foreground, 0.10)
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 26
                anchors.verticalCenter: parent.verticalCenter
                text: "Read-only  ·  " + filesystem.providerName + " on " + filesystem.source
                color: appTheme.muted
                font.family: appTheme.monoFont
                font.pixelSize: 10
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 26
                anchors.verticalCenter: parent.verticalCenter
                text: filesystem.lastUpdated.length ? "Checked " + filesystem.lastUpdated : "Checking…"
                color: appTheme.muted
                font.family: appTheme.sansFont
                font.pixelSize: 10
            }
        }
    }

    Connections {
        target: homeSpace
        function onStateChanged() {
            if (homeSpace.state === "scanning") {
                root.shadowFiltersCaptured = false
            } else if (homeShadow.configured &&
                       !root.shadowFiltersCaptured &&
                       (homeSpace.state === "complete" ||
                        homeSpace.state === "cancelled")) {
                homeShadow.updateExclusions(homeSpace.buildFindings)
                root.shadowFiltersCaptured = true
            }
        }
    }

    FolderDialog {
        id: shadowFolderDialog
        title: "Choose a folder on your second drive"
        onAccepted: {
            root.shadowCandidate = homeShadow.inspectDestination(selectedFolder)
            shadowSetupDialog.open()
        }
    }

    Dialog {
        id: shadowSetupDialog
        anchors.centerIn: parent
        width: Math.min(560, root.width - 48)
        height: root.shadowCandidate.valid ? 392 : 268
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 0

        background: Rectangle {
            radius: appTheme.radius + 2
            color: appTheme.darkerBackground
            border.width: 1
            border.color: appTheme.alpha(root.shadowCandidate.valid
                                         ? appTheme.accent : appTheme.urgent, 0.42)
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 0

            Text {
                text: root.shadowCandidate.valid ? "CREATE HOME SHADOW"
                                                  : "DESTINATION NOT READY"
                color: root.shadowCandidate.valid ? appTheme.accent : appTheme.urgent
                font.family: appTheme.monoFont
                font.pixelSize: 9
                font.letterSpacing: 1.4
            }
            Item { Layout.preferredHeight: 9 }
            RowLayout {
                Layout.fillWidth: true
                spacing: 18

                Text {
                    Layout.fillWidth: true
                    text: root.shadowCandidate.title || "Choose another folder"
                    color: appTheme.brightForeground
                    font.family: appTheme.sansFont
                    font.pixelSize: 23
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }

                ButterGlyph {
                    visible: Boolean(root.shadowCandidate.valid)
                    Layout.preferredWidth: 70
                    Layout.preferredHeight: 51
                    variant: "stick"
                    lineColor: appTheme.accent
                    opacity: 0.58
                }
            }
            Item { Layout.preferredHeight: 7 }
            Text {
                Layout.fillWidth: true
                text: root.shadowCandidate.detail || ""
                color: appTheme.foreground
                font.family: root.shadowCandidate.valid
                             ? appTheme.monoFont : appTheme.sansFont
                font.pixelSize: root.shadowCandidate.valid ? 10 : 12
                wrapMode: Text.WrapAnywhere
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: 14
                visible: Boolean(root.shadowCandidate.valid)
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: "Butter will make a new immutable generation each day the drive is connected. Changed and deleted files remain in earlier generations; completed generations are never pruned automatically."
                    color: appTheme.foreground
                    font.family: appTheme.sansFont
                    font.pixelSize: 12
                    lineHeight: 1.32
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    text: "The first copy starts now. Later copies hard-link unchanged files. Butter currently knows " +
                          homeSpace.buildFindings.length + " rebuildable/cache locations to consider, plus its safe baseline exclusions."
                    color: appTheme.muted
                    font.family: appTheme.sansFont
                    font.pixelSize: 11
                    lineHeight: 1.3
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    text: "Butter does not encrypt the destination; its privacy depends on the drive you chose."
                    color: appTheme.muted
                    font.family: appTheme.sansFont
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
                Text {
                    text: (root.shadowCandidate.filesystem || "filesystem") +
                          "  ·  " + (root.shadowCandidate.drive || "second drive") +
                          "  ·  no administrator access"
                    color: appTheme.muted
                    font.family: appTheme.monoFont
                    font.pixelSize: 9
                }
            }

            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }
                ChromeButton {
                    label: root.shadowCandidate.valid ? "Cancel" : "Close"
                    quiet: true
                    onTriggered: shadowSetupDialog.close()
                }
                ChromeButton {
                    visible: Boolean(root.shadowCandidate.valid)
                    label: "Create and start"
                    glyph: "◐"
                    onTriggered: {
                        shadowSetupDialog.close()
                        homeShadow.configure(shadowFolderDialog.selectedFolder,
                                             homeSpace.buildFindings)
                    }
                }
            }
        }
    }

    Dialog {
        id: removeDialog
        anchors.centerIn: parent
        width: Math.min(500, root.width - 48)
        height: 286
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 0

        background: Rectangle {
            radius: appTheme.radius + 2
            color: appTheme.darkerBackground
            border.width: 1
            border.color: appTheme.alpha(appTheme.urgent, 0.42)
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 0

            Text {
                text: "REMOVE RECOVERY COPY"
                color: appTheme.urgent
                font.family: appTheme.monoFont
                font.pixelSize: 9
                font.letterSpacing: 1.4
            }
            Item { Layout.preferredHeight: 10 }
            Text {
                Layout.fillWidth: true
                text: "Remove snapshot " + (root.reviewedFinding.snapshotNumber || "—") + "?"
                color: appTheme.brightForeground
                font.family: appTheme.sansFont
                font.pixelSize: 23
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }
            Item { Layout.preferredHeight: 7 }
            Text {
                Layout.fillWidth: true
                text: "This cannot be undone. Butter will first repeat every safety check using the exact subvolume identity you reviewed. If any evidence differs, removal stops."
                color: appTheme.foreground
                font.family: appTheme.sansFont
                font.pixelSize: 12
                lineHeight: 1.35
                wrapMode: Text.WordWrap
            }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }
                ChromeButton {
                    label: "Cancel"
                    quiet: true
                    onTriggered: removeDialog.close()
                }
                ChromeButton {
                    label: "Remove verified copy"
                    glyph: "−"
                    onTriggered: {
                        var snapshotNumber = Number(root.reviewedFinding.snapshotNumber)
                        removeDialog.close()
                        filesystem.removeOrphan(snapshotNumber)
                    }
                }
            }
        }
    }

    Dialog {
        id: artifactDialog
        readonly property bool hasSelection:
            Boolean(root.reviewedSpaceFinding && root.reviewedSpaceFinding.path)
        readonly property bool cleanupForSelection:
            hasSelection && homeSpace.cleanupPath === root.reviewedSpaceFinding.path
        anchors.centerIn: parent
        width: Math.min(560, root.width - 48)
        height: 410
        modal: true
        focus: true
        closePolicy: cleanupForSelection && homeSpace.cleanupState === "running"
                     ? Popup.NoAutoClose : Popup.CloseOnEscape
        padding: 0

        background: Rectangle {
            radius: appTheme.radius + 2
            color: appTheme.darkerBackground
            border.width: 1
            border.color: appTheme.alpha(appTheme.accent, 0.40)
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: 24
            spacing: 0

            Text {
                text: root.reviewedSpaceFinding.safety === "regenerable"
                      ? "REGENERABLE OUTPUT" : "REVIEW REQUIRED"
                color: root.reviewedSpaceFinding.safety === "regenerable"
                       ? appTheme.accent : appTheme.urgent
                font.family: appTheme.monoFont
                font.pixelSize: 9
                font.letterSpacing: 1.4
            }
            Item { Layout.preferredHeight: 9 }
            Text {
                Layout.fillWidth: true
                text: root.reviewedSpaceFinding.title || "Project artifact"
                color: appTheme.brightForeground
                font.family: appTheme.sansFont
                font.pixelSize: 23
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                text: root.formatBytes(root.reviewedSpaceFinding.bytes) +
                      "  ·  " + Number(root.reviewedSpaceFinding.fileCount || 0)
                                    .toLocaleString(Qt.locale(), "f", 0) + " files"
                color: appTheme.muted
                font.family: appTheme.monoFont
                font.pixelSize: 10
            }
            Item { Layout.preferredHeight: 10 }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 45
                radius: appTheme.radius
                color: appTheme.alpha(appTheme.foreground, 0.045)
                border.width: 1
                border.color: appTheme.alpha(appTheme.foreground, 0.12)
                Text {
                    anchors.fill: parent
                    anchors.margins: 11
                    text: root.reviewedSpaceFinding.path || "—"
                    color: appTheme.foreground
                    font.family: appTheme.monoFont
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                }
            }
            Item { Layout.preferredHeight: 10 }
            Text {
                Layout.fillWidth: true
                text: root.reviewedSpaceFinding.consequence || "Review this path before changing it."
                color: appTheme.foreground
                font.family: appTheme.sansFont
                font.pixelSize: 12
                lineHeight: 1.32
                wrapMode: Text.WordWrap
            }
            Item { Layout.preferredHeight: 8 }
            Text {
                Layout.fillWidth: true
                text: root.reviewedSpaceFinding.safety === "regenerable"
                      ? "Butter will re-check the artifact marker and exact Home path, then remove only this folder without crossing into another mounted filesystem. No administrator access is used. This cannot be undone."
                      : "Butter will not remove this automatically because its contents may not be fully reproducible. Hand the exact evidence to your Omarchy agent or review it yourself."
                color: appTheme.muted
                font.family: appTheme.sansFont
                font.pixelSize: 11
                lineHeight: 1.3
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                Layout.topMargin: 10
                visible: artifactDialog.cleanupForSelection &&
                         homeSpace.cleanupState !== "idle"
                radius: appTheme.radius
                color: appTheme.alpha(homeSpace.cleanupState === "error"
                                      ? appTheme.urgent : appTheme.accent, 0.08)
                border.width: 1
                border.color: appTheme.alpha(homeSpace.cleanupState === "error"
                                             ? appTheme.urgent : appTheme.accent, 0.24)
                Column {
                    anchors.fill: parent
                    anchors.margins: 9
                    spacing: 1
                    Text {
                        text: homeSpace.cleanupTitle
                        color: appTheme.foreground
                        font.family: appTheme.sansFont
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                    Text {
                        width: parent.width
                        text: homeSpace.cleanupDetail
                        color: appTheme.muted
                        font.family: appTheme.sansFont
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }

            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Item { Layout.fillWidth: true }
                ChromeButton {
                    label: artifactDialog.cleanupForSelection &&
                           homeSpace.cleanupState === "removed" ? "Done" : "Cancel"
                    quiet: true
                    enabled: !(artifactDialog.cleanupForSelection &&
                               homeSpace.cleanupState === "running")
                    onTriggered: artifactDialog.close()
                }
                ChromeButton {
                    visible: root.reviewedSpaceFinding.safety === "regenerable" &&
                             !(artifactDialog.cleanupForSelection &&
                               homeSpace.cleanupState === "removed")
                    enabled: !(artifactDialog.cleanupForSelection &&
                               homeSpace.cleanupState === "running")
                    label: artifactDialog.cleanupForSelection &&
                           homeSpace.cleanupState === "running"
                           ? "Removing" : "Remove this output"
                    glyph: artifactDialog.cleanupForSelection &&
                           homeSpace.cleanupState === "running" ? "·" : "−"
                    onTriggered: homeSpace.removeArtifact(
                                     root.reviewedSpaceFinding.path,
                                     root.reviewedSpaceFinding.type)
                }
            }
        }
    }
}
