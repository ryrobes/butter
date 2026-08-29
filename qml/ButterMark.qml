import QtQuick

Item {
    id: root
    implicitWidth: 42
    implicitHeight: 42

    Rectangle {
        width: 34
        height: 25
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 2
        radius: 8
        rotation: -5
        color: appTheme.blend(appTheme.accent, appTheme.foreground, 0.44)
        border.width: 1
        border.color: appTheme.alpha(appTheme.foreground, 0.46)

        Rectangle {
            width: parent.width - 7
            height: 5
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 4
            radius: 3
            color: appTheme.alpha(appTheme.foreground, 0.18)
        }

        Repeater {
            model: 3
            Rectangle {
                required property int index
                width: 2
                height: 8
                x: 10 + index * 6
                y: 11
                radius: 1
                color: appTheme.alpha(appTheme.background, 0.24)
            }
        }
    }
}

