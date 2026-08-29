import QtQuick

Item {
    id: root
    property var points: []
    property int selectedIndex: 0

    implicitHeight: 86

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 17
        height: 1
        color: appTheme.alpha(appTheme.foreground, 0.16)
    }

    Row {
        anchors.fill: parent
        spacing: 0

        Repeater {
            model: root.points

            Item {
                required property var modelData
                required property int index
                width: root.width / Math.max(1, root.points.length)
                height: root.height

                Rectangle {
                    id: node
                    anchors.top: parent.top
                    anchors.topMargin: index === root.selectedIndex ? 8 : 11
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: index === root.selectedIndex ? 19 : 13
                    height: width
                    radius: width / 2
                    color: index === root.selectedIndex
                           ? appTheme.accent
                           : appTheme.blend(appTheme.darkBackground,
                                            appTheme.foreground, 0.24)
                    border.width: 2
                    border.color: index === root.selectedIndex
                                  ? appTheme.brightForeground
                                  : appTheme.alpha(appTheme.foreground, 0.42)
                }

                Column {
                    anchors.top: node.bottom
                    anchors.topMargin: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 8
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: {
                            var raw = String(modelData.timestamp || "")
                            var date = raw.length >= 10 ? new Date(raw.replace(" ", "T")) : null
                            return date && !isNaN(date.getTime())
                                   ? date.toLocaleDateString(Qt.locale(), "MMM d") : "—"
                        }
                        color: index === root.selectedIndex
                               ? appTheme.brightForeground : appTheme.foreground
                        font.family: appTheme.sansFont
                        font.pixelSize: 11
                        font.weight: index === root.selectedIndex ? Font.DemiBold : Font.Normal
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData.description || "Omarchy"
                        color: appTheme.muted
                        elide: Text.ElideRight
                        font.family: appTheme.monoFont
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
}
