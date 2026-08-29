import QtQuick

Item {
    id: root
    implicitWidth: 68
    implicitHeight: 42

    ButterGlyph {
        width: 68
        height: 50
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -2
        variant: "dish"
        lineColor: appTheme.accent
    }
}
