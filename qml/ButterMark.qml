import QtQuick

Item {
    id: root
    implicitWidth: 56
    implicitHeight: 42

    ButterGlyph {
        width: 56
        height: 41
        anchors.centerIn: parent
        variant: "dish"
        lineColor: appTheme.accent
    }
}
